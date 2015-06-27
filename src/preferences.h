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
 */

#ifndef PREFERENCES_H
#define PREFERENCES_H

G_BEGIN_DECLS

#define FIFO_FILE_C          "rainbow-cm/fifo_c"
#define FIFO_FILE_P          "rainbow-cm/fifo_p"
#define FIFO_FILE_CMD          "rainbow-cm/fifo_cmd"
#define FIFO_FILE_DAT         "rainbow-cm/fifo_data"
#define PREFERENCES_FILE      "rainbow-cm/rainbow-cm.rc"

struct keys {
	gchar *name;
	gchar *keyval;
	void *keyfunc;
};
struct pref2int {
	gchar *name;
	int *val;
};
struct tool_flag {
	int flag;
	gchar *name;
};

/**for our mapper  */
#define PM_INIT 0
#define PM_UPDATE 1

extern struct keys keylist[];
/*struct pref_item* get_pref(char *name); */
void pref_mapper (struct pref2int *m, int mode);
gint32 set_pref_int32(char *name, gint32 val);
gint32 get_pref_int32 (char *name);
int set_pref_string (char *name, char *string);
gchar *get_pref_string (char *name);
void bind_itemkey(char *name, void (fhk)(char *, gpointer) );
void read_preferences(void);

void show_preferences(gint tab);

G_END_DECLS

#endif
