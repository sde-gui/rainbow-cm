/* Copyright (C) 2007-2008 by Xyhthyx <xyhthyx@gmail.com>
 *
 * This file is part of Parcellite.
 *
 * Parcellite is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Parcellite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.

 Links:
 http://standards.freedesktop.org/clipboards-spec/clipboards-latest.txt
 http://standards.freedesktop.org/clipboard-extensions-spec/clipboard-extensions-latest.txt
 http://www.freedesktop.org/wiki/ClipboardManager/
 https://developer.gnome.org/gtk2/2.24/gtk2-Clipboards.html


 NOTES:
 We keep track of a delete list while the history menu is up. We add/remove items from that 
 list until we get a selection done event, then we delete those items from the real history

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "parcellite.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <ctype.h>
#include <pthread.h>

#define DEFERRED_CHECK_INTERVAL 300

#define GDK_MODIFIER_MASK_MINE (GDK_CONTROL_MASK|GDK_META_MASK|GDK_SUPER_MASK) /*\
                           GDK_MOD1_MASK|GDK_MOD2_MASK |GDK_MOD3_MASK|GDK_MOD4_MASK|GDK_MOD5_MASK|   \
                           GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK|GDK_BUTTON4_MASK|\
                           GDK_BUTTON5_MASK)|GDK_HYPER_MASK) */

static GtkClipboard * selection_primary;
static GtkClipboard * selection_clipboard;
static gchar * text_primary = NULL;
static gchar * text_clipboard = NULL;
static gchar * last_text = NULL; /**last text change, for either clipboard  */


static GtkStatusIcon *status_icon=NULL; 
GMutex *hist_lock=NULL;
static int show_icon=0;

static gchar * history_text_casefold_key = NULL;

/**defines for moving between clipboard histories  */
#define HIST_MOVE_TO_CANCEL     0
#define HIST_MOVE_TO_OK         1


typedef enum {
	CLIPBOARD_ACTION_RESET, /* clear out clipboard  */
	CLIPBOARD_ACTION_SET,   /* set clippoard content  */
	CLIPBOARD_ACTION_CHECK  /* see if there is new/lost contents */
} CLIPBOARD_ACTION;

/***************************************************************************/

typedef struct {
	guint   histno;
	guint   mouse_button;
	guint32 activate_time;
} history_menu_query_t;

/***************************************************************************/

static void schedule_deferred_clipboard_update(void);
static void disable_deferred_clipboard_update(void);

/***************************************************************************/

/**speed up pref to int lookup.  */
static int
	clipboard_management_enabled,
	ignore_whiteonly,
	track_primary_selection,
	track_clipboard_selection,
	restore_empty,
	synchronize;

static struct pref2int pref2int_map[]={
	{.val=&clipboard_management_enabled,.name="enabled"},
	{.val=&ignore_whiteonly,.name="ignore_whiteonly"},
	{.val=&track_primary_selection,.name="track_primary_selection"},
	{.val=&track_clipboard_selection,.name="track_clipboard_selection"},
	{.val=&restore_empty,.name="restore_empty"},
	{.val=&synchronize,.name="synchronize"},
	{.val=NULL,.name=NULL},		
};

/***************************************************************************/

static gboolean should_text_be_saved(gchar * text)
{
	if (!text)
		return FALSE;

	if (ignore_whiteonly)
	{
		gchar * s;
		for (s = text; NULL !=s && *s; ++s)
		{
			if (!isspace(*s))
			{
				return TRUE;
			}
		}
		return FALSE;
	}

	return TRUE;
}

/***************************************************************************/

static void save_and_set_clipboard_text(GtkClipboard * clip, gchar * text, int really_set)
{
	gchar ** p_saved_text;

	if (clip==selection_primary) {
		p_saved_text = &text_primary;
	} else{
		p_saved_text = &text_clipboard;
	}

	if (really_set)
		gtk_clipboard_set_text(clip, text ? text : "", -1);

	if (*p_saved_text != text)
	{
		if (*p_saved_text) {
			g_free(*p_saved_text);
			*p_saved_text = NULL;
		}

		if (text)
			*p_saved_text = g_strdup(text);
	}

	last_text = *p_saved_text;
}

/***************************************************************************/

static gboolean content_exists(GtkClipboard *clip)
{
	gint count;
	GdkAtom *targets;
	gboolean contents = gtk_clipboard_wait_for_targets(clip, &targets, &count);
	g_free(targets);
	return contents;
}

/***************************************************************************/

static gchar * get_clipboard_text(GtkClipboard * clip)
{
	if (gtk_clipboard_wait_is_text_available(clip))
		return(gtk_clipboard_wait_for_text(clip));
	return NULL;
}

/***************************************************************************/

