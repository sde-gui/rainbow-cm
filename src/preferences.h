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

#ifndef PREFERENCES_H
#define PREFERENCES_H

G_BEGIN_DECLS

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

/**for our mapper  */
#define PM_INIT 0
#define PM_UPDATE 1

/*struct pref_item* get_pref(char *name); */
void pref_mapper (struct pref2int *m, int mode);
gint32 set_pref_int32(char *name, gint32 val);
gint32 get_pref_int32 (char *name);
int set_pref_string (char *name, char *string);
gchar *get_pref_string (char *name);

void bind_keys(void);
void unbind_keys(void);

void read_preferences(void);

void show_preferences(void);

G_END_DECLS

#endif
