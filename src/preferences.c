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

#include "rainbow-cm.h"
#include <sys/wait.h>

#define MAX_HISTORY 1000

#define INIT_HISTORY_KEY      NULL
#define INIT_MENU_KEY         NULL

#define DEF_SYNCHRONIZE       FALSE
#define DEF_SAVE_HISTORY      TRUE
#define DEF_HISTORY_LIMIT     25
#define DEF_ITEM_LENGTH       50
#define DEF_ITEM_LENGTH_MAX   200
#define DEF_ELLIPSIZE         2
#define DEF_HISTORY_KEY       "<Mod4>Insert"
#define DEF_MENU_KEY          "<Mod4><Ctrl>Insert"
#define DEF_NO_ICON           FALSE

/**allow lower nibble to become the number of items of this type  */
#define PREF_TYPE_TOGGLE 0x10
#define PREF_TYPE_SPIN   0x20
#define PREF_TYPE_COMBO  0x30
#define PREF_TYPE_ENTRY  0x40 /**gchar *  */
#define PREF_TYPE_ALIGN  0x50 /**label, then align box  */
#define PREF_TYPE_SPACER 0x60
#define PREF_TYPE_FRAME  0x70 /**frame for each section  */
#define PREF_TYPE_MASK	 0xF0 
#define PREF_TYPE_NMASK	 0xF
#define PREF_TYPE_SINGLE_LINE 1

typedef enum {
	PREF_SECTION_NONE,
	PREF_SECTION_CLIP,
	PREF_SECTION_HISTORY,
	PREF_SECTION_FILTERING,
	PREF_SECTION_POPUP,
	PREF_SECTION_HOTKEYS,
	PREF_SECTION_MISC
} pref_section_t;

#define RC_VERSION_NAME "RCVersion"
#define RC_VERSION      1

struct myadj {
	gdouble lower;
	gdouble upper;
	gdouble step;
	gdouble page;
};
struct myadj align_hist_x={1,100,1,10};
struct myadj align_hist_y={1,100,1,10};
struct myadj align_data_lim={0,1000000,1,10};
struct myadj align_hist_lim={5, MAX_HISTORY, 1, 10};
struct myadj align_line_lim={5, DEF_ITEM_LENGTH_MAX, 1, 5};
struct pref_item {
	gchar *name;     /** name/id to find pref  */
	gint32 val;      /** int/bool value*/
	gchar *cval;     /** string value  */
	GtkWidget *w;    /** widget in menu  */
	gint type;       /** PREF_TYPE_  */
	gchar *desc;     /** name in GUI */
	gchar *tooltip;  /** tooltip in GUI */
	pref_section_t section; /** section in GUI */
	gchar *sig;      /** signal, if any  */
	GCallback sfunc; /** function to call  */
	struct myadj *adj;
};
static struct pref_item dummy[2];
static void check_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static struct pref2int *pref2int_mapper=NULL;

/**hot key list, mainly for easy sanity checks.  */
struct keys keylist[]={
	{.name="menu_key",.keyval=DEF_MENU_KEY,.keyfunc=(void *)on_menu_hotkey},
	{.name="history_key",.keyval=DEF_HISTORY_KEY,.keyfunc=(void *)on_history_hotkey},
	{.name=NULL,.keyval=NULL,.keyfunc=(void *)0},
};
/**must be in same order as above struct array  */
static gchar *def_keyvals[]={ DEF_MENU_KEY,DEF_HISTORY_KEY};
static struct pref_item myprefs[]={