static gchar * update_clipboard(GtkClipboard * clipboard, CLIPBOARD_ACTION action, gchar * text_to_set)
{
	gchar ** p_saved_text;

	if (clipboard == selection_primary) {
		p_saved_text = &text_primary;
	} else{
		p_saved_text = &text_clipboard;
	}

	/**check that our clipboards are valid and user wants to use them  */
	if ((clipboard != selection_primary && clipboard != selection_clipboard) ||
		(clipboard == selection_primary && !track_primary_selection) ||
		(clipboard == selection_clipboard && !track_clipboard_selection))
			return NULL;

	switch (action)
	{
		case CLIPBOARD_ACTION_RESET:
		{
			save_and_set_clipboard_text(clipboard, NULL, 1);
			break;
		}
		case CLIPBOARD_ACTION_SET:
		{
			if (g_strcmp0(text_to_set, *p_saved_text) != 0)
				save_and_set_clipboard_text(clipboard, text_to_set, 1);
			break;
		}
		case CLIPBOARD_ACTION_CHECK:
		{
			if (!clipboard_management_enabled)
			{
				disable_deferred_clipboard_update();
				break;
			}

			if (clipboard == selection_primary)
			{
				/* HACK: don't spam the history with useless records when text selection is in progress */
				GdkModifierType button_state;
				gdk_window_get_pointer(NULL, NULL, NULL, &button_state);
				if (button_state & (GDK_BUTTON1_MASK|GDK_SHIFT_MASK)) { /* button down, done. */
					schedule_deferred_clipboard_update();
					break;
				} else {
					disable_deferred_clipboard_update();
				}
			}

			gchar * new_text = get_clipboard_text(clipboard);
			if (new_text) {
				if (validate_utf8_text(new_text, strlen(new_text)) == 0) {
					g_free(new_text);
					break;
				}
			}

			if (!new_text) {
				if (restore_empty && !content_exists(clipboard) && *p_saved_text)
					save_and_set_clipboard_text(clipboard, *p_saved_text, 1);
				break;
			}

			if (g_strcmp0(*p_saved_text, new_text) == 0)
			{
				g_free(new_text);
				break;
			}

			if (!should_text_be_saved(new_text))
			{
				g_free(new_text);
				break;
			}

			save_and_set_clipboard_text(clipboard, new_text, 0);

			g_free(new_text);
			break;
		}
	}

	if (last_text)
		history_add_text_item(last_text, 0);

	return *p_saved_text;
}

/***************************************************************************/

static void update_clipboards(CLIPBOARD_ACTION action, gchar * text_to_set)
{
	/*g_printf("upclips\n"); */
	update_clipboard(selection_primary, action, text_to_set);
	update_clipboard(selection_clipboard, action, text_to_set);
}

/***************************************************************************/

static void check_clipboards(void)
{
	gchar * ptext = update_clipboard(selection_primary, CLIPBOARD_ACTION_CHECK, NULL);
	gchar * ctext = update_clipboard(selection_clipboard, CLIPBOARD_ACTION_CHECK, NULL);

	if (synchronize && clipboard_management_enabled) {
		if (ptext || ctext) {
			gchar * last = last_text;
			if (last && g_strcmp0(ptext, ctext) != 0) {
				last = strdup(last);
				update_clipboards(CLIPBOARD_ACTION_SET, last);
				g_free(last);
			}
		}
	}
}

/***************************************************************************/

static gboolean check_clipboards_tic(gpointer data)
{
	check_clipboards();
	return TRUE;
}

static guint deferred_clipboard_update_source_id = 0;

static void schedule_deferred_clipboard_update(void)
{
	if (!deferred_clipboard_update_source_id)
		deferred_clipboard_update_source_id = g_timeout_add(DEFERRED_CHECK_INTERVAL, check_clipboards_tic, NULL);
}

static void disable_deferred_clipboard_update(void)
{
	if (deferred_clipboard_update_source_id)
	{
		g_source_remove(deferred_clipboard_update_source_id);
		deferred_clipboard_update_source_id = 0;
	}
}

/***************************************************************************/

static void on_clipboard_owner_change(GtkClipboard * clipboard, GdkEvent * event, gpointer user_data)
{
	check_clipboards();
}

/***************************************************************************/

/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static gboolean  handle_history_item_right_click (int i, gpointer data)
{
	/* we passed the view as userdata when we connected the signal */
	struct history_info * h = (struct history_info *) data;
	switch(i){
		case HIST_MOVE_TO_CANCEL:
/*			g_print("canceled\n"); */
			break;
		case HIST_MOVE_TO_OK:
/*			g_printf("Move to"); */
			handle_marking(h,h->wi.item,h->wi.index,OPERATE_PERSIST);
			break;
	}
	/*gtk_widget_grab_focus(h->menu); */
	return TRUE;
}
/**callback wrappers for the above function  */
static gboolean  history_item_right_click_on_move (GtkWidget *menuitem, gpointer data)
{
	return handle_history_item_right_click(HIST_MOVE_TO_OK,data);
}

static gboolean history_item_right_click_on_cancel (GtkWidget *menuitem, gpointer data)
{
	return handle_history_item_right_click(HIST_MOVE_TO_CANCEL,data);
}

/***************************************************************************/
/** Fixes the right-click history being up when history is deactiviated.
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void destroy_right_click_history_cb(GtkWidget *attach_widget, GtkMenu *menu)
{
	/*g_printf("%s:\n",__func__); */
	gtk_widget_destroy	((GtkWidget *) attach_widget);
}
/***************************************************************************/
/** Display the right-click menu. h->menu contains the top-level history window
\n\b Arguments:
\n\b Returns:
if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
	view_popup_menu(treeview, event, userdata);
	h->wi.index contains the element of the item clicked on.
****************************************************************************/
static void history_item_right_click (struct history_info *h, GdkEventKey *e, gint index)
{
  GtkWidget *menu, *menuitem;
  
	struct history_item *c=NULL;
	if(NULL !=h ){
		GList* element = g_list_nth(history_list, h->wi.index);
		if(NULL !=element){
			c=(struct history_item *)(element->data);
			/*g_printf("%s ",c->text); */
		}
	} else{
		g_fprintf(stderr,"h-i-r-c: h is NULL");
		return;
	}
	
	menu = gtk_menu_new();
	gtk_menu_attach_to_widget((GtkMenu *)h->menu,menu,destroy_right_click_history_cb); /**fix  */

    if(NULL != c) {
		if(c->flags & CLIP_TYPE_PERSISTENT)
			menuitem = gtk_menu_item_new_with_label(_("Unpin"));
		else
			menuitem = gtk_menu_item_new_with_label(_("Pin"));
		g_signal_connect(menuitem, "activate",(GCallback) history_item_right_click_on_move, (gpointer)h);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}

	menuitem = gtk_menu_item_new_with_label("Cancel");
	g_signal_connect(menuitem, "activate", (GCallback) history_item_right_click_on_cancel, (gpointer)h);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	
	gtk_widget_show_all(menu);

	/* Note: event can be NULL here when called from view_onPopupMenu;
	 *  gdk_event_get_time() accepts a NULL argument */
	gtk_menu_popup(GTK_MENU(menu), h->menu, NULL, NULL, NULL,
		0,
		/*(e != NULL) ? ((GdkEventButton *)e)->button : 0, */
		gdk_event_get_time((GdkEvent*)e));
	/*gtk_widget_grab_focus(menu);  */
}

