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
#include <pthread.h>

/******************************************************************************/

static void on_save_history_menu_item_activated(GtkMenuItem *menu_item, gpointer user_data)
{
	history_save_as();
}

/******************************************************************************/

static void on_clear_history_menu_item_activated(GtkMenuItem *menu_item, gpointer user_data)
{
	int do_clear = 1;
	GtkWidget * confirm_dialog = gtk_message_dialog_new(
		NULL,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_OK_CANCEL,
		_("Clear the history?"));

	gtk_window_set_title((GtkWindow *) confirm_dialog, "Rainbow CM");

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

/******************************************************************************/

static void on_enabled_menu_item_toggled(GtkCheckMenuItem * menu_item, gpointer user_data)
{
	set_pref_int32("enabled", gtk_check_menu_item_get_active(menu_item));
}

/******************************************************************************/

static void on_about_menu_item_activated(GtkMenuItem *menu_item, gpointer user_data)
{
	show_about_dialog();
}

/******************************************************************************/

static void on_preferences_menu_item_activated(GtkMenuItem *menu_item, gpointer user_data)
{
	/* FIXME: wrong way! */
	/* This helps prevent multiple instances */
	if (!gtk_grab_get_current()) {
		show_preferences(0);
	}
}

/******************************************************************************/

static void on_quit_menu_item_activated(GtkMenuItem *menu_item, gpointer user_data)
{
	/* Prevent quit with dialogs open */
	if (!gtk_grab_get_current())
		/* Quit the program */
		gtk_main_quit();
}

/******************************************************************************/

static GtkWidget * add_menu_item(GtkWidget * menu, const char * title, const char * tooltip, GCallback callback)
{
	GtkWidget * menu_item = gtk_image_menu_item_new_with_mnemonic(title);
	g_signal_connect((GObject*)menu_item, "activate", callback, NULL);
	if (tooltip)
		gtk_widget_set_tooltip_text(menu_item, tooltip);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	return menu_item;
}

static GtkWidget * add_check_menu_item(GtkWidget * menu, const char * title, const char * tooltip, GCallback callback, int checked)
{
	GtkWidget * menu_item = gtk_check_menu_item_new_with_mnemonic(title);
	g_signal_connect((GObject*)menu_item, "toggled", callback, NULL);
	if (tooltip)
		gtk_widget_set_tooltip_text(menu_item, tooltip);
	gtk_check_menu_item_set_active((GtkCheckMenuItem *) menu_item, checked);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	return menu_item;
}

/******************************************************************************/

static GtkWidget * create_main_menu(void)
{
	GtkWidget * menu = gtk_menu_new();

	add_menu_item(menu,
		_("_Save History..."), _("Save the clipboard history in a text file."),
		(GCallback) on_save_history_menu_item_activated);

	add_menu_item(menu,
		_("_Clear History"),
		_("Erase all entries from the clipboard history."),
		(GCallback) on_clear_history_menu_item_activated);

	gtk_menu_shell_append((GtkMenuShell*)menu, gtk_separator_menu_item_new());

	{
		const char * title = clipboard_management_enabled ?
			_("_Enabled") :
			_("Clipboard Management Disabled");

		const char * tooltip = clipboard_management_enabled ?
			_("Clipboard Management and History Tracking are enabled.") :
			_("Clipboard Management and History Tracking are disabled.\nClick here to enable.");

		add_check_menu_item(menu,
			title,
			tooltip,
			(GCallback) on_enabled_menu_item_toggled,
			clipboard_management_enabled);
	}

	add_menu_item(menu,
		_("_Preferences"),
		NULL,
		(GCallback) on_preferences_menu_item_activated);

	add_menu_item(menu,
		_("_About"),
		NULL,
		(GCallback) on_about_menu_item_activated);

	gtk_menu_shell_append((GtkMenuShell*)menu, gtk_separator_menu_item_new());

	add_menu_item(menu,
		_("_Quit"),
		NULL,
		(GCallback) on_quit_menu_item_activated);

	gtk_widget_show_all(menu);
	return menu;
}

static void  show_main_menu(GtkStatusIcon *status_icon, guint button, guint activate_time,  gpointer data)
{
	GtkWidget * menu = create_main_menu();
	gtk_menu_popup((GtkMenu*)menu, NULL, NULL, NULL, NULL, button, activate_time);	
}
