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

#ifndef MAIN_H
#define MAIN_H
#include "preferences.h"
G_BEGIN_DECLS

extern GMutex *hist_lock;

#define POPUP_DELAY    100

struct widget_info{
	GtkWidget *menu; /**top level history list window  */
	GtkWidget *item; /**item we are looking at  */
	GdkEventKey *event; /**event info where we filled this struct  */
	gint index;      /**index into the array  */
};
/**keeps track of each menu item and the element it's created from.  */
struct s_item_info {
	GtkWidget *item;
	GList *element;
};

struct history_info{
	GtkWidget *menu;			/**top level history menu  */
	GList *delete_list; /**struct s_item_info - for the delete list  */
	GList *persist_list; /**struct s_item_info - for the persistent list  */
	struct widget_info wi;  /**temp  for usage in popups  */
	gint change_flag;	/**bit wise flags for history state  */
	GtkIMContext * im_context;
	GString * search_string;
	gchar * search_string_casefold;
	GtkWidget * first_matched;
};

void on_history_hotkey(char *keystring, gpointer user_data);
void on_menu_hotkey(char *keystring, gpointer user_data);
void on_enable_cm_hotkey(char *keystring, gpointer user_data);
void on_disable_cm_hotkey(char *keystring, gpointer user_data);
void on_run_command_hotkey(char *keystring, gpointer user_data);

void update_status_icon(void);

G_END_DECLS

#endif