/* Called when Clear is selected from history menu */
static void clear_selected(GtkMenuItem *menu_item, gpointer user_data)
{
	int do_clear = 1;
	GtkWidget * confirm_dialog = gtk_message_dialog_new(
		NULL,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_OK_CANCEL,
		_("Clear the history?"));

	gtk_window_set_title((GtkWindow *) confirm_dialog, "Parcellite");

    if (gtk_dialog_run((GtkDialog *) confirm_dialog) != GTK_RESPONSE_OK) {
		do_clear = 0;
	}
	gtk_widget_destroy(confirm_dialog);

	if (do_clear) {
		struct history_info * h = (struct history_info *) user_data;
		/* Clear history and free history-related variables */
		remove_deleted_items(h); /**fix bug 92, Shift/ctrl right-click followed by clear segfaults/double free.  */	
		clear_history();
		/*g_printf("Clear hist done, h=%p, h->delete_list=%p\n",h, h->delete_list); */
		update_clipboards(CLIPBOARD_ACTION_RESET, "");
	}
}

static void on_enabled_toggled(GtkCheckMenuItem * menu_item, gpointer user_data)
{
	set_pref_int32("enabled", gtk_check_menu_item_get_active(menu_item));
}

/* Called when About is selected from right-click menu */
static void show_about_dialog(GtkMenuItem *menu_item, gpointer user_data)
{
  /* This helps prevent multiple instances */
  if (!gtk_grab_get_current())
  {
    const gchar* authors[] = {_("Gilberto \"Xyhthyx\" Miralla <xyhthyx@gmail.com>\nDoug Springer <gpib@rickyrockrat.net>"), NULL};
    const gchar* license =
      "This program is free software; you can redistribute it and/or modify\n"
      "it under the terms of the GNU General Public License as published by\n"
      "the Free Software Foundation; either version 3 of the License, or\n"
      "(at your option) any later version.\n\n"
      "This program is distributed in the hope that it will be useful,\n"
      "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
      "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
      "GNU General Public License for more details.\n\n"
      "You should have received a copy of the GNU General Public License\n"
      "along with this program.  If not, see <http://www.gnu.org/licenses/>.";
    
    /* Create the about dialog */
    GtkWidget* about_dialog = gtk_about_dialog_new();
    gtk_window_set_icon((GtkWindow*)about_dialog,
                        gtk_widget_render_icon(about_dialog, GTK_STOCK_ABOUT, -1, NULL));
    
    gtk_about_dialog_set_name((GtkAboutDialog*)about_dialog, "Parcellite");
    #ifdef HAVE_CONFIG_H	/**VER=555; sed "s#\(.*\)svn.*\".*#\1svn$VER\"#" config.h  */
    gtk_about_dialog_set_version((GtkAboutDialog*)about_dialog, VERSION);
    #endif
    gtk_about_dialog_set_comments((GtkAboutDialog*)about_dialog,
                                _("Lightweight GTK+ clipboard manager."));
    
/*    gtk_about_dialog_set_website((GtkAboutDialog*)about_dialog,
                                 "http://parcellite.sourceforge.net");*/
    
    gtk_about_dialog_set_copyright((GtkAboutDialog*)about_dialog, _("Copyright (C) 2015 Vadim Ushakov\nCopyright (C) 2007, 2008 Gilberto \"Xyhthyx\" Miralla\nCopyright (C) 2010-2013 Doug Springer"));
    gtk_about_dialog_set_authors((GtkAboutDialog*)about_dialog, authors);
    gtk_about_dialog_set_translator_credits ((GtkAboutDialog*)about_dialog,
                                             "Miloš Koutný <milos.koutny@gmail.com>\n"
                                             "Kim Jensen <reklamepost@energimail.dk>\n"
                                             "Eckhard M. Jäger <bart@neeneenee.de>\n"
                                             "Michael Stempin <mstempin@web.de>\n"
                                             "Benjamin Danon <benjamin@sphax3d.org>\n" 
                                             "Németh Tamás <ntomasz@vipmail.hu>\n"
                                             "Davide Truffa <davide@catoblepa.org>\n"
                                             "Jiro Kawada <jiro.kawada@gmail.com>\n"
                                             "Øyvind Sæther <oyvinds@everdot.org>\n"
                                             "pankamyk <pankamyk@o2.pl>\n"
                                             "Tomasz Rusek <tomek.rusek@gmail.com>\n"
                                             "Phantom X <megaphantomx@bol.com.br>\n"
                                             "Ovidiu D. Niţan <ov1d1u@sblug.ro>\n"
                                             "Alexander Kazancev <kazancas@mandriva.ru>\n"
                                             "Daniel Nylander <po@danielnylander.se>\n"
                                             "Hedef Türkçe <iletisim@hedefturkce.com>\n"
                                             "Lyman Li <lymanrb@gmail.com>\n"
                                             "Gilberto \"Xyhthyx\" Miralla <xyhthyx@gmail.com>");
    
    gtk_about_dialog_set_license((GtkAboutDialog*)about_dialog, license);
	  gtk_about_dialog_set_logo_icon_name((GtkAboutDialog*)about_dialog, APP_ICON);
    /* Run the about dialog */
    gtk_dialog_run((GtkDialog*)about_dialog);
    gtk_widget_destroy(about_dialog);
  }
}

