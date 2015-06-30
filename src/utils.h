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

#ifndef UTILS_H
#define UTILS_H

G_BEGIN_DECLS
#include "parcellite.h"
#define CONFIG_DIR  APP_PROG_NAME
#define DATA_DIR    APP_PROG_NAME

struct cmdline_opts {
	gboolean icon;
	gboolean exit;
	gboolean version;
};

void check_dirs( void );

struct cmdline_opts *parse_options(int argc, char* argv[]);

void show_gtk_dialog(gchar *message, gchar *title);
G_END_DECLS

#endif
