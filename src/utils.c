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


#include "parcellite.h"
/**for our fifo interface  */
#include <sys/types.h>
#include <dirent.h> 
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
/**set this to debug looking up user environment vars to see other instances
of parcellite  */
/**#define DEBUG_MULTI  */
#ifdef DEBUG_MULTI
#  define DMTRACE(x) x
#else
#  define DMTRACE(x) do {} while (FALSE);
#endif

/** Wrapper to replace g_strdup to limit size of text copied from clipboard. 
g_strndup will dup to the size of the limit, which will waste resources, so
try to allocate using other methods.
*/
gchar *p_strdup( const gchar *str )
{
  gchar *n=NULL;
  size_t l,x;
  if(NULL == str)
    return NULL;
	x=get_pref_int32("item_size")*1000; 
	l=get_pref_int32("data_size")*1000; 
	if(l>0 && l<x) /**whichever is smaller, limit.  */
		x=l;
  if(0 == x)
    return g_strdup(str);
		/**use the following to test truncation  */
	  /*x=get_pref_int32("data_size")*10; */
  l=strlen(str);
    
/*  g_printf("Str '%s' x=%d l=%d u8=%d ",str,x,l,u8); */
  if(l>x){
    l=x;
  }
/*	g_printf("Tl=%d ",l); */
  
  if(NULL !=(n=g_malloc(l+8))){
    n[l+7]=0;
    g_strlcpy(n,str,l+1);
  }
/*  g_printf("str '%s'\n",n);  */
  return n;
}

/* Creates program related directories if needed */
void check_dirs( void )
{
  gchar* data_dir = g_build_path(G_DIR_SEPARATOR_S, g_get_user_data_dir(), DATA_DIR,  NULL);
  gchar* config_dir = g_build_path(G_DIR_SEPARATOR_S, g_get_user_config_dir(), CONFIG_DIR,  NULL);
	gchar *rc_file;
	
  /* Check if data directory exists */
  if (!g_file_test(data_dir, G_FILE_TEST_EXISTS))
  {
    /* Try to make data directory */
    if (g_mkdir_with_parents(data_dir, 0755) != 0)
      g_warning(_("Couldn't create directory: %s\n"), data_dir);
  }
  /* Check if config directory exists */
  if (!g_file_test(config_dir, G_FILE_TEST_EXISTS))
  {
    /* Try to make config directory */
    if (g_mkdir_with_parents(config_dir, 0755) != 0)
      g_warning(_("Couldn't create directory: %s\n"), config_dir);
  }
	/**now see if we need to setup any config files  */
	rc_file = g_build_filename(g_get_user_config_dir(), PREFERENCES_FILE, NULL);
	if(0 != access(rc_file,  R_OK)){/**doesn't exist */
		const gchar * const *sysconfig=g_get_system_config_dirs();	/**NULL-terminated list of strings  */
		const gchar * const *d;
		gchar *sysrc;
		for (d=sysconfig; NULL!= *d; ++d){
			sysrc=g_build_filename(*d, PREFERENCES_FILE, NULL);
			g_fprintf(stderr,"Looking in '%s'\n",sysrc);
			if(0 == access(sysrc,  F_OK)){/**exists */
				GError *e=NULL;
				GFile *src=g_file_new_for_path(sysrc);
				GFile *dst=g_file_new_for_path(rc_file);
				g_fprintf(stderr,"Using parcelliterc from '%s', place in '%s'\n",sysrc,rc_file);
				if(FALSE ==g_file_copy(src,dst,G_FILE_COPY_NONE,NULL,NULL,NULL,&e)){
					g_fprintf(stderr,"Failed to copy. %s\n",e->message);
				}
				g_free(sysrc);
				goto done;
			}	
			g_free(sysrc);
		}
	}	
done:
	/* Cleanup */
	g_free(rc_file);
	g_free(data_dir);
  g_free(config_dir);
}

/* Returns TRUE if text is a hyperlink */
gboolean is_hyperlink(gchar* text)
{
  /* TODO: I need a better regex, this one is poor */
  GRegex* regex = g_regex_new("([A-Za-z][A-Za-z0-9+.-]{1,120}:[A-Za-z0-9/]" \
                              "(([A-Za-z0-9$_.+!*,;/?:@&~=-])|%[A-Fa-f0-9]{2}){1,333}" \
                              "(#([a-zA-Z0-9][a-zA-Z0-9$_.+!*,;/?:@&~=%-]{0,1000}))?)",
                              G_REGEX_CASELESS, 0, NULL);
  
  gboolean result = g_regex_match(regex, text, 0, NULL);
  g_regex_unref(regex);
  return result;
}



/* Parses the program arguments. Returns TRUE if program needs
 * to exit after parsing is complete
 */
struct cmdline_opts *parse_options(int argc, char* argv[])
{
    struct cmdline_opts * opts = g_malloc0(sizeof(struct cmdline_opts));
    if (NULL == opts) {
        g_fprintf(stderr,"Unable to malloc cmdline_opts\n");
        return NULL;
    }

    if (argc <= 1)
        return opts;

    GOptionEntry main_entries[] =
    {
        {
            "no-icon", 'n',
            0,
            G_OPTION_ARG_NONE,
            &opts->icon, _("Do not use status icon (Ctrl-Alt-P for menu)"),
            NULL
        },
        {
            "version", 'v',
            0,
            G_OPTION_ARG_NONE,
            &opts->version, _("Display Version info"),
            NULL
        },
        {
            NULL
        }
    };

    GOptionContext* context = g_option_context_new(NULL);
    /*g_option_context_set_summary(context,
        _(""));*/
    g_option_context_set_description(context, _(
            "Copyright (c) 2015 Vadim Ushakov.\n"
            "Copyright (c) 2007-2014 Gilberto \"Xyhthyx\" Miralla and Doug Springer.\n"
            "Report bugs to <igeekless@gmail.com>."
    ));
    g_option_context_add_main_entries(context, main_entries, NULL);
    g_option_context_parse(context, &argc, &argv, NULL);
    g_option_context_free(context);

    if (opts->icon)
        set_pref_int32("no_icon",TRUE);
    else
        set_pref_int32("no_icon",FALSE);

    if (opts->version) {
        gchar *v;
        #ifdef HAVE_CONFIG_H
            v = VERSION;
        #else
            v = "Unknown";
        #endif
        g_fprintf(stderr,"Rainbow Clipboard Manager %s, GTK %d.%d.%d\n",
            v, gtk_major_version, gtk_minor_version,gtk_micro_version);
        opts->exit = 1;
    }
	return opts;
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
void show_gtk_dialog(gchar *message, gchar *title)
{
GtkWidget *dialog;
	if(NULL == message || NULL == title)
		return;
	dialog= gtk_message_dialog_new(NULL,GTK_DIALOG_DESTROY_WITH_PARENT,
	            GTK_MESSAGE_WARNING,  GTK_BUTTONS_OK,
	            message,NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), title ); 
	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);	
}

