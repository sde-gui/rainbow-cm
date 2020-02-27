/*
 * Rainbow CM
 * 
 * Copyright (C) 2015-2020 Vadim Ushakov
 * Copyright (C) 2007-2008 by Xyhthyx <xyhthyx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rainbow-cm.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <ctype.h>

/**defines for moving between clipboard histories  */
#define HIST_MOVE_TO_CANCEL     0
#define HIST_MOVE_TO_OK         1

typedef struct {
	guint   mouse_button;
	guint32 activate_time;
} history_menu_query_t;

/******************************************************************************/

static gchar * history_text_casefold_key_ = NULL;

gchar * get_history_text_casefold_key(void)
{
	if (!history_text_casefold_key_)
	{
		history_text_casefold_key_ = g_strdup_printf(
			"%s history_text_casefold_key %x-%x",
			APP_PROG_NAME,
			(unsigned) g_random_int(),
			(unsigned) g_random_int());
	}
	return history_text_casefold_key_;
}
/******************************************************************************/

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

/******************************************************************************/

static void destroy_right_click_history_cb(GtkWidget *attach_widget, GtkMenu *menu)
{
	/*g_printf("%s:\n",__func__); */
	gtk_widget_destroy	((GtkWidget *) attach_widget);
}

/******************************************************************************/

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

/******************************************************************************/

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

/******************************************************************************/

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

/******************************************************************************/

static void apply_search_string_cb(GtkWidget * widget, gpointer user_data)
{
	struct history_info * h = (struct history_info *) user_data;
	GtkMenuItem * menu_item = GTK_MENU_ITEM(widget);
	if (!h || !menu_item)
		return;

	const gchar * history_text_casefold = (const gchar *) g_object_get_data(
		(GObject *) menu_item, get_history_text_casefold_key());
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

/******************************************************************************/

static void apply_search_string(struct history_info * h)
{
	g_free(h->search_string_casefold);
	h->search_string_casefold = g_utf8_casefold(h->search_string->str, -1);
	h->first_matched = NULL;
	gtk_container_foreach((GtkContainer *) h->menu, apply_search_string_cb, h);
}

/******************************************************************************/

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

/******************************************************************************/

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

/******************************************************************************/

static void set_clipboard_text_from_item(struct history_info *h, GList *element)
{
	gchar *txt=NULL;
	if(NULL == find_h_item(h->delete_list,NULL,element)){	/**not in our delete list  */
		/**make a copy of txt, because it gets freed and re-allocated.  */
		txt=g_strdup(((struct history_item *)(element->data))->text);
		update_clipboards(CLIPBOARD_ACTION_SET, txt);
	}
	g_signal_emit_by_name ((gpointer)h->menu,"selection-done");
	
	if(NULL != txt)
		g_free(txt);
}


/******************************************************************************/

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

/******************************************************************************/

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

/******************************************************************************/

static void write_history_menu_items(GList *list, GtkWidget *menu)
{
	GList *element;
	if(NULL == list)
		return;
	for (element = list; element != NULL; element = element->next) 
			gtk_menu_shell_append((GtkMenuShell*)menu,element->data);	
}

/******************************************************************************/

static GString* convert_nonprinting_characters(GString* s)
{
	gchar arrow[4]={0xe2,0x86,0x92,0x00};
	gchar paragraph[4]={0xe2,0x81,0x8b,0x00};
	gchar square_u[4]={0xe2,0x90,0xA3,0};
	gchar *p, *r;
	
	for (p=s->str; p!= NULL; p=g_utf8_find_next_char(p,s->str+s->len)){
		switch(*p){
			case 0x09:r=arrow; break;
			case 0x0a:r=paragraph; break;
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

/******************************************************************************/

static void destroy_history_menu(GtkMenuShell *menu, gpointer u)
{
	/*g_printf("%s:\n",__func__); */
	selection_done(menu,u);	/**allow deleted items to be deleted.  */
	gtk_widget_destroy((GtkWidget *)menu);
}

/******************************************************************************/

static gboolean do_show_history_menu(gpointer data)
{
	history_menu_query_t * query = (history_menu_query_t *) data;
	if (!query)
		goto end;

	GtkWidget * menu, * menu_item, * item_label;

	static struct history_info h;
	h.change_flag = 0;
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
		gint32 display_nonprinting_characters = get_pref_int32("display_nonprinting_characters");
		gint element_number = 0;
		gchar * primary_temp = gtk_clipboard_wait_for_text(selection_primary);
		gchar * clipboard_temp = gtk_clipboard_wait_for_text(selection_clipboard);

		/* Go through each element and adding each */
		for (element = history_list; element != NULL; element = element->next) {
			struct history_item *c=(struct history_item *)(element->data);
			gchar* hist_text=c->text;
			GString* string = g_string_new(hist_text);
			if (display_nonprinting_characters)
				string = convert_nonprinting_characters(string);
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

			g_object_set_data_full((GObject *) menu_item, get_history_text_casefold_key(),
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
				h.wi.index=element_number;
			}
			else if ((primary_temp) && (g_strcmp0(hist_text, primary_temp) == 0))
			{
				gchar* italic_text = g_markup_printf_escaped("<i>%s</i>", string->str);
				if( NULL == italic_text) g_fprintf(stderr,"NulIMKUp:'%s'\n",string->str);
				gtk_label_set_markup((GtkLabel*)item_label, italic_text);
				g_free(italic_text);
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

/******************************************************************************/

static void show_history_menu(guint mouse_button, guint32 activate_time)
{
	if (activate_time == GDK_CURRENT_TIME)
		activate_time = gtk_get_current_event_time();

	history_menu_query_t * query = g_try_new(history_menu_query_t, 1);
	if (!query)
		return;

	query->mouse_button = mouse_button;
	query->activate_time = activate_time;

	g_timeout_add(POPUP_DELAY, do_show_history_menu, query);
}
