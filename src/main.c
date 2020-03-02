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

#include "rainbow-cm.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <ctype.h>

#define DEFERRED_CHECK_INTERVAL 300

static GtkClipboard * selection_primary;
static GtkClipboard * selection_clipboard;
static gchar * text_primary = NULL;
static gchar * text_clipboard = NULL;
static gchar * last_text = NULL; /**last text change, for either clipboard  */


static GtkStatusIcon *status_icon=NULL; 
GMutex *hist_lock=NULL;

typedef enum {
	CLIPBOARD_ACTION_RESET, /* clear out clipboard  */
	CLIPBOARD_ACTION_SET,   /* set clippoard content  */
	CLIPBOARD_ACTION_CHECK  /* see if there is new/lost contents */
} CLIPBOARD_ACTION;

/******************************************************************************/

static void schedule_deferred_clipboard_update(void);
static void disable_deferred_clipboard_update(void);

/******************************************************************************/

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

/******************************************************************************/

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

/******************************************************************************/

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

/******************************************************************************/

static gboolean content_exists(GtkClipboard *clip)
{
	gint count;
	GdkAtom *targets;
	gboolean contents = gtk_clipboard_wait_for_targets(clip, &targets, &count);
	g_free(targets);
	return contents;
}

/******************************************************************************/

static gchar * get_clipboard_text(GtkClipboard * clip)
{
	if (gtk_clipboard_wait_is_text_available(clip))
		return(gtk_clipboard_wait_for_text(clip));
	return NULL;
}

/******************************************************************************/

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

/******************************************************************************/

static void update_clipboards(CLIPBOARD_ACTION action, gchar * text_to_set)
{
	/*g_printf("upclips\n"); */
	update_clipboard(selection_primary, action, text_to_set);
	update_clipboard(selection_clipboard, action, text_to_set);
}

/******************************************************************************/

static void check_clipboards(void)
{
	gchar * ptext = update_clipboard(selection_primary, CLIPBOARD_ACTION_CHECK, NULL);
	gchar * ctext = update_clipboard(selection_clipboard, CLIPBOARD_ACTION_CHECK, NULL);

	if (clipboard_management_enabled &&
		synchronize &&
		track_primary_selection &&
		track_clipboard_selection)
	{
		if (ptext || ctext) {
			gchar * last = last_text;
			if (last && g_strcmp0(ptext, ctext) != 0) {
				last = g_strdup(last);
				update_clipboards(CLIPBOARD_ACTION_SET, last);
				g_free(last);
			}
		}
	}
}

/******************************************************************************/

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

/******************************************************************************/

static void on_clipboard_owner_change(GtkClipboard * clipboard, GdkEvent * event, gpointer user_data)
{
	check_clipboards();
}

/******************************************************************************/

#include "history-menu.c.h"
#include "main-menu.c.h"

/******************************************************************************/

/* Called when status icon is left-clicked */
static void status_icon_clicked(GtkStatusIcon *status_icon, gpointer user_data)
{
  show_history_menu(1, GDK_CURRENT_TIME);
}

/******************************************************************************/

void update_status_icon(void)
{
	if (get_pref_int32("display_status_icon"))
	{
		if (!status_icon)
		{
			status_icon = gtk_status_icon_new_from_icon_name(APP_ICON);
			gtk_status_icon_set_tooltip((GtkStatusIcon*)status_icon, _("Clipboard Manager"));
			g_signal_connect((GObject*)status_icon, "activate", (GCallback)status_icon_clicked, NULL);
			g_signal_connect((GObject*)status_icon, "popup-menu", (GCallback)show_main_menu, NULL);
		}
		gtk_status_icon_set_visible((GtkStatusIcon*)status_icon, TRUE);
	}
	else
	{
		if (status_icon)
		{
			gtk_status_icon_set_visible((GtkStatusIcon*)status_icon, FALSE);
		}
	}
}

/******************************************************************************/

void on_history_hotkey(char *keystring, gpointer user_data)
{
	show_history_menu(0, GDK_CURRENT_TIME);
}

void on_menu_hotkey(char *keystring, gpointer user_data)
{
	show_main_menu(status_icon, 0, 0, NULL);
}

/******************************************************************************/

void on_enable_cm_hotkey(char *keystring, gpointer user_data) { set_pref_int32("enabled", TRUE); }
void on_disable_cm_hotkey(char *keystring, gpointer user_data) { set_pref_int32("enabled", FALSE); }

/******************************************************************************/

void on_run_command_hotkey(char *keystring, gpointer user_data)
{
	gint argc = 0;
	gchar **argv = NULL;
	GError *error = NULL;

	gchar * text = get_clipboard_text(selection_primary);
	if (!text)
		goto out;

	if (!g_shell_parse_argv(text, &argc, &argv, &error))
	{
		goto out;
	}

	if (!g_spawn_async(
			g_get_tmp_dir(),
			argv,
			NULL, /* env */
			G_SPAWN_SEARCH_PATH,
			NULL, /* child_setup */
			NULL, /* user_data */
			NULL, /* child_pid */
			&error
	))
	{
		goto out;
	}

out:
	if (error)
	{
		GtkWidget * dialog = gtk_message_dialog_new (
			NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"Error running the command:\n"
			"%s\n"
			"\n"
			"%s\n",
			text,
			error->message
		);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	if (error)
		g_error_free(error);
	if (argv)
		g_strfreev(argv);
	if (text)
		g_free(text);
}

/******************************************************************************/

static void application_init(void)
{
	int i;

	/* Create clipboard */
	selection_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	selection_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	hist_lock= g_mutex_new();

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

	keybinder_init();
	bind_keys();

	update_status_icon();
}

/******************************************************************************/

int main(int argc, char *argv[])
{
	struct cmdline_opts *opts;
	
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	gtk_init(&argc, &argv);

	/**this just maps to the static struct, prefs do not need to be loaded  */
	pref_mapper(pref2int_map,PM_INIT); 
	check_dirs(); /**make sure we set up default RC if it doesn't exist.  */

	read_preferences();

	opts = parse_options(argc, argv);
	if (!opts)
		return 1;
	if (opts->exit)
		return 0;

	application_init();
	gtk_main();

	unbind_keys();

	/*
	g_free(prefs.history_key);
	g_free(prefs.menu_key);
	*/
	g_list_free(history_list);

	return 0;
}