/* Called when Preferences is selected from right-click menu */
static void preferences_selected(GtkMenuItem *menu_item, gpointer user_data)
{
  /* This helps prevent multiple instances */
  if (!gtk_grab_get_current()){
		 /* Show the preferences dialog */
    show_preferences(0);
	}

}

/* Called when Quit is selected from right-click menu */
static void quit_selected(GtkMenuItem *menu_item, gpointer user_data)
{
  /* Prevent quit with dialogs open */
  if (!gtk_grab_get_current())
    /* Quit the program */
    gtk_main_quit();
}

/***************************************************************************/
/** Called just before destroying history menu.
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static gboolean selection_done(GtkMenuShell *menushell, gpointer user_data) 
{
	struct history_info * h = (struct history_info *) user_data;
	if (h && h->delete_list) {
		remove_deleted_items(h);
		goto done;
	}

	/*g_print("selection_active=%d\n",selection_active); */
	/*g_print("Got selection_done\n"); */
	if (h && h->change_flag && get_pref_int32("save_history")) {
		save_history();
		h->change_flag = 0;
	}

done:
	/*gtk_widget_destroy((GtkWidget *)menushell); - fixes annoying GTK_IS_WIDGET/GTK_IS_WINDOW
	  warnings from GTK when history dialog is destroyed. */
	return FALSE;
}

/***************************************************************************/

static void on_history_menu_position(GtkMenu * menu, gint * px, gint * py, gboolean * push_in, gpointer user_data)
{
	gint x = 0, y = 0;
	Display * display = gdk_x11_get_default_xdisplay();
	Window focus;
	int revert_to;
	XGetInputFocus(display, &focus, &revert_to);

	if (focus != None && focus != PointerRoot) {
		int dest_x, dest_y;
		Window child_return;
		if (XTranslateCoordinates(display,
			focus, XDefaultRootWindow(display),
			0, 0,
			&dest_x, &dest_y,
			&child_return))
		{
			x = dest_x;
			y = dest_y;
		}
	}

	*px = x;
	*py = y;
}

/***************************************************************************/

static void apply_search_string_cb(GtkWidget * widget, gpointer user_data)
{
	struct history_info * h = (struct history_info *) user_data;
	GtkMenuItem * menu_item = GTK_MENU_ITEM(widget);
	if (!h || !menu_item)
		return;

	const gchar * history_text_casefold = (const gchar *) g_object_get_data(
		(GObject *) menu_item, history_text_casefold_key);
	if (!history_text_casefold)
		return;

	gboolean match = h->search_string_casefold[0] == 0 ||
		g_strstr_len(history_text_casefold, -1, h->search_string_casefold) != NULL;

	//gtk_widget_set_sensitive(widget, match);
	gtk_widget_set_visible(widget, match);

	if (match && !h->first_matched)
	{
		h->first_matched = widget;
		gtk_menu_shell_select_item((GtkMenuShell *) h->menu, widget);
	}
}

/***************************************************************************/

static void apply_search_string(struct history_info * h)
{
	g_free(h->search_string_casefold);
	h->search_string_casefold = g_utf8_casefold(h->search_string->str, -1);
	h->first_matched = NULL;
	gtk_container_foreach((GtkContainer *) h->menu, apply_search_string_cb, h);
}

/***************************************************************************/

static void on_history_menu_im_context_commit(GtkIMContext * context, gchar * str, gpointer user_data)
{
	struct history_info * h = (struct history_info *) user_data;

	if (!h || !h->search_string)
		return;

	if (str) {
		g_string_append(h->search_string, str);
	} else {
		glong l = h->search_string->len;
		glong truncate_at = 0;
		if (l > 1) {
			gchar * prev_char = g_utf8_find_prev_char(h->search_string->str, h->search_string->str + l);
			if (!prev_char)
				prev_char = h->search_string->str + l - 1;
			truncate_at = prev_char - h->search_string->str;
		}
		g_string_truncate(h->search_string, truncate_at);
	}

	//g_fprintf(stderr, "%s\n", h->search_string->str);

	apply_search_string(h);
}

/***************************************************************************/

static gboolean key_release_cb (GtkWidget *w,GdkEventKey *e, gpointer user)
{
	struct history_info * h = (struct history_info *) user;

	if (!e)
		return FALSE;

	if (e->type == GDK_KEY_PRESS || e->type == GDK_KEY_RELEASE) {
		GdkEventKey * ke = (GdkEventKey *) e;
		if (h->im_context && h->search_string && get_pref_int32("type_search")) {
			gboolean filtered = gtk_im_context_filter_keypress(h->im_context, ke);
			if (filtered)
				return TRUE;
			if (e->type == GDK_KEY_PRESS && ke->keyval == GDK_KEY_BackSpace && ke->state & GDK_CONTROL_MASK) {
				g_string_truncate(h->search_string, 0);
				apply_search_string(h);
				return TRUE;
			}
			else if (e->type == GDK_KEY_PRESS && ke->keyval == GDK_KEY_BackSpace) {
				on_history_menu_im_context_commit(NULL, NULL, h);
				return TRUE;
			}
		}
	}

	return FALSE;
}