	{.section=PREF_SECTION_CLIP,.type=PREF_TYPE_FRAME,.desc=N_("<b>Clipboard Management</b>")},
	{.sig="toggled",.sfunc=(GCallback)check_toggled,.section=PREF_SECTION_CLIP,
	 .name="enabled",.type=PREF_TYPE_TOGGLE,
	 .desc=N_("<b>Clipboard Managment _Enabled</b>"),
	 .tooltip=N_("When unchecked, fully disables clipboard managment and clipboard tracking.\n\nThis option is useful to temporarely disable the Rainbow Clipboard Manager, if you encounter a conflict between the Manager and another application, or if you copy and paste confidential information that should not be visible in the clipboard history."),
	 .val=TRUE},
	{.sig="toggled",.sfunc=(GCallback)check_toggled,.section=PREF_SECTION_CLIP,
	 .name="track_clipboard_selection",.type=PREF_TYPE_TOGGLE,
	 .desc=N_("Track the history of the <b>C_lipboard</b> buffer"),
	 .tooltip=N_("If checked, Rainbow CM keeps track of changes in the clipboard (X11 CLIPBOARD SELECTION)."),
	 .val=TRUE},
	{.sig="toggled",.sfunc=(GCallback)check_toggled,.section=PREF_SECTION_CLIP,
	 .name="track_primary_selection",.type=PREF_TYPE_TOGGLE,
	 .desc=N_("Track the history of the <b>_Selected Text</b> buffer"),
	 .tooltip=N_("If checked, Rainbow CM keeps track of changes in the selected text (X11 PRIMARY SELECTION)."),
	 .val=FALSE},
	{.section=PREF_SECTION_CLIP,
	 .name="synchronize",.type=PREF_TYPE_TOGGLE,
	 .desc=N_("Synchroni_ze clipboards"),
	 .tooltip=N_("If checked, Rainbow CM forces the both buffers to keep the same data."),
	 .val=DEF_SYNCHRONIZE},
	{.section=PREF_SECTION_CLIP,
	.name="restore_empty",.type=PREF_TYPE_TOGGLE,
	.desc=N_("Restore the contents of the e_mpty clipboard."),
	.tooltip=N_("Restore the contents of the clipboard when it gets empty.\n\nThe clipboard typically gets empty when an application that has held the clipboard contents is closed."),
	.val=1},

	{.section=PREF_SECTION_HISTORY,.type=PREF_TYPE_FRAME,.desc=N_("<b>History</b>")},
	{.section=PREF_SECTION_HISTORY,.name="save_history",.type=PREF_TYPE_TOGGLE,.desc=N_("Sa_ve history across sessions"),.tooltip=N_("Keep history in a file across sessions."),.val=DEF_SAVE_HISTORY},
	{.adj=&align_hist_lim,.section=PREF_SECTION_HISTORY,.name="history_limit",.type=PREF_TYPE_SPIN,.desc=N_("History limit: {{}} entries"),.tooltip=N_("Maximum number of clipboard entries to keep"),.val=DEF_HISTORY_LIMIT},
	{.section=PREF_SECTION_HISTORY,.name="nop",.type=PREF_TYPE_SPACER,.desc=" ",.tooltip=NULL},

	{.section=PREF_SECTION_FILTERING,.type=PREF_TYPE_FRAME,.desc=N_("<b>Filtering</b>")},
	{.section=PREF_SECTION_FILTERING,.name="ignore_whiteonly",.type=PREF_TYPE_TOGGLE,.desc=N_("Ignore whitespace strings"),.tooltip=N_("Ignore any clipboard data that contain only whitespace characters (space, tab, new line etc).")},

	{.section=PREF_SECTION_POPUP,.type=PREF_TYPE_FRAME,
	 .desc=N_("<b>Settings of the History menu</b>"),
	 .tooltip=NULL,.val=0
	},
	{.section=PREF_SECTION_POPUP,.name="nop",.type=PREF_TYPE_SPACER,.desc=" ",.tooltip=NULL},
	{.section=PREF_SECTION_POPUP,
	 .name="type_search",.type=PREF_TYPE_TOGGLE,
	 .desc=N_("Search _As You Type"),
	 .tooltip=N_("Enables Instant Search in the History menu.\n\nType a word when the History menu is shown to see only the entries that contains this word.")
	},
	{.section=PREF_SECTION_POPUP,
	 .name="display_nonprinting_characters",.type=PREF_TYPE_TOGGLE,
	 .desc=N_("Display _non-printing characters"),
	 .tooltip=N_("Enables displaying of non-printing characters:\n\n"
	  "The horizontal tab character: → (rightwards arrow).\n"
	  "The space character: ␣ (open box).\n"
	  "The new line character: ¶ (paragraph sign)."),
	 .val=FALSE
	},
	{.adj=&align_line_lim,.section=PREF_SECTION_POPUP,
	 .name="item_length",.type=PREF_TYPE_SPIN,
	 .desc=N_("_Limit the History menu width to {{}} characters"),
	 .tooltip=NULL,
	 .val=DEF_ITEM_LENGTH},
	{.section=PREF_SECTION_POPUP,.name="nop",.type=PREF_TYPE_SPACER,.desc=" ",.tooltip=NULL},
	{.section=PREF_SECTION_NONE,
	 .name="ellipsize",.type=PREF_TYPE_COMBO,
	 .desc=N_("Omit characters in the:"),
	 .tooltip=NULL,
	 .val=DEF_ELLIPSIZE
	},

