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

#ifndef ABOUT_H
#define ABOUT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "about.h"
#include "rainbow-cm.h"
#include <gtk/gtk.h>

static const char ** get_authors(void)
{
	static const gchar* authors[] = {
		"Vadim Ushakov <vadim.ush@gmail.com>",
		"Gilberto \"Xyhthyx\" Miralla <xyhthyx@gmail.com>",
		"Doug Springer <gpib@rickyrockrat.net>",
		NULL
	};

	return authors;
}

static const char * get_translator_credits(void)
{
	static const char * translator_credits = NULL;
	static const char * translator_credits_translated = NULL;

	translator_credits = N_("translator-credits");
	translator_credits_translated = _(translator_credits);

	if (!translator_credits_translated)
		return NULL;

	if (strlen(translator_credits_translated) == 0)
		return NULL;

	if (strcmp(translator_credits_translated, translator_credits) == 0)
		return NULL;

	return translator_credits_translated;
}

static const char * get_copyright(void)
{
	static const char * copyright = NULL;
	copyright = _(
		"Copyright (C) 2015-2020 Vadim Ushakov\n"
		"Copyright (C) 2010-2013 Doug Springer\n"
		"Copyright (C) 2007, 2008 Gilberto \"Xyhthyx\" Miralla"
	);
	return copyright;
}

static const char * get_licence(void)
{
	static const char * license =
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

	return license;
}

static const char * get_name(void)
{
	static const char * name = NULL;
	name = _(
		"Rainbow CM"
	);
	return name;
}

static const char * get_comments(void)
{
	static const char * comments = NULL;
	comments = _(
		"Lightweight GTK+ clipboard manager."
	);
	return comments;
}

static const char * get_version(void)
{
	return VERSION;
}

static const char * get_website_url(void)
{
	return PACKAGE_BUGREPORT;
}

/******************************************************************************/

void show_about_dialog()
{
	/* FIXME: meh! */
	/* This helps prevent multiple instances */
	if (gtk_grab_get_current())
		return;

	GtkWidget* about_dialog = gtk_about_dialog_new();

	gtk_window_set_icon((GtkWindow*)about_dialog,
		gtk_widget_render_icon(about_dialog, GTK_STOCK_ABOUT, -1, NULL));

	gtk_about_dialog_set_name((GtkAboutDialog*)about_dialog, get_name());
	gtk_about_dialog_set_version((GtkAboutDialog*)about_dialog, get_version());
	gtk_about_dialog_set_comments((GtkAboutDialog*)about_dialog, get_comments());
	gtk_about_dialog_set_website((GtkAboutDialog*)about_dialog, get_website_url());

	gtk_about_dialog_set_copyright((GtkAboutDialog*)about_dialog,get_copyright());
    gtk_about_dialog_set_authors((GtkAboutDialog*)about_dialog, get_authors());

	const char * translator_credits = get_translator_credits();
	if (translator_credits)
		gtk_about_dialog_set_translator_credits((GtkAboutDialog*)about_dialog, translator_credits);

	gtk_about_dialog_set_license((GtkAboutDialog*)about_dialog, get_licence());
	gtk_about_dialog_set_logo_icon_name((GtkAboutDialog*)about_dialog, APP_ICON);

	gtk_dialog_run((GtkDialog*)about_dialog);
	gtk_widget_destroy(about_dialog);
}

#endif /* ABOUT_H */
