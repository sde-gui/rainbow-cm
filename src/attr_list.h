/* Copyright (C) 2011-2013 by rickyrockrat <gpib at rickyrockrat dot net>
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
 */
#ifndef _ATTR_LIST_H_
#define  _ATTR_LIST_H_ 1
/**for add/find/remove from list  */
#define OPERATE_DELETE 1 /**delete_list  */
#define OPERATE_PERSIST 2	/**persist_list  */

GList *find_h_item(GList *list,GtkWidget *w, GList *e);
void remove_deleted_items(struct history_info *h);
void handle_marking(struct history_info *h, GtkWidget *w, gint index, gint which);
#endif

