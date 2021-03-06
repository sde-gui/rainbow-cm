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

#ifndef HISTORY_H
#define HISTORY_H

G_BEGIN_DECLS

#define HISTORY_FILE "rainbow-cm/history"

#define CLIP_TYPE_TEXT       0x1
#define CLIP_TYPE_IMG        0x2
#define CLIP_TYPE_PERSISTENT 0x4

struct history_item {
	guint32 len; /**length of data item, MUST be first in structure  */
	gint16 type; /**currently, text or image  */
	gint16 flags;	/**persistence, or??  */
	guint32 res[4];
	gchar text[8]; /**reserve 64 bits (8 bytes) for pointer to data.  */
}__attribute__((__packed__));


extern GList* history_list;

glong validate_utf8_text(gchar *text, glong len);

void read_history();

void save_history(void);

void history_add_text_item(gchar * text, gint flags);

void truncate_history();

void clear_history(void);

void history_save_as(void);
G_END_DECLS

#endif
