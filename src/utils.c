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
            "daemon", 'd',
            0,
            G_OPTION_ARG_NONE,
            &opts->daemon, _("Run as daemon"),
            NULL
        },
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
        g_fprintf(stderr,"Rainbow Clipboarb Manager %s, GTK %d.%d.%d\n",
            v, gtk_major_version, gtk_minor_version,gtk_micro_version);
        opts->exit = 1;
    }
	return opts;
}

/***************************************************************************/
/** Given a PID, look at owner of proc to determine UID.
\n\b Arguments:
\n\b Returns:	-1 on error or UID if found
****************************************************************************/
int pid_to_uid (pid_t pid )
{
	struct stat st;
	char path[255];
	snprintf(path, 254, "/proc/%ld", (long)pid);
	if(0 != stat(path,&st) ){
		g_fprintf(stderr,"Can't stat '%s'\n",path);
		perror("");
		return -1;
	}
	/*g_fprintf(stderr,"%ld pid is %ld uid\n",pid,st.st_uid); */
	return (int)st.st_uid;
}

/***************************************************************************/
/** Get the current user name by examining the XDG directory.
\n\b Arguments:
\n\b Returns: username, if found. Use  g_free.
****************************************************************************/
gchar * get_username_xdg( void )
{
	gchar* username;
	username = (gchar *)g_getenv ("HOME");
  if (!username)
    username = (gchar *)g_get_home_dir ();
	/*g_printf("get_username: '%s'\n",username); */
	if(NULL != username ){
		gchar *t;
		t=g_strdup (username);
		username=g_strdup(basename(t));		
		g_free(t);
		return username;
	}
	return NULL;
}

/***************************************************************************/
/** Get uid from pid_to_uid, then get the username from userid
pid_to_uid.struct passwd *getpwuid(uid_t uid); ->  char   *pw_name;
\n\b Arguments: pid of process to lookup
\n\b Returns:
****************************************************************************/
gchar *get_username_pid( pid_t pid )
{
	int uid;
	struct passwd *p;
	if(-1 ==(uid=pid_to_uid(pid)) )
		return NULL;
	if(NULL == (p=getpwuid((uid_t) uid)) ){
		perror("Unable to get username from passwd\n");
		return NULL;
	}
	/*g_printf("%ld uid='%s'\n",pid,p->pw_name); */
	return g_strdup(p->pw_name);
}
/***************************************************************************/
/** Gets a value give the key name from the pid envronment with null-terminated
strings.
\n\b Arguments:
\n\b Returns: 
-1 on error
0 if not found
1 if found
value is filled with value or NULL.
value or NULL. Free value with g_free.
****************************************************************************/
int get_value_from_env(pid_t pid, gchar *key, char **value)
{
	GFile *fp;
	char *env=NULL, *envf;
	gsize elen, i;
	int rtn=-1;
	if(NULL == value){
		g_fprintf(stderr,"get_value_from_env: value null!\n");
		return rtn;
	}
	*value=NULL;
	if(NULL == (envf=(g_malloc(strlen("/proc/9999999/environ"))))) {
		g_fprintf(stderr,"Unable to allocate for environ file name.\n");
		return rtn;
	}
	g_sprintf(envf,"/proc/%ld/environ",(long int)pid);
	if(0 != access(envf,  R_OK)){/**doesn't exist, or not our process  */
		g_fprintf(stderr,"Unable to open '%s' for PID %ld\n",envf,(long int)pid);
		goto error;
	}
	fp=g_file_new_for_path(envf);
	/**this dumps a zero at end of file.  */
	if(FALSE == g_file_load_contents(fp,NULL,&env,&elen,NULL,NULL)	){
		g_fprintf(stderr,"Error finding '%s' for PID %ld\n",envf,(long int)pid);
		goto error;
	}
	if(NULL == env){
		 g_fprintf(stderr,"NULL evironment\n");
		 goto error; 
	}
	/*g_printf("Loaded file '%s', looking for KEY '%s'\n",envf,key); */
	++elen; /**allow for zero @ end of file.  */
	for (i=0; i<elen; ++i){
		gchar *f;
		f=g_strrstr(&env[i],key);
		
		if(NULL != f){
			if(i==0 || f[-1] == 0){/**beginning terminators  */
				i+=f-&env[i]; /**beginning of string  */
				while(i<elen && 0!=env[i] && '=' !=env[i]) ++i;
				if('=' == env[i] ){/**found, return key  */
					*value=g_strdup(&env[i+1]);
					/*g_printf("%s=%s\n",key,value); */
					rtn=1;
					goto error;
				} /**if we get out of this loop, we are either @ 0 or end of environment.  */
			}else
				i+=strlen(f);
		} else { /**go to end of string, & loop increment will go past 0  */
			while(i<elen && 0!= env[i]) ++i;
		}
	}
	rtn=0;/**not found  */
	
error:
	if(NULL != envf)
		g_free (envf);
	if(NULL != env)																										 
		g_free(env);
	return rtn;
}

/** Stat of the pid in /proc/pid give uid.  Environment is only readable by
   owner */