/***************************************************************************/
/** Set clipboard from history list.
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void set_clipboard_text_from_item(struct history_info *h, GList *element)
{
	gchar *txt=NULL;
	if(NULL == find_h_item(h->delete_list,NULL,element)){	/**not in our delete list  */
		/**make a copy of txt, because it gets freed and re-allocated.  */
		txt=p_strdup(((struct history_item *)(element->data))->text);
		update_clipboards(CLIPBOARD_ACTION_SET, txt);
	}
	g_signal_emit_by_name ((gpointer)h->menu,"selection-done");
	
	if(NULL != txt)
		g_free(txt);
}


/***************************************************************************/
/** This handles the events for each item in the history menu.
\n\b Arguments:	 user is element number
\n\b Returns:
****************************************************************************/
static gboolean my_item_event (GtkWidget *w,GdkEventKey *e, gpointer user)
{
	static struct history_info *h=NULL;
	
	if(NULL==w && NULL==e){
		h=(struct history_info *)user;
	  /*g_print("my_item_event: Set menu to %p\n",h); */
		return FALSE;
	}
	if(NULL == h)
		return FALSE;	
	/**check for enter  */
	if(GDK_MOTION_NOTIFY == e->type)
		return FALSE;
	/*printf("my_item_event: T 0x%x S 0x%x ",e->type,e->state); */
	if(NULL !=h && GDK_ENTER_NOTIFY ==e->type ){/**add to delete   */
		GdkEventCrossing *enter=(GdkEventCrossing *)e;
		/*printf("state 0x%x\n",enter->state); */
		/**use shift and right-click  */
		if(GDK_SHIFT_MASK&enter->state && GDK_BUTTON3_MASK&enter->state)
			handle_marking(h,w,GPOINTER_TO_INT(user),OPERATE_DELETE);
	}
	if(GDK_KEY_PRESS == e->type){
		/*GdkEventKey *k=	(GdkEventKey *)e; */
		printf("key press %d (0x%x)\n",e->keyval,e->keyval); 
		fflush(NULL);
	}
	if(GDK_BUTTON_RELEASE==e->type){
		GdkEventButton *b=(GdkEventButton *)e;
		GList* element = g_list_nth(history_list, GPOINTER_TO_INT(user));
		/*printf("type %x State 0x%x val %x %p '%s'\n",e->type, b->state,b->button,w,(gchar *)((struct history_item *(element->data))->text));  */
		if(3 == b->button){ /**right-click  */
			if(GDK_CONTROL_MASK&b->state){
				handle_marking(h,w,GPOINTER_TO_INT(user),OPERATE_DELETE);
			}else{ /**shift-right click release  */
				 if((GDK_CONTROL_MASK|GDK_SHIFT_MASK)&b->state)
					return FALSE;
				/*g_print("Calling popup\n");  */
	      h->wi.event=e;
	      h->wi.item=w;
				h->wi.index=GPOINTER_TO_INT(user);
				/*g_fprintf(stderr,"Calling hist_itemRclk\n"); */
		    history_item_right_click(h,e,GPOINTER_TO_INT(user));
				
			}
			return TRUE;
		}else if( 1 == b->button){
		  /* Get the text from the right element and set as clipboard */
			set_clipboard_text_from_item(h,element);
		}	
		fflush(NULL);
	}
	
	return FALSE;
}

/***************************************************************************/
/** Attempt to handle enter key behaviour.
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void item_selected(GtkMenuItem *menu_item, gpointer user_data)
{
	static struct history_info *h=NULL;
	if(NULL ==menu_item){
		h=(struct history_info *)user_data;
		return;
	}
		
		
	GdkEventKey *k=(GdkEventKey *)gtk_get_current_event();
	GList* element = g_list_nth(history_list, GPOINTER_TO_INT(user_data));
	/*g_print ("item_selected '%s' type %x val %x\n",(gchar *)((struct history_item *(element->data))->text),k->type, k->keyval);  */
	if(0xFF0d == k->keyval && GDK_KEY_PRESS == k->type){
		set_clipboard_text_from_item(h,element);
	}
}	

/***************************************************************************/
/** Write the elements to the menu.
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void write_history_menu_items(GList *list, GtkWidget *menu)
{
	GList *element;
	if(NULL == list)
		return;
	for (element = list; element != NULL; element = element->next) 
			gtk_menu_shell_append((GtkMenuShell*)menu,element->data);	
}

/***************************************************************************/
/** Replace non-printing characters with ??.
0x09->E28692 - \2192
0x0a->E2818B - \204b
space-E290A3 - \u2423 
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static GString* convert_string(GString* s)
{
	gchar arrow[4]={0xe2,0x86,0x92,0x00};/**0xe28692 (UTF-8 right-arrow) \\  */
	gchar pharagraph[4]={0xe2,0x81,0x8b,0x00}; /**utf-8 pharagraph symbol \2192 */
	gchar square_u[4]={0xe2,0x90,0xA3,0};	 /**square-u \\2423  */
	gchar *p, *r;
	
	for (p=s->str; p!= NULL; p=g_utf8_find_next_char(p,s->str+s->len)){
		switch(*p){
			case 0x09:r=arrow; break;
			case 0x0a:r=pharagraph; break;
			case 0x20:r=square_u; break;
		  default:r=NULL; break; 
		}
		if(NULL !=r ) {/**replace. */
			gint32 pos=(p-s->str)+1;
			*p=r[0];
			s=g_string_insert(s,pos,&r[1]);
			p=s->str+pos+2;
		}
	}
	return s;
}