	{.section=PREF_SECTION_HOTKEYS,.name="menu_key",.type=PREF_TYPE_ENTRY,.desc=N_("Men_u key combination"),.tooltip=NULL},
	{.section=PREF_SECTION_HOTKEYS,.name="history_key",.type=PREF_TYPE_ENTRY,.desc=N_("_History key combination:"),.tooltip=NULL},

	{.section=PREF_SECTION_NONE,.name="no_icon",.val=FALSE},

	{.adj=NULL,.cval=NULL,.sig=NULL,.section=PREF_SECTION_NONE,.name=NULL,.desc=NULL},
};

/***************************************************************************/

static GtkWidget * label_new_with_markup_and_mnemonic(const char * markup)
{
	GtkWidget * label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic((GtkLabel *) label, markup);
	return label;
}

/***************************************************************************/

struct pref_item* get_pref(char *name)
{
	int i;
	for (i=0;NULL != myprefs[i].desc; ++i){
		if(NULL == myprefs[i].name)
			continue;
		if(!g_strcmp0(myprefs[i].name,name))
			return &myprefs[i];
	}	
	return &dummy[0];
}

/***************************************************************************/

void pref_mapper (struct pref2int *m, int mode)
{
	int i;
	if(PM_INIT == mode){
		pref2int_mapper=m;
		return;
	}
	for (i=0; pref2int_mapper[i].name != NULL && pref2int_mapper[i].val != NULL; ++i){
		struct pref_item *p=get_pref(pref2int_mapper[i].name);
		*pref2int_mapper[i].val=p->val;
	}
	
}

/***************************************************************************/

static int get_first_pref(pref_section_t section)
{
	int i;
	for (i=0;NULL != myprefs[i].desc; ++i){
		if(section == myprefs[i].section){
			return i;
		}
			
	}	
	return i;
}

/***************************************************************************/

static int init_pref(void)
{
	GdkScreen *s;
	gint sx,sy;
	s=gdk_screen_get_default();
	sx= gdk_screen_get_width(s);
	sy= gdk_screen_get_height(s);
	dummy[0].cval="dummy String";
	dummy[0].w=NULL;
	dummy[0].desc="Dummy desc";
	dummy[0].tooltip="Dummy tip";
	dummy[1].name=NULL;
	dummy[1].section=PREF_SECTION_NONE;
	align_hist_y.upper=sy-100;
	align_hist_x.upper=sx-100;
	return 0;
}

/***************************************************************************/

static GtkWidget *get_pref_widget (char *name)
{
	struct pref_item *p=get_pref(name);
	if(NULL == p)
		return dummy[0].w;
	return p->w;
 }

/***************************************************************************/

int set_pref_int32(char *name, gint32 val)
{
	struct pref_item *p=get_pref(name);
	if(NULL == p)
		return -1;
	p->val=val;
	pref_mapper(NULL, PM_UPDATE);
	return 0;
}

/***************************************************************************/

gint32 get_pref_int32 (char *name)
{
	struct pref_item *p=get_pref(name);
	if(NULL == p)
		return -1;
	return p->val;
}

/***************************************************************************/

int set_pref_string (char *name, char *string)
{
	struct pref_item *p=get_pref(name);
	if(NULL == p)
		return -1;
	if(p->cval != NULL)
		g_free(p->cval);
	p->cval=g_strdup(string);
	return 0;
}

/***************************************************************************/

gchar *get_pref_string (char *name)
{
	struct pref_item *p=get_pref(name);
	if(NULL == p)
		return dummy[0].cval;
	return p->cval;
 }

/***************************************************************************/

void unbind_itemkey(char *name, void *fhk )
{
	
	struct pref_item *p=get_pref(name);
	if(NULL == p){
		return;
	}
	keybinder_unbind(p->cval, fhk);
	g_free(p->cval);
	p->cval=NULL;
	
}

/***************************************************************************/

