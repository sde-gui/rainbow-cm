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

#include "about.h"
#include "rainbow-cm.h"
#include <gtk/gtk.h>

void show_about_dialog()
{
  /* This helps prevent multiple instances */
  if (!gtk_grab_get_current())
  {
    const gchar* authors[] = {
		"Vadim Ushakov <vadim.ush@gmail.com>",
		"Gilberto \"Xyhthyx\" Miralla <xyhthyx@gmail.com>",
		"Doug Springer <gpib@rickyrockrat.net>",
		NULL};
    const gchar* license =
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

    /* Create the about dialog */
    GtkWidget* about_dialog = gtk_about_dialog_new();
    gtk_window_set_icon((GtkWindow*)about_dialog,
                        gtk_widget_render_icon(about_dialog, GTK_STOCK_ABOUT, -1, NULL));

    gtk_about_dialog_set_name((GtkAboutDialog*)about_dialog, "Rainbow CM");
    gtk_about_dialog_set_version((GtkAboutDialog*)about_dialog, VERSION);
    gtk_about_dialog_set_comments((GtkAboutDialog*)about_dialog,
                                _("Lightweight GTK+ clipboard manager."));

    gtk_about_dialog_set_website((GtkAboutDialog*)about_dialog,
                                 PACKAGE_BUGREPORT);
    
    gtk_about_dialog_set_copyright((GtkAboutDialog*)about_dialog, _("Copyright (C) 2015-2020 Vadim Ushakov\nCopyright (C) 2007, 2008 Gilberto \"Xyhthyx\" Miralla\nCopyright (C) 2010-2013 Doug Springer"));
    gtk_about_dialog_set_authors((GtkAboutDialog*)about_dialog, authors);
    gtk_about_dialog_set_translator_credits ((GtkAboutDialog*)about_dialog,
                                             "Miloš Koutný <milos.koutny@gmail.com>\n"
                                             "Kim Jensen <reklamepost@energimail.dk>\n"
                                             "Eckhard M. Jäger <bart@neeneenee.de>\n"
                                             "Michael Stempin <mstempin@web.de>\n"
                                             "Benjamin Danon <benjamin@sphax3d.org>\n" 
                                             "Németh Tamás <ntomasz@vipmail.hu>\n"
                                             "Davide Truffa <davide@catoblepa.org>\n"
                                             "Jiro Kawada <jiro.kawada@gmail.com>\n"
                                             "Øyvind Sæther <oyvinds@everdot.org>\n"
                                             "pankamyk <pankamyk@o2.pl>\n"
                                             "Tomasz Rusek <tomek.rusek@gmail.com>\n"
                                             "Phantom X <megaphantomx@bol.com.br>\n"
                                             "Ovidiu D. Niţan <ov1d1u@sblug.ro>\n"
                                             "Alexander Kazancev <kazancas@mandriva.ru>\n"
                                             "Daniel Nylander <po@danielnylander.se>\n"
                                             "Hedef Türkçe <iletisim@hedefturkce.com>\n"
                                             "Lyman Li <lymanrb@gmail.com>\n"
                                             "Gilberto \"Xyhthyx\" Miralla <xyhthyx@gmail.com>");
    
    gtk_about_dialog_set_license((GtkAboutDialog*)about_dialog, license);
	  gtk_about_dialog_set_logo_icon_name((GtkAboutDialog*)about_dialog, APP_ICON);
    /* Run the about dialog */
    gtk_dialog_run((GtkDialog*)about_dialog);
    gtk_widget_destroy(about_dialog);
  }
}


#endif /* ABOUT_H */