/***************************************************************************/
/** Assume if there is an error, it is a different user/session.
XDG_SESSION_COOKIE and DBUS_SESSION_BUS_ADDRESS are possible candidates for
UUIDs.
\n\b Arguments:
\n\b Returns: 1 if user matches what is found in the pid environ, or error
Returns 0 if it all works and the user is found in the pid environ.
****************************************************************************/
int is_current_user (pid_t pid, int mode)
{ 
	int rtn=1;
	gchar *username=NULL;
	gchar *user=NULL;
	if(PROC_MODE_USER_QUALIFY & mode ){
		username=get_username_pid(getpid());
		if(NULL == username){
			g_fprintf(stderr,"get_username fail\n");
			goto done;
		}
		user=get_username_pid(pid);	
		/*user=get_value_from_env(pid,"USERNAME"); */
		/*g_printf("user='%s'\n",user); */
		if(NULL == user){
			goto done;
		}
		/*g_printf("We are '%s', Found '%s' for USERNAME in pid %ld\n",username,user,pid); */
		if(! g_strcmp0(username,user)){
			/*g_printf("Match\n"); */
			goto done;
		}
		rtn=0;
	}	/*else g_printf("UQ not set\n"); */
done:
	if(NULL != user)
		g_free(user);
	if(NULL != username)
		g_free(username); 
	return rtn;
}

/***************************************************************************/
/** .
\n\b Arguments: Pid to check session type.
\n\b Returns: 
-1 if error
0 if not found
1 if it is found (i.e. in current session)
2 if it is found, and not in current session
****************************************************************************/
int is_current_xsession_var (pid_t pid, gchar *env_var)
{
	int rtn=-1;
	gchar *mine, *theirs;
	mine=theirs=NULL;
	if(NULL == env_var){
		g_fprintf(stderr,"env_var for pid %ld is NULL!\n",(long)pid);
		return -1;
	}
	if(-1 == get_value_from_env(pid, env_var,&theirs) ){
		g_fprintf(stderr,"Unable to access '%s' for pid %ld\n",env_var,(long)pid);
		return -1;
	}
	if( -1 == get_value_from_env(getpid(),env_var,&mine)){
		g_fprintf(stderr,"Unable to access my '%s'\n",env_var);
		goto done;
	}
	rtn=0;
	if(NULL == theirs)	DMTRACE(g_fprintf(stderr,"Their Null "));
	if(NULL == mine )   DMTRACE(g_fprintf(stderr,"Mine Null "));
	if(NULL != theirs && NULL != mine){
		DMTRACE(g_fprintf(stderr,"Both found "));
		if(! g_strcmp0(mine,theirs))
			rtn=1;	
		else
			rtn=2;
	}
	
	/*g_fprintf(stderr,"my='%s', pid %ld='%s',match=%d\n",mine,(long)pid,theirs,rtn);  */
done:
	if(NULL != mine)
		g_free(mine);
	if(NULL != theirs)
		g_free(theirs);
	return rtn;
}

/***************************************************************************/
/** Use several methods to figure out the session we are in.
1) Check XDG_SESSION_COOKIE
2) Check DISPLAY var
3) ??
\n\b Arguments:
\n\b Returns:
1 if in current session
0 if not in current session
 #-1 if error
****************************************************************************/
int is_current_xsession (pid_t pid)
{
	int i,x;
	gchar *names[]={"XDG_SESSION_COOKIE","XDG_SEAT","DISPLAY",NULL};
		
	for (i=0;NULL != names[i]; ++i){
		DMTRACE(g_fprintf(stderr,"@%s ",names[i]));
		if((x=is_current_xsession_var(pid,names[i]))>0){
			if( 1 == x){
				DMTRACE(g_fprintf(stderr," Found \n"));
				return 1;
			}	else {
				DMTRACE(g_fprintf(stderr," Not Found \n"));
				return 0;
			}
				
		}	
	}
	/**default to not in current session  */
	return 0;
}
/***************************************************************************/
/** Return a PID given a name. Used to check a if a process is running..
if 2 or greater, process is running
TODO: Figure out if we are in the same X instance....Possibly this?
Get our DISPLAY var
Display *XOpenDisplay(NULL); --default to DISPLAY env var....
char *XDisplayString(Display *display);	
Then grab the Display var from ENV, or just use ENV?? Ugh.

\n\b Arguments: 
name is name of program to find
mode sets the mode of the find (exact or partial)
pid is a place to put the last found pid

\n\b Returns:	number of instances of name found
Note: /proc/pid/environ contains interesting variables:
HOME
USERNAME
LOGNAME
with this format:
0x00KEY=VAL0x00
****************************************************************************/
int proc_find(const char* name, int mode, pid_t *pid) 
{
	DIR* dir;
	FILE* fp;
	struct dirent* ent;
  char* endptr;
  char buf[PATH_MAX];
	int instances=0;
 
  if (!(dir = opendir("/proc"))) {
  	perror("can't open /proc");
    return -1;
  }

  while((ent = readdir(dir)) != NULL) {
    /* if endptr is not a null character, the directory is not
     * entirely numeric, so ignore it */
    long lpid = strtol(ent->d_name, &endptr, 10);
    if (*endptr != '\0') {
        continue;
    }
    /* try to open the cmdline file */
    snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);

    if ((fp= fopen(buf, "r"))) {
      if (fgets(buf, sizeof(buf), fp) != NULL) {
				if(PROC_MODE_EXACT & mode){
					gchar *b=g_path_get_basename(buf);
		      if (!g_strcmp0(b, name)) {
						if(is_current_user(lpid,mode) && 1 == is_current_xsession(lpid) )
							++instances;
						if(NULL !=pid)
						  *pid=lpid;
		      }
				}else if( PROC_MODE_STRSTR & mode){
					if(NULL !=g_strrstr(buf,name)){
						if(is_current_user(lpid,mode) && 1 == is_current_xsession(lpid))
							++instances;
						if(NULL !=pid)
						  *pid=lpid;
						/*g_printf("Looking for '%s', found '%s'\n",name,buf); */
					}
				}
				
      }
      fclose(fp);
    }

  }

  closedir(dir);
  return instances;
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