void bind_itemkey(char *name, void (fhk)(char *, gpointer) )
{
	struct pref_item *p=get_pref(name);
	if(NULL ==p){
		return;
	}
	if(NULL != p->cval && 0 != p->cval)
		keybinder_bind(p->cval, fhk, NULL);
}

/***************************************************************************/

static void set_key_entry(gchar *name, gchar *val)
{
	int i;
	for (i=0;NULL != keylist[i].name; ++i){
		if(!g_strcmp0(keylist[i].name,name)){
		  if(NULL == val)
		  	keylist[i].keyval="";
			else
				keylist[i].keyval=val;
			return;
		}
	}
}

/***************************************************************************/

static void set_keys_from_prefs(void)
{
	int i,l;
	for (i=0;NULL != keylist[i].name; ++i){
		/**NOTE: do not set up default keys here! User may WANT them null */
			/** call egg_accelerator_parse_virtual to validate? */
		set_key_entry(keylist[i].name,get_pref_string(keylist[i].name));
		/*g_fprintf(stderr,"key '%s' val '%s'\n",keylist[i].name, keylist[i].keyval); */
	}	
	/**now go through and make sure we have no duplicates */
	for (i=0;NULL != keylist[i].name; ++i){
		if(0 !=  keylist[i].keyval[0]){
			/**see if it exists elsewhere  */
			for (l=0;NULL != keylist[l].name; ++l){
				if(l!=i && 0 != keylist[l].keyval[0]){
					if(!g_strcmp0(keylist[i].keyval, keylist[l].keyval)) { /**conflict!, delete second  */
						g_fprintf(stderr,"Error! Hot keys have same key '%s': '%s' and '%s'. Ignoring second entry\n",keylist[i].keyval,keylist[i].name,keylist[l].name);
						set_key_entry(keylist[l].name,"");
						set_pref_string(keylist[l].name,"");
					}
				}	
			}	
		}
	}	
	
}

/***************************************************************************/

static void check_sanity(void)
{
	gint32 x;
	x = get_pref_int32("history_limit");
	if ((!x) || (x > MAX_HISTORY) || (x < 0))
		set_pref_int32("history_limit",DEF_HISTORY_LIMIT);

	x = get_pref_int32("item_length");
	if ((!x) || (x > DEF_ITEM_LENGTH_MAX) || (x < 0))
		set_pref_int32("item_length",DEF_ITEM_LENGTH);

	x = get_pref_int32("ellipsize");
	if ((!x) || (x > 3) || (x < 0))
		set_pref_int32("ellipsize",DEF_ELLIPSIZE);

	set_keys_from_prefs();
}
/* Apply the new preferences */
static void apply_preferences()
{
	int i;
  /* Unbind the keys before binding new ones */
	for (i=0;NULL != keylist[i].name; ++i)
	  unbind_itemkey(keylist[i].name,keylist[i].keyfunc);
	
	
  for (i=0;NULL != myprefs[i].desc; ++i){
		if(NULL == myprefs[i].name)
			continue;
		switch(myprefs[i].type&PREF_TYPE_MASK){
			case PREF_TYPE_TOGGLE:
				myprefs[i].val=gtk_toggle_button_get_active((GtkToggleButton*)myprefs[i].w);
				break;
			case PREF_TYPE_SPIN:
				myprefs[i].val=gtk_spin_button_get_value_as_int((GtkSpinButton*)myprefs[i].w);
				break;
			case PREF_TYPE_COMBO:
				myprefs[i].val=gtk_combo_box_get_active((GtkComboBox*)myprefs[i].w) + 1;
				break;
			case PREF_TYPE_ENTRY:
				myprefs[i].cval=g_strdup(gtk_entry_get_text((GtkEntry*)myprefs[i].w ));
				break;
			case PREF_TYPE_SPACER:
				break;
			default:
				break;
		}
	}
  check_sanity();
  /* Bind keys and apply the new history limit */
	for (i=0;NULL != keylist[i].name; ++i)
	  bind_itemkey(keylist[i].name,keylist[i].keyfunc);	
  truncate_history();
	pref_mapper(NULL, PM_UPDATE);
}


/***************************************************************************/