/***************************************************************************/
/** .
\n\b Arguments:
u is history.
\n\b Returns:
****************************************************************************/
static void destroy_history_menu(GtkMenuShell *menu, gpointer u)
{
	/*g_printf("%s:\n",__func__); */
	selection_done(menu,u);	/**allow deleted items to be deleted.  */
	gtk_widget_destroy((GtkWidget *)menu);
}
/***************************************************************************/
/**  Called when status icon is left-clicked or action key hit.
\n\b Arguments:
\n\b Returns:
****************************************************************************/

static gboolean do_show_history_menu(gpointer data)
{
	history_menu_query_t * query = (history_menu_query_t *) data;
	if (!query)
		goto end;

	GtkWidget * menu, * menu_item, * item_label;

	static struct history_info h;
	h.histno = query->histno;
	h.change_flag = 0;
	h.element_text = NULL;
	h.wi.index = -1;

	if (!h.search_string)
		h.search_string = g_string_new("");
	else
		g_string_truncate(h.search_string, 0);

	if (!h.im_context) {
		h.im_context = gtk_im_multicontext_new();
		gtk_im_context_set_use_preedit(h.im_context, FALSE);
		g_signal_connect((GObject *) h.im_context, "commit",
			(GCallback) on_history_menu_im_context_commit, (gpointer)&h);
	}
	gtk_im_context_reset(h.im_context);


	GList * element, * persistent = NULL;
	GList * lhist = NULL;

	/* Create the menu */
	menu = gtk_menu_new();

	h.menu = menu;
	h.clip_item = NULL;
	h.delete_list = NULL;
	h.persist_list = NULL;

	my_item_event(NULL,NULL,(gpointer)&h); /**init our function  */
	item_selected(NULL,(gpointer)&h);	/**ditto  */
	gtk_menu_shell_set_take_focus((GtkMenuShell *)menu,TRUE); /**grab keyboard focus  */
	/*g_signal_connect((GObject*)menu, "selection-done", (GCallback)selection_done, gtk_menu_get_attach_widget (menu));  */
	g_signal_connect((GObject*)menu, "cancel", (GCallback)selection_done, &h); 
	g_signal_connect((GObject*)menu, "selection-done", (GCallback)selection_done, &h); 
	/*g_signal_connect((GObject*)menu, "selection-done", (GCallback)gtk_widget_destroy, NULL); */
	/**Trap key events  */
	g_signal_connect((GObject*)menu, "event", (GCallback)key_release_cb, (gpointer)&h);

	/* -------------------- */
	/*gtk_menu_shell_append((GtkMenuShell*)menu, gtk_separator_menu_item_new()); */
	/* Items */
	if ((history_list != NULL) && (history_list->data != NULL)) {
		/* Declare some variables */
		gint32 item_length = get_pref_int32("item_length");
		gint32 ellipsize = get_pref_int32("ellipsize");
		gint32 nonprint_disp = get_pref_int32("nonprint_disp");
		gint element_number = 0;
		gchar * primary_temp = gtk_clipboard_wait_for_text(selection_primary);
		gchar * clipboard_temp = gtk_clipboard_wait_for_text(selection_clipboard);

		/* Go through each element and adding each */
		for (element = history_list; element != NULL; element = element->next) {
			struct history_item *c=(struct history_item *)(element->data);
			gchar* hist_text=c->text;
			if(!(HIST_DISPLAY_PERSISTENT&h.histno) && (c->flags & CLIP_TYPE_PERSISTENT))
				goto next_loop;
			else if( !(HIST_DISPLAY_NORMAL&h.histno) && !(c->flags & CLIP_TYPE_PERSISTENT))
				goto next_loop;
			GString* string = g_string_new(hist_text);
			if(nonprint_disp)
				string=convert_string(string);
			glong len=g_utf8_strlen(string->str, string->len);
			gchar * tooltip = NULL;
			/* Ellipsize text */
			if (len > item_length) {
				if (len > item_length * 4) {
					tooltip = g_strndup(string->str,
						g_utf8_offset_to_pointer(string->str, len - item_length * 4) - string->str);
				} else {
					tooltip = g_strdup(string->str);
				}
				switch (ellipsize) {
					case PANGO_ELLIPSIZE_START:
						string = g_string_erase(string, 0, g_utf8_offset_to_pointer(string->str, len - item_length) - string->str);
						/*string = g_string_erase(string, 0, string->len-(get_pref_int32("item_length"))); */
						string = g_string_prepend(string, "...");
						break;
					case PANGO_ELLIPSIZE_MIDDLE:
					{
						gchar* p1 = g_utf8_offset_to_pointer(string->str, item_length / 2);
						gchar* p2 = g_utf8_offset_to_pointer(string->str, len - item_length / 2);
						string = g_string_erase(string, p1 - string->str, p2 - p1);
						string = g_string_insert(string, p1 - string->str, "...");
						/** string = g_string_erase(string, (get_pref_int32("item_length")/2), string->len-(get_pref_int32("item_length")));
						string = g_string_insert(string, (string->len/2), "...");*/	
						break;
					}
					case PANGO_ELLIPSIZE_END:
						string = g_string_truncate(string, g_utf8_offset_to_pointer(string->str, item_length) - string->str);
						/*string = g_string_truncate(string, get_pref_int32("item_length")); */
						string = g_string_append(string, "...");
						break;
				}
			}
			/* Remove control characters */
			gsize i = 0;
			while (i < string->len)
			{	 /**fix 100% CPU utilization for odd data. - bug 2976890   */
				gsize nline=0;
				while(string->str[i+nline] == '\n' && nline+i<string->len)
					nline++;
				if(nline){
					g_string_erase(string, i, nline);
					/* RMME printf("e %ld",nline);fflush(NULL); */
				}
				else
					i++;
			}

			/* Make new item with ellipsized text */
			menu_item = gtk_menu_item_new_with_label(string->str);
			g_signal_connect((GObject*)menu_item, "event",
				(GCallback)my_item_event, GINT_TO_POINTER(element_number));
			g_signal_connect((GObject*)menu_item, "activate",
				(GCallback)item_selected, GINT_TO_POINTER(element_number));

			g_object_set_data_full((GObject *) menu_item, history_text_casefold_key,
				g_utf8_casefold(hist_text, -1), g_free);

			if (tooltip) {
				gtk_widget_set_tooltip_text(menu_item, tooltip);
				g_free(tooltip);
			}

			/* Modify menu item label properties */
			item_label = gtk_bin_get_child((GtkBin*)menu_item);
			gtk_label_set_single_line_mode((GtkLabel*)item_label, TRUE);

			/* Check if item is also clipboard text and make bold */
			if ((clipboard_temp) && (g_strcmp0(hist_text, clipboard_temp) == 0))
			{
				gchar* bold_text = g_markup_printf_escaped("<b>%s</b>", string->str);
				if( NULL == bold_text) g_fprintf(stderr,"NulBMKUp:'%s'\n",string->str);
				gtk_label_set_markup((GtkLabel*)item_label, bold_text);
				g_free(bold_text);
				h.clip_item=menu_item;
				h.element_text=hist_text;
				h.wi.index=element_number;
			}
			else if ((primary_temp) && (g_strcmp0(hist_text, primary_temp) == 0))
			{
				gchar* italic_text = g_markup_printf_escaped("<i>%s</i>", string->str);
				if( NULL == italic_text) g_fprintf(stderr,"NulIMKUp:'%s'\n",string->str);
				gtk_label_set_markup((GtkLabel*)item_label, italic_text);
				g_free(italic_text);
				h.clip_item=menu_item;
				h.element_text=hist_text;
				h.wi.index=element_number;
			}
			if(c->flags &CLIP_TYPE_PERSISTENT){
				persistent = g_list_prepend(persistent, menu_item);
				/*g_printf("persistent %s\n",c->text); */
			}	else{
				/* Append item */
				lhist = g_list_prepend(lhist, menu_item);
			}

			/* Prepare for next item */
			g_string_free(string, TRUE);
next_loop:
			element_number++;
		}	/**end of for loop for each history item  */
		/* Cleanup */
		g_free(primary_temp);
		g_free(clipboard_temp);
	}
	else
	{
		/* Nothing in history so adding empty */
		menu_item = gtk_menu_item_new_with_label(_("Empty"));
		gtk_widget_set_sensitive(menu_item, FALSE);
		gtk_menu_shell_append((GtkMenuShell*)menu, menu_item);
	}

	{
		lhist = g_list_reverse(lhist);
		persistent = g_list_reverse(persistent);
	}	

	/**now actually add them from the list  */
    {
		write_history_menu_items(lhist,menu);
		gtk_menu_shell_append((GtkMenuShell*)menu, gtk_separator_menu_item_new()); 
		write_history_menu_items(persistent,menu);
	}

	g_list_free(lhist);
	g_list_free(persistent);
	/*g_signal_connect(menu,"deactivate",(GCallback)destroy_history_menu,(gpointer)&h); */
	g_signal_connect(menu,"selection-done",(GCallback)destroy_history_menu,(gpointer)&h);
	/* Popup the menu... */
	gtk_widget_show_all(menu);
	gtk_menu_popup((GtkMenu*)menu, NULL, NULL,
		query->mouse_button ? NULL : on_history_menu_position,
		NULL,
		query->mouse_button,
		query->activate_time);
	/**set last entry at first -fixes bug 2974614 */
	gtk_menu_shell_select_first((GtkMenuShell*)menu, TRUE);

end:
	g_free(query);

	/* Return FALSE so the g_timeout_add() function is called only once */
	return FALSE;
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static guint figure_histories(void)
{
	return HIST_DISPLAY_PERSISTENT | HIST_DISPLAY_NORMAL;
}

/***************************************************************************/

static void show_history_menu(guint histno, guint mouse_button, guint32 activate_time)
{
	if (activate_time == GDK_CURRENT_TIME)
		activate_time = gtk_get_current_event_time();

	history_menu_query_t * query = g_try_new(history_menu_query_t, 1);
	if (!query)
		return;

	query->histno = histno;
	query->mouse_button = mouse_button;
	query->activate_time = activate_time;

	g_timeout_add(POPUP_DELAY, do_show_history_menu, query);
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static GtkWidget * create_main_menu(void)
{
	GtkWidget * menu = gtk_menu_new();

	/* About */
	{
		GtkWidget * menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
		g_signal_connect((GObject*)menu_item, "activate", (GCallback)show_about_dialog, NULL);
		gtk_menu_shell_append((GtkMenuShell*)menu, menu_item);
	}

	/* Save History */
	{
		GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Save History as..."));
		GtkWidget * menu_image = gtk_image_new_from_stock(GTK_STOCK_SAVE_AS, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image((GtkImageMenuItem*)menu_item, menu_image);
		g_signal_connect((GObject*)menu_item, "activate", (GCallback)history_save_as, NULL);
		gtk_widget_set_tooltip_text(menu_item,
			_("Save History as a text file. "
			  "Prepends xHIST_0000 to each entry. x is either P(persistent) or N (normal)"));
		gtk_menu_shell_append((GtkMenuShell*)menu, menu_item);
	}

	/* Clear History */
	{
		GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Clear History"));
		GtkWidget * menu_image = gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image((GtkImageMenuItem*)menu_item, menu_image);
		g_signal_connect((GObject*)menu_item, "activate", (GCallback)clear_selected, NULL);
		gtk_menu_shell_append((GtkMenuShell*)menu, menu_item);
	}

	/* Enabled */
	{
		GtkWidget * menu_item = gtk_check_menu_item_new_with_mnemonic(_("_Enabled"));
		gtk_check_menu_item_set_active((GtkCheckMenuItem *) menu_item, clipboard_management_enabled);
		g_signal_connect((GObject*)menu_item, "toggled", (GCallback)on_enabled_toggled, NULL);
		gtk_menu_shell_append((GtkMenuShell*)menu, menu_item);
	}

	/* Preferences */
	{
		GtkWidget * menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
		g_signal_connect((GObject*)menu_item, "activate", (GCallback)preferences_selected, NULL);
		gtk_menu_shell_append((GtkMenuShell*)menu, menu_item);
	}
	
	gtk_menu_shell_append((GtkMenuShell*)menu, gtk_separator_menu_item_new());

	/* Quit */
	{
		GtkWidget * menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
		g_signal_connect((GObject*)menu_item, "activate", (GCallback)quit_selected, NULL);
		gtk_menu_shell_append((GtkMenuShell*)menu, menu_item);
	}

	gtk_widget_show_all(menu);
	return menu;
}

/* Called when status icon is right-clicked */
static void  show_main_menu(GtkStatusIcon *status_icon, guint button, guint activate_time,  gpointer data)
{
	GtkWidget * menu = create_main_menu();
	gtk_menu_popup((GtkMenu*)menu, NULL, NULL, NULL, NULL, button, activate_time);	
}


/* Called when status icon is left-clicked */
static void status_icon_clicked(GtkStatusIcon *status_icon, gpointer user_data)
{
  show_history_menu(figure_histories(), 1, GDK_CURRENT_TIME);
}
/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void setup_icon( void )
{
	if(NULL == status_icon){
		status_icon = gtk_status_icon_new_from_icon_name(APP_ICON);
		gtk_status_icon_set_tooltip((GtkStatusIcon*)status_icon, _("Clipboard Manager"));
		g_signal_connect((GObject*)status_icon, "activate", (GCallback)status_icon_clicked, NULL);
		g_signal_connect((GObject*)status_icon, "popup-menu", (GCallback)show_main_menu, NULL);	
	}	else {
		gtk_status_icon_set_from_icon_name(status_icon,APP_ICON);
	}
}

/* Called when history global hotkey is pressed */
void history_hotkey(char *keystring, gpointer user_data)
{
  show_history_menu(figure_histories(), 0, GDK_CURRENT_TIME);
}

/* Called when actions global hotkey is pressed */
void menu_hotkey(char *keystring, gpointer user_data)
{
  show_main_menu(status_icon, 0, 0, NULL);
}


/* Startup calls and initializations */
static void parcellite_init(void)
{
	int i;

	/* Create clipboard */
	selection_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	selection_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	/**check to see if optional helpers exist.  */
	if(FALSE ==g_thread_supported()){
		g_fprintf(stderr,"g_thread not init!\n");
	}
	hist_lock= g_mutex_new();
  
  show_icon=!get_pref_int32("no_icon");
  /* Read history */
  if (get_pref_int32("save_history")){
		gchar *x;
		/*g_printf("Calling read_hist\n"); */
		read_history();
		if(NULL != history_list){
			struct history_item *c;
			c=(struct history_item *)(history_list->data);	
			if (NULL == (x=get_clipboard_text(selection_primary)))
				update_clipboard(selection_primary, CLIPBOARD_ACTION_SET, c->text);
			else
				g_free (x);
			if (NULL == (x=get_clipboard_text(selection_clipboard)))
				update_clipboard(selection_clipboard, CLIPBOARD_ACTION_SET, c->text);
			else
				g_free(x);
		}
	}

	g_signal_connect(selection_primary, "owner-change", (GCallback) on_clipboard_owner_change, NULL);
	g_signal_connect(selection_clipboard, "owner-change", (GCallback) on_clipboard_owner_change, NULL);

  /* Bind global keys */
  keybinder_init();
	for (i=0;NULL != keylist[i].name; ++i)
		bind_itemkey(keylist[i].name,keylist[i].keyfunc);
  
  /* Create status icon */
  if (show_icon)
  {
		setup_icon();
  }
}

/***************************************************************************/
	/** .
	\n\b Arguments:
	\n\b Returns:
	****************************************************************************/
int main(int argc, char *argv[])
{
	struct cmdline_opts *opts;
	
	bindtextdomain(GETTEXT_PACKAGE, PARCELLITELOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	/* Initiate GTK+ */
	gtk_init(&argc, &argv);

	history_text_casefold_key = g_strdup_printf("Parcellite history_text_casefold_key %x%x",
		(unsigned) g_random_int(), (unsigned) g_random_int());

	/**this just maps to the static struct, prefs do not need to be loaded  */
	pref_mapper(pref2int_map,PM_INIT); 
	check_dirs(); /**make sure we set up default RC if it doesn't exist.  */

	read_preferences();
#ifdef	DEBUG_UPDATE
	if(get_pref_int32("debug_update")) debug_update=1;
#endif
  /* Parse options */
	opts=parse_options(argc, argv);
  if(NULL == opts)
   	return 1;
	if(opts->exit)
		return 0;

  /* Init Parcellite */
  parcellite_init();
  /*g_printf("Start main loop\n"); */
  /* Run GTK+ loop */
  gtk_main();

  /* Unbind keys */
  keybinder_unbind(get_pref_string("history_key"), history_hotkey);
  keybinder_unbind(get_pref_string("menu_key"), menu_hotkey);
  /* Cleanup */
	/**  
  g_free(prefs.history_key);
  g_free(prefs.menu_key);
	*/
  g_list_free(history_list);
  /* Exit */
  return 0;
}