static void save_preferences()
{
	GKeyFile* rc_key = g_key_file_new();
	g_key_file_set_integer(rc_key, "rc", RC_VERSION_NAME, RC_VERSION);

	for (int i = 0; NULL != myprefs[i].desc; ++i)
	{
		if (NULL == myprefs[i].name)
			continue;

		switch(myprefs[i].type&PREF_TYPE_MASK)
		{
			case PREF_TYPE_TOGGLE:
				g_key_file_set_boolean(rc_key, "rc", myprefs[i].name, myprefs[i].val);
				break;
			case PREF_TYPE_COMBO:
			case PREF_TYPE_SPIN:
				g_key_file_set_integer(rc_key, "rc", myprefs[i].name, myprefs[i].val);
				break;
			case PREF_TYPE_ENTRY:
				g_key_file_set_string(rc_key, "rc", myprefs[i].name, myprefs[i].cval);
				break;
			case PREF_TYPE_SPACER:
				break;
			default:
				break;
		}
	}

	check_dirs();

	gchar* rc_file = g_build_filename(g_get_user_config_dir(), PREFERENCES_FILE, NULL);
	g_key_file_save_to_file(rc_key, rc_file, NULL);
	g_key_file_free(rc_key);
	g_free(rc_file);
}


/***************************************************************************/

void read_preferences(void)
{
	gchar *c,*rc_file = g_build_filename(g_get_user_config_dir(), PREFERENCES_FILE, NULL);

	gint32 z;
	GError *err=NULL;

	init_pref();

	GKeyFile* rc_key = g_key_file_new();
	if (g_key_file_load_from_file(rc_key, rc_file, G_KEY_FILE_NONE, NULL))
	{
		int i;
		i=g_key_file_get_integer(rc_key, "rc", RC_VERSION_NAME,&err);

		for (i = 0; NULL != myprefs[i].desc; ++i)
		{
			if (NULL == myprefs[i].name)
				continue;
			err=NULL;
			switch (myprefs[i].type&PREF_TYPE_MASK)
			{
				case PREF_TYPE_TOGGLE:
					z = g_key_file_get_boolean(rc_key, "rc", myprefs[i].name, &err);
					if (NULL == err)
						myprefs[i].val = z;
					break;
				case PREF_TYPE_COMBO:
				case PREF_TYPE_SPIN:
					z = g_key_file_get_integer(rc_key, "rc", myprefs[i].name, &err);
					if (NULL ==err)
						myprefs[i].val = z;
					break;
				case PREF_TYPE_ENTRY:
					c=g_key_file_get_string(rc_key, "rc", myprefs[i].name, &err);
					if (NULL == err)
						myprefs[i].cval=c;
					break;
				case PREF_TYPE_SPACER:
					break;
				default: 
					continue;
					break;
			}

			if (NULL != err)
				g_printf("Unable to load pref '%s'\n", myprefs[i].name);
		}
		/* Check for errors and set default values if any */
		check_sanity();
	}
	else
	{
		/* Init default keys on error */
		int i;
		for (i = 0; NULL != keylist[i].name; ++i)
			set_pref_string(keylist[i].name, def_keyvals[i]);
	}

	pref_mapper(NULL, PM_UPDATE);

	g_key_file_free(rc_key);
	g_free(rc_file);
}

/* Called when clipboard checks are pressed */
static void check_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gtk_widget_set_sensitive(
		get_pref_widget("synchronize"),
		(
			gtk_toggle_button_get_active((GtkToggleButton*)get_pref_widget("track_clipboard_selection")) &&
			gtk_toggle_button_get_active((GtkToggleButton*)get_pref_widget("track_primary_selection"))
		)
	);
}

/***************************************************************************/

static int update_pref_widgets(void)
{
	int i,rtn=0;
	for (i=0;NULL !=myprefs[i].desc; ++i){
		if(NULL != myprefs[i].name){
			switch (myprefs[i].type&PREF_TYPE_MASK){
				case PREF_TYPE_TOGGLE:
					gtk_toggle_button_set_active((GtkToggleButton*)myprefs[i].w, myprefs[i].val);
					break;
				case PREF_TYPE_SPIN:
					gtk_spin_button_set_value((GtkSpinButton*)myprefs[i].w, (gdouble)myprefs[i].val);
					break;
				case PREF_TYPE_COMBO:
					gtk_combo_box_set_active((GtkComboBox*)myprefs[i].w, myprefs[i].val - 1);
					break;
				case PREF_TYPE_ENTRY:
					gtk_entry_set_text((GtkEntry*)myprefs[i].w, myprefs[i].cval);
					break;
				case PREF_TYPE_SPACER:
					break;
				default: 
					rtn=-1;
					continue;
					break;
			}
		}
		
	}	
	
	return rtn;
}

/***************************************************************************/

static void label_pair_from_markup(const gchar * markup, GtkWidget ** p_label1, GtkWidget ** p_label2)
{
	if (p_label1)
		*p_label1 = NULL;
	if (p_label2)
		*p_label2 = NULL;

	gchar ** pair = g_strsplit(markup, "{{}}", 2);

	if (pair && pair[0])
	{
		if (p_label1) {
			*p_label1 = label_new_with_markup_and_mnemonic(pair[0]);
		}

		if (pair[1] && p_label2)
		{
			*p_label2 = label_new_with_markup_and_mnemonic(pair[1]);
		}
	}

	g_strfreev(pair);
}

/***************************************************************************/

static int add_section(pref_section_t sec, GtkWidget *parent)
{
	int i,rtn=0;
	int single_st, single_is;
	gint connect;
	GtkWidget *hbox, *label, *child, *vbox, *alignment;
	GtkWidget* packit;
	vbox=parent;
	single_st=single_is=0;
	for (i=get_first_pref(sec);sec==myprefs[i].section; ++i){
		connect=1;
		single_st=(myprefs[i].type&(PREF_TYPE_NMASK|PREF_TYPE_SINGLE_LINE)); /**deterimine if we are in single line  */
		
		if(single_st && !single_is){ /**start of single line  */
			hbox = gtk_hbox_new(FALSE, 2);  /**create hbox  */
			/*g_printf("alloc %p hbox\n",hbox); */
			single_is=1;
		}
		
		switch (myprefs[i].type & PREF_TYPE_MASK){
			case PREF_TYPE_FRAME:/**must be first in section, since it sets vbox.  */
					myprefs[i].w= gtk_frame_new(NULL);
				  gtk_frame_set_shadow_type((GtkFrame*)	myprefs[i].w, GTK_SHADOW_NONE);
				  label = gtk_label_new(NULL);							/**<b>myprefs[i].desc  */
				  gtk_label_set_markup((GtkLabel*)label, _(myprefs[i].desc));
				  gtk_frame_set_label_widget((GtkFrame*)	myprefs[i].w, label);
				  alignment = gtk_alignment_new(0.50, 0.50, 1.0, 1.0);
				  gtk_alignment_set_padding((GtkAlignment*)alignment, 12, 0, 12, 0);
				  gtk_container_add((GtkContainer*)	myprefs[i].w, alignment);
				  vbox = gtk_vbox_new(FALSE, 2);
				  gtk_container_add((GtkContainer*)alignment, vbox);
					gtk_box_pack_start((GtkBox*)parent,	myprefs[i].w,FALSE,FALSE,0);	
				continue;
				break;
			case PREF_TYPE_TOGGLE:
			{
				GtkWidget * label1;
				label_pair_from_markup(_(myprefs[i].desc), &label1, NULL);

				myprefs[i].w = gtk_check_button_new();
				gtk_container_add(GTK_CONTAINER(myprefs[i].w), label1);

				gtk_label_set_mnemonic_widget((GtkLabel *) label1, myprefs[i].w);

				packit = myprefs[i].w;
				break;
			}
			
			case PREF_TYPE_SPIN:
			case PREF_TYPE_ENTRY:
			{
				GtkWidget * label1;
				GtkWidget * label2;
				label_pair_from_markup(_(myprefs[i].desc), &label1, &label2);

				if(!single_is)
					hbox = gtk_hbox_new(label2 == NULL, 4);

				if (label1) {
					gtk_misc_set_alignment((GtkMisc *) label1, 0.0, 0.50);
					gtk_box_pack_start((GtkBox *) hbox, label1, label2 == NULL, label2 == NULL, 0);
					if (NULL != myprefs[i].tooltip)
						gtk_widget_set_tooltip_text(label1, _(myprefs[i].tooltip));
				}

				if ((myprefs[i].type & PREF_TYPE_MASK) == PREF_TYPE_SPIN)
				{
					myprefs[i].w = gtk_spin_button_new(
						(GtkAdjustment*)gtk_adjustment_new (
							myprefs[i].val,
							myprefs[i].adj->lower,
							myprefs[i].adj->upper,
							myprefs[i].adj->step,
							myprefs[i].adj->page,0
						),
						10, 0
					);
					gtk_box_pack_start((GtkBox*)hbox, myprefs[i].w, TRUE, TRUE, 0);
					gtk_spin_button_set_update_policy((GtkSpinButton*)myprefs[i].w, GTK_UPDATE_IF_VALID);
				}
				else if ((myprefs[i].type & PREF_TYPE_MASK) == PREF_TYPE_ENTRY)
				{
					myprefs[i].w = gtk_entry_new();
					gtk_entry_set_width_chars((GtkEntry*)myprefs[i].w, 10);
					gtk_box_pack_start((GtkBox*)hbox,myprefs[i].w, TRUE, TRUE, 0);
				}
				packit = hbox;

				if (label1)
					gtk_label_set_mnemonic_widget((GtkLabel *) label1, myprefs[i].w);

				if (label2) {
					gtk_misc_set_alignment((GtkMisc *) label2, 0.0, 0.50);
					gtk_box_pack_start((GtkBox *) hbox, label2, FALSE, FALSE, 0);
					if (NULL != myprefs[i].tooltip)
						gtk_widget_set_tooltip_text(label2, _(myprefs[i].tooltip));
					gtk_label_set_mnemonic_widget((GtkLabel *) label2, myprefs[i].w);
				}

				break;
			}
			case PREF_TYPE_COMBO: /**handled in show_preferences, only one so  */
				continue;
				break;
			case PREF_TYPE_SPACER:
				packit=myprefs[i].w=gtk_menu_item_new_with_label("");
				child=gtk_bin_get_child((GtkBin *)packit);
				gtk_misc_set_padding((GtkMisc *)child,0,0);
				gtk_label_set_markup ((GtkLabel *)child, "<span size=\"0\"> </span>");
				break;
			
			default: 
				rtn=-1;
				continue;
				break;
		}
		
		/**tooltips are set on the label of the spin box, not the widget and are handled above */
		if(PREF_TYPE_SPIN != myprefs[i].type && NULL != myprefs[i].tooltip)
		  gtk_widget_set_tooltip_text(myprefs[i].w, _(myprefs[i].tooltip));
		
		if(NULL != myprefs[i].sig && connect)
			g_signal_connect((GObject*)myprefs[i].w, myprefs[i].sig, (GCallback)myprefs[i].sfunc, myprefs[i].w);
		
		if(single_is){
			if(packit != hbox){
				/*g_printf("Packed a slwidget %p<-%p\n",hbox, myprefs[i].w); */
				gtk_box_pack_start((GtkBox*)hbox,myprefs[i].w , FALSE, FALSE, 0);	
			}
			/**else already packed above.  */
		}	else
																							/**espand fill padding  */
			gtk_box_pack_start((GtkBox*)vbox, packit, TRUE, TRUE, 0);
	/**check for end of single line.  */
		single_st=(myprefs[i+1].type&(PREF_TYPE_NMASK|PREF_TYPE_SINGLE_LINE)); /**deterimine if we are in single line  */
		if(single_is && !single_st){/**end of single line  */
																							/**exp fill padding  */
			gtk_box_pack_start((GtkBox*)vbox, hbox, TRUE, TRUE, 0);		/**pack the hbox into parent  */
			/*g_printf("pack %p<-%p hbox\n",vbox,hbox); */
			single_is=0;
		}
	}
	return rtn;
	
}

/* Shows the preferences dialog on the given tab */
void show_preferences(gint tab)
{
	GtkWidget *frame,*label,*alignment,*hbox, *vbox;
	struct pref_item *p;
	init_pref();

	/* Create the dialog */
	GtkWidget* dialog = gtk_dialog_new_with_buttons(
		_("Preferences"), NULL,
		(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_REJECT,
		GTK_STOCK_OK,
		GTK_RESPONSE_ACCEPT,
		NULL
	);

	gtk_window_set_icon((GtkWindow*)dialog, gtk_widget_render_icon(dialog, GTK_STOCK_PREFERENCES, -1, NULL));
	gtk_window_set_resizable((GtkWindow*)dialog, FALSE);

	GtkWidget* notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area (GTK_DIALOG(dialog))), notebook, TRUE, TRUE, 2);

	GtkWidget* page_behavior = gtk_alignment_new(0.50, 0.50, 1.0, 1.0);
	gtk_alignment_set_padding((GtkAlignment*)page_behavior, 12, 6, 12, 6);
	gtk_notebook_append_page((GtkNotebook*)notebook, page_behavior,
		label_new_with_markup_and_mnemonic(_("_General")));
	GtkWidget* vbox_behavior = gtk_vbox_new(FALSE, 12);
	gtk_container_add((GtkContainer*)page_behavior, vbox_behavior);

	add_section(PREF_SECTION_CLIP, vbox_behavior);
	add_section(PREF_SECTION_HISTORY, vbox_behavior);
	add_section(PREF_SECTION_FILTERING, vbox_behavior);

	GtkWidget* page_display = gtk_alignment_new(0.50, 0.50, 1.0, 1.0);
	gtk_alignment_set_padding((GtkAlignment*)page_display, 12, 6, 12, 6);
	gtk_notebook_append_page((GtkNotebook*)notebook, page_display, label_new_with_markup_and_mnemonic(_("_Popup")));
	GtkWidget* vbox_display = gtk_vbox_new(FALSE, 12);
	gtk_container_add((GtkContainer*)page_display, vbox_display);

	add_section(PREF_SECTION_POPUP,vbox_display);
	
	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type((GtkFrame*)frame, GTK_SHADOW_NONE);
	label = gtk_label_new(NULL);
	gtk_label_set_markup((GtkLabel*)label, _("<b>Omitting</b>"));
	gtk_frame_set_label_widget((GtkFrame*)frame, label);
	alignment = gtk_alignment_new(0.50, 0.50, 1.0, 1.0);
	gtk_alignment_set_padding((GtkAlignment*)alignment, 12, 0, 12, 0);
	gtk_container_add((GtkContainer*)frame, alignment);
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_container_add((GtkContainer*)alignment, vbox);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start((GtkBox*)vbox, hbox, FALSE, FALSE, 0);
	p=get_pref("ellipsize");
	label = gtk_label_new(_(p->desc));
	gtk_misc_set_alignment((GtkMisc*)label, 0.0, 0.50);
	gtk_box_pack_start((GtkBox*)hbox, label, FALSE, FALSE, 0);
	p->w = gtk_combo_box_new_text();
	gtk_combo_box_append_text((GtkComboBox*)p->w, _("Beginning"));
	gtk_combo_box_append_text((GtkComboBox*)p->w, _("Middle"));
	gtk_combo_box_append_text((GtkComboBox*)p->w, _("End"));
	gtk_box_pack_start((GtkBox*)hbox, p->w, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox*)vbox_display, frame, FALSE, FALSE, 0);
	
	add_section(PREF_SECTION_MISC,vbox_display);

	GtkWidget* page_extras = gtk_alignment_new(0.50, 0.50, 1.0, 1.0);
	gtk_alignment_set_padding((GtkAlignment*)page_extras, 12, 6, 12, 6);
	gtk_notebook_append_page((GtkNotebook*)notebook, page_extras, label_new_with_markup_and_mnemonic(_("_Hotkeys")));
	GtkWidget* vbox_extras = gtk_vbox_new(FALSE, 12);
	gtk_container_add((GtkContainer*)page_extras, vbox_extras);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type((GtkFrame*)frame, GTK_SHADOW_NONE);
	label = gtk_label_new(NULL);
	gtk_label_set_markup((GtkLabel*)label, _("<b>Hotkeys</b>"));
	gtk_frame_set_label_widget((GtkFrame*)frame, label);
	alignment = gtk_alignment_new(0.50, 0.50, 1.0, 1.0);
	gtk_alignment_set_padding((GtkAlignment*)alignment, 12, 0, 12, 0);
	gtk_container_add((GtkContainer*)frame, alignment);
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_container_add((GtkContainer*)alignment, vbox);
	add_section(PREF_SECTION_HOTKEYS,vbox);
	gtk_box_pack_start((GtkBox*)vbox_extras,frame,FALSE,FALSE,0);

	update_pref_widgets();

	gtk_widget_show_all(dialog);
	gtk_notebook_set_current_page((GtkNotebook*)notebook, tab);
	if (gtk_dialog_run((GtkDialog*)dialog) == GTK_RESPONSE_ACCEPT)
	{
		/* Apply and save preferences */
		apply_preferences();
		save_preferences();
	}
	gtk_widget_destroy(dialog);
}

