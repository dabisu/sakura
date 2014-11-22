/*******************************************************************************
 *  Filename: sakura.c
 *  Description: VTE-based terminal emulator
 *
 *           Copyright (C) 2006-2012  David Gómez <david@pleyades.net>
 *           Copyright (C) 2008       Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *****************************************************************************/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <wchar.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <locale.h>
#include <libintl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <vte/vte.h>
#include <gdk/gdkx.h>

#define _(String) gettext(String)
#define N_(String) (String)
#define GETTEXT_PACKAGE "sakura"

#define SAY(format,...) do {\
	if (strcmp("Debug", BUILDTYPE)==0) {\
	    fprintf(stderr, "[%d] ", getpid());\
	    fprintf(stderr, "[%s] ", __FUNCTION__);\
	    if (format) fprintf(stderr, format, ##__VA_ARGS__);\
	    fputc('\n', stderr);\
		fflush(stderr);\
	}\
} while (0)

#define PALETTE_SIZE 16

/* 16 color palettes in GdkRGBA format (red, green, blue, alpha)  
 * Text displayed in the first 8 colors (0-7) is meek (uses thin strokes).
 * Text displayed in the second 8 colors (8-15) is bold (uses thick strokes). */

const GdkRGBA tango_palette[PALETTE_SIZE] = {
	{0,        0,        0,        1},
	{0.8,      0,        0,        1},
	{0.305882, 0.603922, 0.023529, 1},
	{0.768627, 0.627451, 0,        1},
	{0.203922, 0.396078, 0.643137, 1},
	{0.458824, 0.313725, 0.482353, 1},
	{0.0235294,0.596078, 0.603922, 1},
	{0.827451, 0.843137, 0.811765, 1},
	{0.333333, 0.341176, 0.32549,  1},
	{0.937255, 0.160784, 0.160784, 1},
	{0.541176, 0.886275, 0.203922, 1},
	{0.988235, 0.913725, 0.309804, 1},
	{0.447059, 0.623529, 0.811765, 1},
	{0.678431, 0.498039, 0.658824, 1},
	{0.203922, 0.886275, 0.886275, 1},
	{0.933333, 0.933333, 0.92549,  1}
};

const GdkRGBA linux_palette[PALETTE_SIZE] = {
	{0,        0,        0,        1},
	{0.666667, 0,        0,        1},
	{0,        0.666667, 0,        1},
	{0.666667, 0.333333, 0,        1},
	{0,        0,        0.666667, 1},
	{0.666667, 0,        0.666667, 1},
	{0,        0.666667, 0.666667, 1},
	{0.666667, 0.666667, 0.666667, 1},
	{0.333333, 0.333333, 0.333333, 1},
	{1,        0.333333, 0.333333, 1},
	{0.333333, 1,        0.333333, 1},
	{1,        1,        0.333333, 1},
	{0.333333, 0.333333, 1,        1},
	{1,        0.333333, 1,        1},
	{0.333333, 1,        1,        1},
	{1,        1,        1,        1}
};

const GdkRGBA solarized_dark_palette[PALETTE_SIZE] = {
	{0.027451, 0.211765, 0.258824, 1},
	{0.862745, 0.196078, 0.184314, 1},
	{0.521569, 0.600000, 0.000000, 1},
	{0.709804, 0.537255, 0.000000, 1},
	{0.149020, 0.545098, 0.823529, 1},
	{0.827451, 0.211765, 0.509804, 1},
	{0.164706, 0.631373, 0.596078, 1},
	{0.933333, 0.909804, 0.835294, 1},
	{0.000000, 0.168627, 0.211765, 1},
	{0.796078, 0.294118, 0.086275, 1},
	{0.345098, 0.431373, 0.458824, 1},
	{0.396078, 0.482353, 0.513725, 1},
	{0.513725, 0.580392, 0.588235, 1},
	{0.423529, 0.443137, 0.768627, 1},
	{0.576471, 0.631373, 0.631373, 1},
	{0.992157, 0.964706, 0.890196, 1}
#if 0
    { 0, 0x0707, 0x3636, 0x4242 }, // 0  base02 black (used as background color)
    { 0, 0xdcdc, 0x3232, 0x2f2f }, // 1  red
    { 0, 0x8585, 0x9999, 0x0000 }, // 2  green
    { 0, 0xb5b5, 0x8989, 0x0000 }, // 3  yellow
    { 0, 0x2626, 0x8b8b, 0xd2d2 }, // 4  blue
    { 0, 0xd3d3, 0x3636, 0x8282 }, // 5  magenta
    { 0, 0x2a2a, 0xa1a1, 0x9898 }, // 6  cyan
    { 0, 0xeeee, 0xe8e8, 0xd5d5 }, // 7  base2 white (used as foreground color)
    { 0, 0x0000, 0x2b2b, 0x3636 }, // 8  base3 bright black
    { 0, 0xcbcb, 0x4b4B, 0x1616 }, // 9  orange
    { 0, 0x5858, 0x6e6e, 0x7575 }, // 10 base01 bright green
    { 0, 0x6565, 0x7b7b, 0x8383 }, // 11 base00 bright yellow
    { 0, 0x8383, 0x9494, 0x9696 }, // 12 base0 brigth blue
    { 0, 0x6c6c, 0x7171, 0xc4c4 }, // 13 violet
    { 0, 0x9393, 0xa1a1, 0xa1a1 }, // 14 base1 cyan
    { 0, 0xfdfd, 0xf6f6, 0xe3e3 }  // 15 base3 white
#endif
};

const GdkRGBA solarized_light_palette[PALETTE_SIZE] = {
	{0.933333, 0.909804, 0.835294, 1},
	{0.862745, 0.196078, 0.184314, 1},
	{0.521569, 0.600000, 0.000000, 1},
	{0.709804, 0.537255, 0.000000, 1},
	{0.149020, 0.545098, 0.823529, 1},
	{0.827451, 0.211765, 0.509804, 1},
	{0.164706, 0.631373, 0.596078, 1},
	{0.027451, 0.211765, 0.258824, 1},
	{0.992157, 0.964706, 0.890196, 1},
	{0.796078, 0.294118, 0.086275, 1},
	{0.576471, 0.631373, 0.631373, 1},
	{0.513725, 0.580392, 0.588235, 1},
	{0.396078, 0.482353, 0.513725, 1},
	{0.423529, 0.443137, 0.768627, 1},
	{0.345098, 0.431373, 0.458824, 1},
	{0.000000, 0.168627, 0.211765, 1}
#if 0
	{ 0, 0xeeee, 0xe8e8, 0xd5d5 }, // 0 S_base2
	{ 0, 0xdcdc, 0x3232, 0x2f2f }, // 1 S_red
	{ 0, 0x8585, 0x9999, 0x0000 }, // 2 S_green
	{ 0, 0xb5b5, 0x8989, 0x0000 }, // 3 S_yellow
	{ 0, 0x2626, 0x8b8b, 0xd2d2 }, // 4 S_blue
	{ 0, 0xd3d3, 0x3636, 0x8282 }, // 5 S_magenta
	{ 0, 0x2a2a, 0xa1a1, 0x9898 }, // 6 S_cyan
	{ 0, 0x0707, 0x3636, 0x4242 }, // 7 S_base02
	{ 0, 0xfdfd, 0xf6f6, 0xe3e3 }, // 8 S_base3
	{ 0, 0xcbcb, 0x4b4B, 0x1616 }, // 9 S_orange
	{ 0, 0x9393, 0xa1a1, 0xa1a1 }, // 10 S_base1
	{ 0, 0x8383, 0x9494, 0x9696 }, // 11 S_base0
	{ 0, 0x6565, 0x7b7b, 0x8383 }, // 12 S_base00
	{ 0, 0x6c6c, 0x7171, 0xc4c4 }, // 13 S_violet
	{ 0, 0x5858, 0x6e6e, 0x7575 }, // 14 S_base01
	{ 0, 0x0000, 0x2b2b, 0x3636 } // 15 S_base03
#endif
};


const GdkRGBA xterm_palette[PALETTE_SIZE] = {
    {0,        0,        0,        1},
    {0.803922, 0,        0,        1},
    {0,        0.803922, 0,        1},
    {0.803922, 0.803922, 0,        1},
    {0.117647, 0.564706, 1,        1},
    {0.803922, 0,        0.803922, 1},
    {0,        0.803922, 0.803922, 1},
    {0.898039, 0.898039, 0.898039, 1},
    {0.298039, 0.298039, 0.298039, 1},
    {1,        0,        0,        1},
    {0,        1,        0,        1},
    {1,        1,        0,        1},
    {0.27451,  0.509804, 0.705882, 1},
    {1,        0,        1,        1},
    {0,        1,        1,        1},
    {1,        1,        1,        1}
};


#define CLOSE_BUTTON_CSS "* {\n"\
				"-GtkButton-default-border : 0;\n"\
				"-GtkButton-default-outside-border : 0;\n"\
				"-GtkButton-inner-border: 0;\n"\
				"-GtkWidget-focus-line-width : 0;\n"\
				"-GtkWidget-focus-padding : 0;\n"\
				"padding: 0;\n"\
				"}"

#define HIG_DIALOG_CSS "* {\n"\
				"-GtkDialog-action-area-border : 12;\n"\
				"-GtkDialog-button-spacing : 12;\n"\
				"}"

#define NUM_COLORSETS 6

static struct {
	GtkWidget *main_window;
	GtkWidget *notebook;
	GtkWidget *menu;
	PangoFontDescription *font;
	GdkRGBA forecolors[NUM_COLORSETS];
	GdkRGBA backcolors[NUM_COLORSETS];
	GdkRGBA curscolors[NUM_COLORSETS];
	const GdkRGBA *palette;
	bool has_rgba;				/* RGBA capabilities */
	char *current_match;
	guint width;
	guint height;
	glong columns;
	glong rows;
	gint label_count;
	VteTerminalCursorShape cursor_type;
	bool first_tab;
	bool show_scrollbar;
	bool show_resize_grip;
	bool show_closebutton;
	bool tabs_on_bottom;
	bool less_questions;
	bool audible_bell;
	bool visible_bell;
	bool blinking_cursor;
	bool allow_bold;
	bool fullscreen;
	bool keep_fc;				/* Global flag to indicate that we don't want changes in the files and columns values */
	bool config_modified;		/* Configuration has been modified */
	bool externally_modified;	/* Configuration file has been modified by another proccess */
	bool resized;
	GtkWidget *item_clear_background; /* We include here only the items which need to be hidden */
	GtkWidget *item_copy_link;
	GtkWidget *item_open_link;
	GtkWidget *open_link_separator;
	GKeyFile *cfg;
	GtkCssProvider *provider;
	char *configfile;
	char *background;
	char *word_chars;			/* Set of characters for word boundaries */
	gint last_colorset;
	gint add_tab_accelerator;
	gint del_tab_accelerator;
	gint switch_tab_accelerator;
	gint move_tab_accelerator;
	gint copy_accelerator;
	gint scrollbar_accelerator;
	gint open_url_accelerator;
	gint font_size_accelerator;
	gint set_tab_name_accelerator;
	gint set_colorset_accelerator;
	gint add_tab_key;
	gint del_tab_key;
	gint prev_tab_key;
	gint next_tab_key;
	gint copy_key;
	gint paste_key;
	gint scrollbar_key;
	gint set_tab_name_key;
	gint fullscreen_key;
	gint set_colorset_keys[NUM_COLORSETS];
	GRegex *http_regexp;
	char *argv[3];
} sakura;

struct terminal {
	GtkWidget *hbox;
	GtkWidget *vte;     /* Reference to VTE terminal */
	GPid pid;          /* pid of the forked proccess */
	GtkWidget *scrollbar;
	GtkWidget *label;
	gchar *label_text;
	bool label_set_byuser;
	GtkBorder *border;   /* inner-property data */
	int colorset;
};


#define ICON_FILE "terminal-tango.svg"
#define SCROLL_LINES 4096
#define HTTP_REGEXP "(ftp|http)s?://[-a-zA-Z0-9.?$%&/=_~#.,:;+]*"
#define DEFAULT_CONFIGFILE "sakura.conf"
#define DEFAULT_COLUMNS 80
#define DEFAULT_ROWS 24
#define DEFAULT_FONT "Ubuntu Mono,monospace 13"
#define FONT_MINIMAL_SIZE (PANGO_SCALE*6)
#define DEFAULT_WORD_CHARS  "-A-Za-z0-9,./?%&#_~"
#define DEFAULT_PALETTE "solarized_dark"
#define TAB_MAX_SIZE 40
#define TAB_MIN_SIZE 6
#define FORWARD 1
#define BACKWARDS 2
#define DEFAULT_ADD_TAB_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_DEL_TAB_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_SWITCH_TAB_ACCELERATOR  (GDK_MOD1_MASK)
#define DEFAULT_MOVE_TAB_ACCELERATOR (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_COPY_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_SCROLLBAR_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_OPEN_URL_ACCELERATOR (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_FONT_SIZE_ACCELERATOR (GDK_CONTROL_MASK)
#define DEFAULT_SET_TAB_NAME_ACCELERATOR (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_SELECT_COLORSET_ACCELERATOR (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_ADD_TAB_KEY  GDK_KEY_T
#define DEFAULT_DEL_TAB_KEY  GDK_KEY_W
#define DEFAULT_PREV_TAB_KEY  GDK_KEY_Left
#define DEFAULT_NEXT_TAB_KEY  GDK_KEY_Right
#define DEFAULT_COPY_KEY  GDK_KEY_C
#define DEFAULT_PASTE_KEY  GDK_KEY_V
#define DEFAULT_SCROLLBAR_KEY  GDK_KEY_S
#define DEFAULT_SET_TAB_NAME_KEY  GDK_KEY_N
#define DEFAULT_FULLSCREEN_KEY  GDK_KEY_F11
/* make this an array instead of #defines to get a compile time
 * error instead of a runtime if NUM_COLORSETS changes */
static int cs_keys[NUM_COLORSETS] = 
		{GDK_KEY_F1, GDK_KEY_F2, GDK_KEY_F3, GDK_KEY_F4, GDK_KEY_F5, GDK_KEY_F6};

#define ERROR_BUFFER_LENGTH 256
const char cfg_group[] = "sakura";

static GQuark term_data_id = 0;
#define  sakura_get_page_term( sakura, page_idx )  \
    (struct terminal*)g_object_get_qdata(  \
            G_OBJECT( gtk_notebook_get_nth_page( (GtkNotebook*)sakura.notebook, page_idx ) ), term_data_id);

#define  sakura_set_page_term( sakura, page_idx, term )  \
    g_object_set_qdata_full( \
            G_OBJECT( gtk_notebook_get_nth_page( (GtkNotebook*)sakura.notebook, page_idx) ), \
            term_data_id, term, (GDestroyNotify)g_free);

#define  sakura_set_config_integer(key, value) do {\
	g_key_file_set_integer(sakura.cfg, cfg_group, key, value);\
	sakura.config_modified=TRUE;\
	} while(0);

#define  sakura_set_config_string(key, value) do {\
	g_key_file_set_value(sakura.cfg, cfg_group, key, value);\
	sakura.config_modified=TRUE;\
	} while(0);

#define  sakura_set_config_boolean(key, value) do {\
	g_key_file_set_boolean(sakura.cfg, cfg_group, key, value);\
	sakura.config_modified=TRUE;\
	} while(0);


/* Callbacks */
static gboolean sakura_key_press (GtkWidget *, GdkEventKey *, gpointer);
static void     sakura_increase_font (GtkWidget *, void *);
static void     sakura_decrease_font (GtkWidget *, void *);
static void     sakura_child_exited (GtkWidget *, void *);
static void     sakura_eof (GtkWidget *, void *);
static void     sakura_title_changed (GtkWidget *, void *);
static gboolean sakura_delete_event (GtkWidget *, void *);
static void     sakura_destroy_window (GtkWidget *, void *);
static void     sakura_font_dialog (GtkWidget *, void *);
static void     sakura_set_name_dialog (GtkWidget *, void *);
static void     sakura_color_dialog (GtkWidget *, void *);
static void     sakura_set_title_dialog (GtkWidget *, void *);
static void     sakura_select_background_dialog (GtkWidget *, void *);
static void     sakura_new_tab (GtkWidget *, void *);
static void     sakura_close_tab (GtkWidget *, void *);
static void     sakura_fullscreen (GtkWidget *, void *);
static void     sakura_open_url (GtkWidget *, void *);
static void     sakura_clear (GtkWidget *, void *);
static gboolean sakura_resized_window(GtkWidget *, GdkEventConfigure *, void *);
static void     sakura_setname_entry_changed(GtkWidget *, void *);
static void     sakura_copy(GtkWidget *, void *);
static void     sakura_paste(GtkWidget *, void *);
static void     sakura_show_first_tab (GtkWidget *widget, void *data);
static void     sakura_tabs_on_bottom (GtkWidget *widget, void *data);
static void     sakura_less_questions (GtkWidget *widget, void *data);
static void     sakura_show_close_button (GtkWidget *widget, void *data);
static void     sakura_show_scrollbar(GtkWidget *, void *);
static void     sakura_show_resize_grip(GtkWidget *, void *);
static void     sakura_closebutton_clicked(GtkWidget *, void *);
static void     sakura_conf_changed(GtkWidget *, void *);
static void     sakura_window_show_event(GtkWidget *, gpointer);

/* Misc */
static void     sakura_error(const char *, ...);

/* Functions */
static void     sakura_init();
static void     sakura_init_popup();
static void     sakura_destroy();
static void     sakura_add_tab();
static void     sakura_del_tab();
static void     sakura_move_tab(gint);
static gint     sakura_find_tab(VteTerminal *);
static void     sakura_set_font();
static void     sakura_set_tab_label_text(const gchar *, gint page);
static void     sakura_set_size(void);
static void     sakura_set_bgimage();
static void     sakura_set_config_key(const gchar *, guint);
static guint    sakura_get_config_key(const gchar *);
static void     sakura_config_done();
static void     sakura_set_colorset (int);
static void     sakura_set_colors (void);

static const char *option_font;
static const char *option_workdir;
static const char *option_execute;
static gchar **option_xterm_args;
static gboolean option_xterm_execute=FALSE;
static gboolean option_version=FALSE;
static gint option_ntabs=1;
static gint option_login = FALSE;
static const char *option_title;
static int option_rows, option_columns;
static gboolean option_hold=FALSE;
static const char *option_geometry;
static char *option_config_file;
static gboolean option_fullscreen;
static gboolean option_maximize;

static GOptionEntry entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &option_version, N_("Print version number"), NULL },
	{ "font", 'f', 0, G_OPTION_ARG_STRING, &option_font, N_("Select initial terminal font"), NULL },
	{ "ntabs", 'n', 0, G_OPTION_ARG_INT, &option_ntabs, N_("Select initial number of tabs"), NULL },
	{ "working-directory", 'd', 0, G_OPTION_ARG_STRING, &option_workdir, N_("Set working directory"), NULL },
	{ "execute", 'x', 0, G_OPTION_ARG_STRING, &option_execute, N_("Execute command"), NULL },
	{ "xterm-execute", 'e', 0, G_OPTION_ARG_NONE, &option_xterm_execute, N_("Execute command (last option in the command line)"), NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &option_xterm_args, NULL, NULL },
	{ "login", 'l', 0, G_OPTION_ARG_NONE, &option_login, N_("Login shell"), NULL },
	{ "title", 't', 0, G_OPTION_ARG_STRING, &option_title, N_("Set window title"), NULL },
	{ "columns", 'c', 0, G_OPTION_ARG_INT, &option_columns, N_("Set columns number"), NULL },
	{ "rows", 'r', 0, G_OPTION_ARG_INT, &option_rows, N_("Set rows number"), NULL },
	{ "hold", 'h', 0, G_OPTION_ARG_NONE, &option_hold, N_("Hold window after execute command"), NULL },
	{ "maximize", 'm', 0, G_OPTION_ARG_NONE, &option_maximize, N_("Maximize window"), NULL },
	{ "fullscreen", 's', 0, G_OPTION_ARG_NONE, &option_fullscreen, N_("Fullscreen mode"), NULL },
	{ "geometry", 0, 0, G_OPTION_ARG_STRING, &option_geometry, N_("X geometry specification"), NULL },
	{ "config-file", 0, 0, G_OPTION_ARG_FILENAME, &option_config_file, N_("Use alternate configuration file"), NULL },
	{ NULL }
};


static
gboolean sakura_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (event->type!=GDK_KEY_PRESS) return FALSE;

	unsigned int topage = 0;

	gint npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	guint keyval = event->keyval;

	/* Check if Caps lock is enabled. If it is, change keyval to make keybindings work with
	   both lowercase and uppercase letters */
	if (gdk_keymap_get_caps_lock_state(gdk_keymap_get_default())) {
		keyval = gdk_keyval_to_upper(keyval);
	}

	/* add_tab_accelerator + T or del_tab_accelerator + W pressed */
	if ( (event->state & sakura.add_tab_accelerator)==sakura.add_tab_accelerator &&
			keyval==sakura.add_tab_key) {
		sakura_add_tab();
		return TRUE;
	} else if ( (event->state & sakura.del_tab_accelerator)==sakura.del_tab_accelerator &&
			keyval==sakura.del_tab_key ) {
		/* Delete current tab */
		sakura_close_tab(NULL, NULL);
		return TRUE;
	}

	/* switch_tab_accelerator + number pressed / switch_tab_accelerator + (left/right) cursor pressed */
	if ( (event->state & sakura.switch_tab_accelerator) == sakura.switch_tab_accelerator ) {
		if ((keyval>=GDK_KEY_1) && (keyval<=GDK_KEY_9) && (keyval<=GDK_KEY_1-1+npages)
				&& (keyval!=GDK_KEY_1+gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook)))) {
			switch(keyval) {
				case GDK_KEY_1: topage=0; break;
				case GDK_KEY_2: topage=1; break;
				case GDK_KEY_3: topage=2; break;
				case GDK_KEY_4: topage=3; break;
				case GDK_KEY_5: topage=4; break;
				case GDK_KEY_6: topage=5; break;
				case GDK_KEY_7: topage=6; break;
				case GDK_KEY_8: topage=7; break;
				case GDK_KEY_9: topage=8; break;
			}
			if (topage <= npages)
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), topage);
			return TRUE;
		} else if (keyval==sakura.prev_tab_key) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==0) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), npages-1);
			} else {
				gtk_notebook_prev_page(GTK_NOTEBOOK(sakura.notebook));
			}
			return TRUE;
		} else if (keyval==sakura.next_tab_key) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==(npages-1)) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), 0);
			} else {
				gtk_notebook_next_page(GTK_NOTEBOOK(sakura.notebook));
			}
			return TRUE;
		}
	}

	/* move_tab_accelerator + (left/right) cursor pressed */
	if ( (event->state & sakura.move_tab_accelerator)==sakura.move_tab_accelerator ) {
		if (keyval==sakura.prev_tab_key) {
			sakura_move_tab(BACKWARDS);
			return TRUE;
		} else if (keyval==sakura.next_tab_key) {
			sakura_move_tab(FORWARD);
			return TRUE;
		}
	}

	/* copy_accelerator-[C/V] pressed */
	//SAY("copy acc: %d", sakura.copy_accelerator);
	//SAY("ev+copy: %d", (event->state & sakura.copy_accelerator));
	if ( (event->state & sakura.copy_accelerator)==sakura.copy_accelerator ) {
		//SAY("%d %d", keyval, sakura.copy_key);
		if (keyval==sakura.copy_key) {
			sakura_copy(NULL, NULL);
			return TRUE;
		} else if (keyval==sakura.paste_key) {
			sakura_paste(NULL, NULL);
			return TRUE;
		}
	}

	/* scrollbar_accelerator-[S] pressed */
	if ( (event->state & sakura.scrollbar_accelerator)==sakura.scrollbar_accelerator ) {
		if (keyval==sakura.scrollbar_key) {
			sakura_show_scrollbar(NULL, NULL);
			return TRUE;
		}
	}

	/* set_tab_name_accelerator-[N] pressed */
	if ( (event->state & sakura.set_tab_name_accelerator)==sakura.set_tab_name_accelerator ) {
		if (keyval==sakura.set_tab_name_key) {
			sakura_set_name_dialog(NULL, NULL);
			return TRUE;
		}
	}

	/* font_size_accelerator-[+] or [-] pressed */
	if ( (event->state & sakura.font_size_accelerator)==sakura.font_size_accelerator ) {
		if (keyval==GDK_KEY_plus) {
			sakura_increase_font(NULL, NULL);
			return TRUE;
		} else if (keyval==GDK_KEY_minus) {
			sakura_decrease_font(NULL, NULL);
			return TRUE;
		}
	}

	/* F11 (fullscreen) pressed */
	if (keyval==sakura.fullscreen_key){
		sakura_fullscreen(NULL, NULL);
		return TRUE;
	}

	/* Change in colorset */
	if ( (event->state & sakura.set_colorset_accelerator)==sakura.set_colorset_accelerator ) {
		int i;
		for(i=0; i<NUM_COLORSETS; i++) {
			if (keyval==sakura.set_colorset_keys[i]){
				sakura_set_colorset(i);
				return TRUE;
			}
		}
	}
	return FALSE;
}


static gboolean
sakura_button_press(GtkWidget *widget, GdkEventButton *button_event, gpointer user_data)
{
	struct terminal *term;
	glong column, row;
	gint page, tag;

	if (button_event->type != GDK_BUTTON_PRESS)
		return FALSE;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	/* Find out if cursor it's over a matched expression...*/

	/* Get the column and row relative to pointer position */
	column = ((glong) (button_event->x) / vte_terminal_get_char_width(
			VTE_TERMINAL(term->vte)));
	row = ((glong) (button_event->y) / vte_terminal_get_char_height(
			VTE_TERMINAL(term->vte)));
	sakura.current_match = vte_terminal_match_check(VTE_TERMINAL(term->vte), column, row, &tag);

	/* Left button: open the URL if any */
	if (button_event->button == 1 &&
	    ((button_event->state & sakura.open_url_accelerator) == sakura.open_url_accelerator)
	    && sakura.current_match) {

		sakura_open_url(NULL, NULL);

		return TRUE;
	}

	/* Right button: show the popup menu */
	if (button_event->button == 3) {
		GtkMenu *menu;
		menu = GTK_MENU (widget);

		if (sakura.current_match) {
			/* Show the extra options in the menu */
			gtk_widget_show(sakura.item_open_link);
			gtk_widget_show(sakura.item_copy_link);
			gtk_widget_show(sakura.open_link_separator);
		} else {
			/* Hide all the options */
			gtk_widget_hide(sakura.item_open_link);
			gtk_widget_hide(sakura.item_copy_link);
			gtk_widget_hide(sakura.open_link_separator);
		}

		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button_event->button, button_event->time);

		return TRUE;
	}

	return FALSE;
}


static void
sakura_page_removed (GtkWidget *widget, void *data)
{
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==1) {
		/* If the first tab is disabled, window size changes and we need
		 * to recalculate its size */
		sakura_set_size();
	}
}


static void
sakura_increase_font (GtkWidget *widget, void *data)
{
	gint new_size;

	/* Increment font size one unit */
	new_size=pango_font_description_get_size(sakura.font)+PANGO_SCALE;

	pango_font_description_set_size(sakura.font, new_size);
	sakura_set_font();
	sakura_set_size();
	sakura_set_config_string("font", pango_font_description_to_string(sakura.font));
}


static void
sakura_decrease_font (GtkWidget *widget, void *data)
{
	gint new_size;

	/* Decrement font size one unit */
	new_size=pango_font_description_get_size(sakura.font)-PANGO_SCALE;
	
	/* Set a minimal size */
	if (new_size >= FONT_MINIMAL_SIZE ) {
		pango_font_description_set_size(sakura.font, new_size);
		sakura_set_font();
		sakura_set_size();
		sakura_set_config_string("font", pango_font_description_to_string(sakura.font));
	}
}


static void
sakura_child_exited (GtkWidget *widget, void *data)
{
	gint status, page, npages;
	struct terminal *term;

	page = gtk_notebook_page_num(GTK_NOTEBOOK(sakura.notebook),
				gtk_widget_get_parent(widget));
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	/* Only write configuration to disk if it's the last tab */
	if (npages==1) {
		sakura_config_done();
	}

	if (option_hold==TRUE) {
		SAY("hold option has been activated");
		return;
	}

	SAY("waiting for terminal pid %d", term->pid);

	waitpid(term->pid, &status, WNOHANG);
	/* TODO: check wait return */

	sakura_del_tab(page);

	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	if (npages==0)
		sakura_destroy();
}


static void
sakura_eof (GtkWidget *widget, void *data)
{
	gint status, npages;
	struct terminal *term;

	SAY("Got EOF signal");

	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Only write configuration to disk if it's the last tab */
	if (npages==1) {
		sakura_config_done();
	}

	/* Workaround for libvte strange behaviour. There is not child-exited signal for
	   the last terminal, so we need to kill it here.  Check with libvte authors about
	   child-exited/eof signals */
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==0) {

		term = sakura_get_page_term(sakura, 0);

		if (option_hold==TRUE) {
			SAY("hold option has been activated");
			return;
		}

        SAY("waiting for terminal pid (in eof) %d", term->pid);

        waitpid(term->pid, &status, WNOHANG);
		/* TODO: check wait return */

		sakura_del_tab(0);

		npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
		if (npages==0)
			sakura_destroy();
	}
}

/* This handler is called when window title changes, and is used to change window and notebook pages titles */
static void
sakura_title_changed (GtkWidget *widget, void *data)
{
	struct terminal *term;
	const char *title;
	gint n_pages;
	gint modified_page;
	VteTerminal *vte_term=(VteTerminal *)widget;

	modified_page = sakura_find_tab(vte_term);
	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, modified_page);

	title = vte_terminal_get_window_title(VTE_TERMINAL(term->vte));

	/* User set values overrides any other one, but title should be changed */
	if (!term->label_set_byuser) 
		sakura_set_tab_label_text(title, modified_page);

	/* FIXME: Check if title has been set by the user */
	if (n_pages==1) {
		/* Beware: It doesn't work in Unity because there is a Compiz bug: #257391 */
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), title);
	} else 
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), "sakura");

}


/* Save configuration */
static void
sakura_config_done()
{
	GError *gerror = NULL;
	gsize len = 0;

	gchar *cfgdata = g_key_file_to_data(sakura.cfg, &len, &gerror);
	if (!cfgdata) {
		fprintf(stderr, "%s\n", gerror->message);
		exit(EXIT_FAILURE);
	}
	/* Write to file IF there's been changes */
	if (sakura.config_modified) {

		bool overwrite=true;

		if (sakura.externally_modified) {
			GtkWidget *dialog;
			gint response;

			dialog=gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
					GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
					_("Configuration has been modified by another proccess. Overwrite?"));

			response=gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);

			if (response==GTK_RESPONSE_YES) {
				overwrite=true;
			} else
				overwrite=false;
		}

		if (overwrite) {
			GIOChannel *cfgfile = g_io_channel_new_file(sakura.configfile, "w", &gerror);
			if (!cfgfile) {
				fprintf(stderr, "%s\n", gerror->message);
				exit(EXIT_FAILURE);
			}

			/* FIXME: if the number of chars written is not "len", something happened.
			 * Check for errors appropriately...*/
			GIOStatus status = g_io_channel_write_chars(cfgfile, cfgdata, len, NULL, &gerror);
			if (status != G_IO_STATUS_NORMAL) {
				// FIXME: we should deal with temporary failures (G_IO_STATUS_AGAIN)
				fprintf(stderr, "%s\n", gerror->message);
				exit(EXIT_FAILURE);
			}
			g_io_channel_shutdown(cfgfile, TRUE, &gerror);
			g_io_channel_unref(cfgfile);
		}
	}
}


static gboolean
sakura_delete_event (GtkWidget *widget, void *data)
{
	struct terminal *term;
	GtkWidget *dialog;
	gint response;
	gint npages;
	gint i;
	pid_t pgid;

	if (!sakura.less_questions) {
		npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

		/* Check for each tab if there are running processes. Use tcgetpgrp to compare to the shell PGID */
		for (i=0; i < npages; i++) {

			term = sakura_get_page_term(sakura, i);
			pgid = tcgetpgrp(vte_terminal_get_pty(VTE_TERMINAL(term->vte)));

			/* If running processes are found, we ask one time and exit */
			if ( (pgid != -1) && (pgid != term->pid)) {
				dialog=gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
											  GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
											  _("There are running processes.\n\nDo you really want to close Sakura?"));

				response=gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);

				if (response==GTK_RESPONSE_YES) {
					sakura_config_done();
					return FALSE;
				} else {
					return TRUE;
				}
			} 
		
		}
	}

	sakura_config_done();
	return FALSE;
}


static void
sakura_destroy_window (GtkWidget *widget, void *data)
{
	sakura_destroy();
}


static void
sakura_window_show_event(GtkWidget *widget, gpointer data)
{
	// set size when the window is first shown
	sakura_set_size();
}


static void
sakura_font_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *font_dialog;
	gint response;

	font_dialog=gtk_font_chooser_dialog_new(_("Select font"), GTK_WINDOW(sakura.main_window));
	gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(font_dialog), sakura.font);

	response=gtk_dialog_run(GTK_DIALOG(font_dialog));

	if (response==GTK_RESPONSE_OK) {
		pango_font_description_free(sakura.font);
		sakura.font=gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(font_dialog));
		sakura_set_font();
		sakura_set_size();
		sakura_set_config_string("font", pango_font_description_to_string(sakura.font));
	}

	gtk_widget_destroy(font_dialog);
}


static void
sakura_set_name_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *input_dialog;
	GtkWidget *entry, *label;
	GtkWidget *name_hbox; /* We need this for correct spacing */
	gint response;
	gint page;
	struct terminal *term;
	const gchar *text;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	input_dialog=gtk_dialog_new_with_buttons(_("Set tab name"), GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
	                                         _("_Cancel"), GTK_RESPONSE_REJECT,
	                                         _("_Apply"), GTK_RESPONSE_ACCEPT, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(input_dialog), TRUE);

	/* Set style */
	gchar *css = g_strdup_printf (HIG_DIALOG_CSS);
	gtk_css_provider_load_from_data(sakura.provider, css, -1, NULL);
	GtkStyleContext *context = gtk_widget_get_style_context (input_dialog);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (sakura.provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);

	name_hbox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	entry=gtk_entry_new();
	label=gtk_label_new(_("New text"));
	/* Set tab label as entry default text (when first tab is not displayed, get_tab_label_text
	   returns a null value, so check accordingly */
	text = gtk_notebook_get_tab_label_text(GTK_NOTEBOOK(sakura.notebook), term->hbox);
	if (text) {
		gtk_entry_set_text(GTK_ENTRY(entry), text);
	}
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(name_hbox), label, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(name_hbox), entry, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(input_dialog))), name_hbox, FALSE, FALSE, 12);
	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(sakura_setname_entry_changed), input_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT, FALSE);

	gtk_widget_show_all(name_hbox);

	response=gtk_dialog_run(GTK_DIALOG(input_dialog));
	if (response==GTK_RESPONSE_ACCEPT) {
		sakura_set_tab_label_text(gtk_entry_get_text(GTK_ENTRY(entry)), page);
		term->label_set_byuser=true;
	}
	gtk_widget_destroy(input_dialog);
}

static void 
sakura_set_colorset (int cs)
{
	gint page;
	struct terminal *term;

	if (cs<0 || cs>= NUM_COLORSETS)
		return;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);	
	term->colorset=cs;

	sakura_set_colors();
}

static void 
sakura_set_colors ()
{
	int i;
	int n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	struct terminal *term;
	GdkRGBA white={255, 255, 255, 1};

	/* Re-apply in each notebook tab its terminals colors */
	for (i = (n_pages - 1); i >= 0; i--) {
		term = sakura_get_page_term(sakura, i);
		if (sakura.has_rgba) {
			/* FIXME: Is this still needed with RGBA colors?? */
			/* This is needed for set_opacity to have effect. The opacity does
			   take effect when switching tabs, so this setting to white is 
			   actually needed only in the shown tab.*/
			vte_terminal_set_color_background_rgba(VTE_TERMINAL (term->vte), &white);
			vte_terminal_set_opacity(VTE_TERMINAL (term->vte), (int)((sakura.backcolors[term->colorset].alpha)*65535));
		}
		vte_terminal_set_colors_rgba(VTE_TERMINAL(term->vte), 
		                        &sakura.forecolors[term->colorset], 
		                        &sakura.backcolors[term->colorset],
		                        sakura.palette, PALETTE_SIZE);
		vte_terminal_set_color_cursor_rgba(VTE_TERMINAL(term->vte), &sakura.curscolors[term->colorset]);
	}

}

/* Callback from the color change dialog. Updates the contents of that
 * dialog, passed as 'data' from user input. */
static void
sakura_color_dialog_changed( GtkWidget *widget, void *data)
{
	int selected=-1;
	GtkDialog *dialog = (GtkDialog*)data;
	GtkColorButton *fore_button = g_object_get_data (G_OBJECT(dialog), "buttonfore");
	GtkColorButton *back_button = g_object_get_data (G_OBJECT(dialog), "buttonback");
	GtkColorButton *curs_button = g_object_get_data (G_OBJECT(dialog), "buttoncurs");
	GtkComboBox *set = g_object_get_data (G_OBJECT(dialog), "set_combo");
	GtkSpinButton *opacity_spin = g_object_get_data( G_OBJECT(dialog), "opacity_spin");
	GdkRGBA *temp_fore_colors = g_object_get_data( G_OBJECT(dialog), "fore");
	GdkRGBA *temp_back_colors = g_object_get_data( G_OBJECT(dialog), "back");
	GdkRGBA *temp_curs_colors = g_object_get_data( G_OBJECT(dialog), "curs");
	selected = gtk_combo_box_get_active( set );

	/* if we come here as a result of a change in the active colorset,
	 * load the new colorset to the buttons.
	 * Else, the colorselect buttons or opacity spin have gotten a new
	 * value, store that. */
	if( (GtkWidget*)set == widget ) {
		/* Spin opacity is a percentage, convert it*/
		gint new_opacity=(int)(temp_back_colors[selected].alpha*100);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(fore_button), &temp_fore_colors[selected]);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(back_button), &temp_back_colors[selected]);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(curs_button), &temp_curs_colors[selected]);
		gtk_spin_button_set_value(opacity_spin, new_opacity);	
	} else {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(fore_button), &temp_fore_colors[selected]);
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(back_button), &temp_back_colors[selected]);
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(curs_button), &temp_curs_colors[selected]);
		gtk_spin_button_update(opacity_spin);
		temp_back_colors[selected].alpha=gtk_spin_button_get_value(opacity_spin)/100;
	}

}


static void
sakura_color_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *color_dialog;
	GtkWidget *label1, *label2, *label3, *set_label, *opacity_label;
	GtkWidget *buttonfore, *buttonback, *buttoncurs, *set_combo, *opacity_spin;
	GtkAdjustment *spinner_adj;
	GtkWidget *hbox_fore, *hbox_back, *hbox_curs, *hbox_sets, *hbox_opacity;
	gint response;
	struct terminal *term;
	gint page;
	int cs;
	int i;
	gchar combo_text[3];
	GdkRGBA temp_fore[NUM_COLORSETS];
	GdkRGBA temp_back[NUM_COLORSETS];
	GdkRGBA temp_curs[NUM_COLORSETS];
	
	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	color_dialog=gtk_dialog_new_with_buttons(_("Select color"), GTK_WINDOW(sakura.main_window),
	                                                            GTK_DIALOG_MODAL,
	                                                            _("_Cancel"), GTK_RESPONSE_REJECT,
	                                                            _("_Select"), GTK_RESPONSE_ACCEPT, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(color_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(color_dialog), TRUE);
	/* Set style */
	gchar *css = g_strdup_printf (HIG_DIALOG_CSS);
	gtk_css_provider_load_from_data(sakura.provider, css, -1, NULL);
	GtkStyleContext *context = gtk_widget_get_style_context (color_dialog);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (sakura.provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);


	/* Add the drop-down combobox that selects current colorset to edit.
	 * TODO: preset the current colorset */
	hbox_sets=gtk_box_new(FALSE, 12);
	set_label=gtk_label_new(_("Colourset to edit"));
	set_combo=gtk_combo_box_text_new();
	for(cs=0; cs<NUM_COLORSETS; cs++){
		g_snprintf(combo_text, 2, "%d", cs+1);	
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(set_combo), NULL, combo_text);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(set_combo), term->colorset);

	hbox_fore=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	hbox_back=gtk_box_new(FALSE, 12);
	hbox_curs=gtk_box_new(FALSE, 12);
	label1=gtk_label_new(_("Select foreground color:"));
	label2=gtk_label_new(_("Select background color:"));
	label3=gtk_label_new(_("Select cursor color:"));
	buttonfore=gtk_color_button_new_with_rgba(&sakura.forecolors[term->colorset]);
	buttonback=gtk_color_button_new_with_rgba(&sakura.backcolors[term->colorset]);
	buttoncurs=gtk_color_button_new_with_rgba(&sakura.curscolors[term->colorset]);

	/* Opacity control */
	hbox_opacity=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12); 
	spinner_adj = gtk_adjustment_new ((sakura.backcolors[term->colorset].alpha)*100, 0.0, 99.0, 1.0, 5.0, 0);
	opacity_spin = gtk_spin_button_new(GTK_ADJUSTMENT(spinner_adj), 1.0, 0);
	opacity_label = gtk_label_new(_("Opacity level (%):"));
	gtk_box_pack_start(GTK_BOX(hbox_opacity), opacity_label, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(hbox_opacity), opacity_spin, FALSE, FALSE, 12);

	gtk_box_pack_start(GTK_BOX(hbox_fore), label1, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(hbox_fore), buttonfore, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox_back), label2, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(hbox_back), buttonback, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox_curs), label3, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(hbox_curs), buttoncurs, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox_sets), set_label, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(hbox_sets), set_combo, FALSE, FALSE, 12);
	

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), hbox_sets, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), hbox_fore, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), hbox_back, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), hbox_curs, FALSE, FALSE, 6);
	gtk_box_pack_end(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), hbox_opacity, FALSE, FALSE, 6);

	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog)));

	/* When user switches the colorset to change, the callback needs access
	 * to these selector widgets */
	g_object_set_data(G_OBJECT(color_dialog), "set_combo", set_combo);
	g_object_set_data(G_OBJECT(color_dialog), "buttonfore", buttonfore);
	g_object_set_data(G_OBJECT(color_dialog), "buttonback", buttonback);
	g_object_set_data(G_OBJECT(color_dialog), "buttoncurs", buttoncurs);
	g_object_set_data(G_OBJECT(color_dialog), "opacity_spin", opacity_spin);
	g_object_set_data(G_OBJECT(color_dialog), "fore", temp_fore);
	g_object_set_data(G_OBJECT(color_dialog), "back", temp_back);
	g_object_set_data(G_OBJECT(color_dialog), "curs", temp_curs);

	g_signal_connect(G_OBJECT(buttonfore), "color-set", 
	                 G_CALLBACK(sakura_color_dialog_changed), color_dialog );
	g_signal_connect(G_OBJECT(buttonback), "color-set", 
	                 G_CALLBACK(sakura_color_dialog_changed), color_dialog );
	g_signal_connect(G_OBJECT(buttoncurs), "color-set", 
	                 G_CALLBACK(sakura_color_dialog_changed), color_dialog );
	g_signal_connect(G_OBJECT(set_combo), "changed", 
	                 G_CALLBACK(sakura_color_dialog_changed), color_dialog );
	g_signal_connect(G_OBJECT(opacity_spin), "changed", 
	                 G_CALLBACK(sakura_color_dialog_changed), color_dialog );

	for(i=0; i<NUM_COLORSETS; i++) {
		temp_fore[i] = sakura.forecolors[i];
		temp_back[i] = sakura.backcolors[i];
		temp_curs[i] = sakura.curscolors[i];
	}

	response=gtk_dialog_run(GTK_DIALOG(color_dialog));
	

	if (response==GTK_RESPONSE_ACCEPT) {
		/* Save all colorsets to both the global struct and configuration.*/
		for( i=0; i<NUM_COLORSETS; i++) {
			char name[20]; 
			gchar *cfgtmp;
			
			sakura.forecolors[i]=temp_fore[i];
			sakura.backcolors[i]=temp_back[i];
			sakura.curscolors[i]=temp_curs[i];
			
			sprintf(name, "colorset%d_fore", i+1);
			cfgtmp=gdk_rgba_to_string(&sakura.forecolors[i]);
			sakura_set_config_string(name, cfgtmp);
			g_free(cfgtmp);

			sprintf(name, "colorset%d_back", i+1);
			cfgtmp=gdk_rgba_to_string(&sakura.backcolors[i]);
			sakura_set_config_string(name, cfgtmp);
			g_free(cfgtmp);

			sprintf(name, "colorset%d_curs", i+1);
			cfgtmp=gdk_rgba_to_string(&sakura.curscolors[i]);
			sakura_set_config_string(name, cfgtmp);
			g_free(cfgtmp);
		}

		/* Apply the new colorsets to all tabs
		 * Set the current tab's colorset to the last selected one in the dialog.
		 * This is probably what the new user expects, and the experienced user
		 * hopefully will not mind. */
		term->colorset = gtk_combo_box_get_active(GTK_COMBO_BOX(set_combo));
		sakura_set_config_integer("last_colorset", term->colorset+1);
		sakura_set_colors();
	}

	gtk_widget_destroy(color_dialog);
}


static void
sakura_set_title_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *title_dialog;
	GtkWidget *entry, *label;
	GtkWidget *title_hbox;
	gint response;

	title_dialog=gtk_dialog_new_with_buttons(_("Set window title"), GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
	                                         _("_Cancel"), GTK_RESPONSE_REJECT,
	                                         _("_Apply"), GTK_RESPONSE_ACCEPT, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(title_dialog), TRUE);
	/* Set style */
	gchar *css = g_strdup_printf (HIG_DIALOG_CSS);
	gtk_css_provider_load_from_data(sakura.provider, css, -1, NULL);
	GtkStyleContext *context = gtk_widget_get_style_context (title_dialog);
	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (sakura.provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);

	entry=gtk_entry_new();
	label=gtk_label_new(_("New window title"));
	title_hbox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	/* Set window label as entry default text */
	gtk_entry_set_text(GTK_ENTRY(entry), gtk_window_get_title(GTK_WINDOW(sakura.main_window)));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(title_hbox), label, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(title_hbox), entry, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(title_dialog))), title_hbox, FALSE, FALSE, 12);
	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(sakura_setname_entry_changed), title_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT, FALSE);

	gtk_widget_show_all(title_hbox);

	response=gtk_dialog_run(GTK_DIALOG(title_dialog));
	if (response==GTK_RESPONSE_ACCEPT) {
		/* Bug #257391 shadow reachs here too... */
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	gtk_widget_destroy(title_dialog);

}


static void
sakura_select_background_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *dialog;
	gint response;
	gchar *filename;

	dialog = gtk_file_chooser_dialog_new (_("Select a background file"), GTK_WINDOW(sakura.main_window),
	                                                                     GTK_FILE_CHOOSER_ACTION_OPEN,
	                                                                     _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                                                     _("_Open"), GTK_RESPONSE_ACCEPT,
	                                                                     NULL);


	response=gtk_dialog_run(GTK_DIALOG(dialog));
	if (response == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		sakura.background=g_strdup(filename);
		sakura_set_bgimage(sakura.background);
		gtk_widget_show(sakura.item_clear_background);
		g_free(filename);
	}

	gtk_widget_destroy(dialog);
}


static void
sakura_copy_url (GtkWidget *widget, void *data)
{
	GtkClipboard* clip;

	clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clip, sakura.current_match, -1 );
	clip = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clip, sakura.current_match, -1 );

}


static void
sakura_open_url (GtkWidget *widget, void *data)
{
	GError *error=NULL;
	gchar *cmd;
	gchar *browser=NULL;

	browser=(gchar *)g_getenv("BROWSER");

	if (browser) {
		cmd=g_strdup_printf("%s %s", browser, sakura.current_match);
	} else {
		if ( (browser = g_find_program_in_path("xdg-open")) ) {
			cmd=g_strdup_printf("%s %s", browser, sakura.current_match);
			g_free(browser);
		} else
			cmd=g_strdup_printf("firefox %s", sakura.current_match);
	}

	if (!g_spawn_command_line_async(cmd, &error)) {
		sakura_error("Couldn't exec \"%s\": %s", cmd, error->message);
	}

	g_free(cmd);
}


static void
sakura_clear (GtkWidget *widget, void *data)
{
	gint page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	gtk_widget_hide(sakura.item_clear_background);

	vte_terminal_set_background_image(VTE_TERMINAL(term->vte), NULL);

	sakura_set_config_string("background", "none");

	g_free(sakura.background);
	sakura.background=NULL;
}


static void
sakura_show_first_tab (GtkWidget *widget, void *data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		sakura_set_config_string("show_always_first_tab", "Yes");
		sakura.first_tab = true;
	} else {
		/* Only hide tabs if the notebook has one page */
		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 1) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}
		sakura_set_config_string("show_always_first_tab", "No");
		sakura.first_tab = false;
	}
	sakura_set_size();
}

static void
sakura_tabs_on_bottom (GtkWidget *widget, void *data)
{

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(sakura.notebook), GTK_POS_BOTTOM);
		sakura_set_config_boolean("tabs_on_bottom", TRUE);
	} else {
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(sakura.notebook), GTK_POS_TOP);
		sakura_set_config_boolean("tabs_on_bottom", FALSE);
	}
}

static void
sakura_less_questions (GtkWidget *widget, void *data)
{

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		sakura.less_questions=TRUE;
		sakura_set_config_boolean("less_questions", TRUE);
	} else {
		sakura.less_questions=FALSE;
		sakura_set_config_boolean("less_questions", FALSE);
	}
}

static void
sakura_show_close_button (GtkWidget *widget, void *data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		sakura_set_config_boolean("closebutton", TRUE);
	} else {
		sakura_set_config_boolean("closebutton", FALSE);
	}
}

static void
sakura_show_resize_grip (GtkWidget *widget, void *data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		sakura_set_config_boolean("resize_grip", TRUE);
		gtk_window_set_has_resize_grip(GTK_WINDOW(sakura.main_window), TRUE);
	} else {
		sakura_set_config_boolean("resize_grip", FALSE);
		gtk_window_set_has_resize_grip(GTK_WINDOW(sakura.main_window), FALSE);
	}
}

static void
sakura_show_scrollbar (GtkWidget *widget, void *data)
{
	gint page;
	struct terminal *term;
	gint n_pages;
	int i;

	sakura.keep_fc=1;

	n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	if (!g_key_file_get_boolean(sakura.cfg, cfg_group, "scrollbar", NULL)) {
		sakura.show_scrollbar=true;
		sakura_set_config_boolean("scrollbar", TRUE);
	} else {
		sakura.show_scrollbar=false;
		sakura_set_config_boolean("scrollbar", FALSE);
	}

	/* Toggle/Untoggle the scrollbar for all tabs */
	for (i = (n_pages - 1); i >= 0; i--) {
		term = sakura_get_page_term(sakura, i);
		if (!sakura.show_scrollbar)
			gtk_widget_hide(term->scrollbar);
		else
			gtk_widget_show(term->scrollbar);
	}
	sakura_set_size();
}


static void
sakura_audible_bell (GtkWidget *widget, void *data)
{
	gint page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		vte_terminal_set_audible_bell (VTE_TERMINAL(term->vte), TRUE);
		sakura_set_config_string("audible_bell", "Yes");
	} else {
		vte_terminal_set_audible_bell (VTE_TERMINAL(term->vte), FALSE);
		sakura_set_config_string("audible_bell", "No");
	}
}


static void
sakura_visible_bell (GtkWidget *widget, void *data)
{
	gint page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		vte_terminal_set_visible_bell (VTE_TERMINAL(term->vte), TRUE);
		sakura_set_config_string("visible_bell", "Yes");
	} else {
		vte_terminal_set_visible_bell (VTE_TERMINAL(term->vte), FALSE);
		sakura_set_config_string("visible_bell", "No");
	}
}


static void
sakura_blinking_cursor (GtkWidget *widget, void *data)
{
	gint page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		vte_terminal_set_cursor_blink_mode (VTE_TERMINAL(term->vte), VTE_CURSOR_BLINK_ON);
		sakura_set_config_string("blinking_cursor", "Yes");
	} else {
		vte_terminal_set_cursor_blink_mode (VTE_TERMINAL(term->vte), VTE_CURSOR_BLINK_OFF);
		sakura_set_config_string("blinking_cursor", "No");
	}
}


static void
sakura_allow_bold (GtkWidget *widget, void *data)
{
	gint page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		vte_terminal_set_allow_bold (VTE_TERMINAL(term->vte), TRUE);
		sakura_set_config_string("allow_bold", "Yes");
	} else {
		vte_terminal_set_allow_bold (VTE_TERMINAL(term->vte), FALSE);
		sakura_set_config_string("allow_bold", "No");
	}
}


static void
sakura_set_cursor(GtkWidget *widget, void *data)
{
	struct terminal *term;
	int n_pages, i;

	char *cursor_string = (char *)data;
	n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {

		if (strcmp(cursor_string, "block")==0) {
			sakura.cursor_type=VTE_CURSOR_SHAPE_BLOCK;
		} else if (strcmp(cursor_string, "underline")==0) {
			sakura.cursor_type=VTE_CURSOR_SHAPE_UNDERLINE;
		} else if (strcmp(cursor_string, "ibeam")==0) {
			sakura.cursor_type=VTE_CURSOR_SHAPE_IBEAM;
		} 

		for (i = (n_pages - 1); i >= 0; i--) {
			term = sakura_get_page_term(sakura, i);
			vte_terminal_set_cursor_shape(VTE_TERMINAL(term->vte), sakura.cursor_type);
		}

		sakura_set_config_integer("cursor_type", sakura.cursor_type);
	}
}


static void
sakura_set_palette(GtkWidget *widget, void *data)
{
	struct terminal *term;
	int n_pages, i;

	char *palette=(char *)data;

	n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		if (strcmp(palette, "linux")==0) {
			sakura.palette=linux_palette;
		} else if (strcmp(palette, "xterm")==0) {
			sakura.palette=xterm_palette;
		} else if (strcmp(palette, "tango")==0) {
			sakura.palette=tango_palette;
		} else if (strcmp(palette, "solarized_dark")==0) {
			sakura.palette=solarized_dark_palette;
		} else {
			sakura.palette=solarized_light_palette;	
		}

		for (i = (n_pages - 1); i >= 0; i--) {
			term = sakura_get_page_term(sakura, i);
			vte_terminal_set_colors_rgba(VTE_TERMINAL(term->vte),
			                        &sakura.forecolors[term->colorset], 
			                        &sakura.backcolors[term->colorset],
			                        sakura.palette, PALETTE_SIZE);
			vte_terminal_set_color_cursor_rgba(VTE_TERMINAL(term->vte), &sakura.curscolors[term->colorset]);
		}

		sakura_set_config_string("palette", palette);
	}
}


/* Retrieve the cwd of the specified term page.
 * Original function was from terminal-screen.c of gnome-terminal, copyright (C) 2001 Havoc Pennington
 * Adapted by Hong Jen Yee, non-linux shit removed by David Gómez */
static char*
sakura_get_term_cwd(struct terminal* term)
{
	char *cwd = NULL;

	if (term->pid >= 0) {
		char *file, *buf;
		struct stat sb;
		int len;

		file = g_strdup_printf ("/proc/%d/cwd", term->pid);

		if (g_stat(file, &sb) == -1) {
			g_free(file);
			return cwd;
		}

		buf = malloc(sb.st_size + 1);

		if(buf == NULL) {
			g_free(file);
			return cwd;
		}

		len = readlink(file, buf, sb.st_size + 1);

		if (len > 0 && buf[0] == '/') {
			buf[len] = '\0';
			cwd = g_strdup(buf);
		}

		g_free(buf);
		g_free(file);
	}

	return cwd;
}


static gboolean
sakura_resized_window (GtkWidget *widget, GdkEventConfigure *event, void *data)
{
	if (event->width!=sakura.width || event->height!=sakura.height) {
		SAY("Configure event received. Current w %d h %d ConfigureEvent w %d h %d",
		sakura.width, sakura.height, event->width, event->height);
		sakura.resized=TRUE;
	}
		
	return FALSE;
}


static void
sakura_setname_entry_changed (GtkWidget *widget, void *data)
{
	GtkDialog *title_dialog=(GtkDialog *)data;

	if (strcmp(gtk_entry_get_text(GTK_ENTRY(widget)), "")==0) {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT, FALSE);
	} else {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT, TRUE);
	}
}


/* Parameters are never used */
static void
sakura_copy (GtkWidget *widget, void *data)
{
	gint page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	vte_terminal_copy_clipboard(VTE_TERMINAL(term->vte));
}


/* Parameters are never used */
static void
sakura_paste (GtkWidget *widget, void *data)
{
	gint page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	vte_terminal_paste_clipboard(VTE_TERMINAL(term->vte));
}


static void
sakura_new_tab (GtkWidget *widget, void *data)
{
	sakura_add_tab();
}


static void
sakura_close_tab (GtkWidget *widget, void *data)
{
	pid_t pgid;
	GtkWidget *dialog;
	gint response;
	struct terminal *term;
	gint page, npages;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	/* Only write configuration to disk if it's the last tab */
	if (npages==1) {
		sakura_config_done();
	}

	/* Check if there are running processes for this tab. Use tcgetpgrp to compare to the shell PGID */
	pgid = tcgetpgrp(vte_terminal_get_pty(VTE_TERMINAL(term->vte)));
	
	if ( (pgid != -1) && (pgid != term->pid) && (!sakura.less_questions) ) {
			dialog=gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
										  GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
										  _("There is a running process in this terminal.\n\nDo you really want to close it?"));

			response=gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);

			if (response==GTK_RESPONSE_YES) {
				sakura_del_tab(page);
			}
	} else
		sakura_del_tab(page);

	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	if (npages==0)
		sakura_destroy();
}


static void
sakura_fullscreen (GtkWidget *widget, void *data)
{
	if (sakura.fullscreen!=TRUE) {
		sakura.fullscreen=TRUE;
		gtk_window_fullscreen(GTK_WINDOW(sakura.main_window));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(sakura.main_window));
		sakura.fullscreen=FALSE;
	}
}


/* Callback for the tabs close buttons */
static void
sakura_closebutton_clicked(GtkWidget *widget, void *data)
{
	gint page;
	GtkWidget *hbox=(GtkWidget *)data;
	struct terminal *term;
	pid_t pgid;
	GtkWidget *dialog;
	gint npages, response;

	page = gtk_notebook_page_num(GTK_NOTEBOOK(sakura.notebook), hbox);
	term = sakura_get_page_term(sakura, page);
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Only write configuration to disk if it's the last tab */
	if (npages==1) {
		sakura_config_done();
	}

	/* Check if there are running processes for this tab. Use tcgetpgrp to compare to the shell PGID */
	pgid = tcgetpgrp(vte_terminal_get_pty(VTE_TERMINAL(term->vte)));
	
	if ( (pgid != -1) && (pgid != term->pid) && (!sakura.less_questions) ) {
			dialog=gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
										  GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
										  _("There is a running process in this terminal.\n\nDo you really want to close it?"));

			response=gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);

			if (response==GTK_RESPONSE_YES) {
				sakura_del_tab(page);

				if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
					sakura_destroy();
			}
	} else {  /* No processes, hell with tab */

		sakura_del_tab(page);

		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
			sakura_destroy();
	}
}

/* Callback called when sakura configuration file is modified by an external process */
static void
sakura_conf_changed (GtkWidget *widget, void *data) 
{
	sakura.externally_modified=true;
}


/******* Functions ********/


static void
sakura_init()
{
	GError *gerror=NULL;
	char* configdir = NULL;
	int i;

	term_data_id = g_quark_from_static_string("sakura_term");

	/* Config file initialization*/
	sakura.cfg = g_key_file_new();
	sakura.config_modified=false;

	configdir = g_build_filename( g_get_user_config_dir(), "sakura", NULL );
	if( ! g_file_test( g_get_user_config_dir(), G_FILE_TEST_EXISTS) )
		g_mkdir( g_get_user_config_dir(), 0755 );
	if( ! g_file_test( configdir, G_FILE_TEST_EXISTS) )
		g_mkdir( configdir, 0755 );
	if (option_config_file) {
		sakura.configfile=g_build_filename(configdir, option_config_file, NULL);
	} else {
		/* Use more standard-conforming path for config files, if available. */
		sakura.configfile=g_build_filename(configdir, DEFAULT_CONFIGFILE, NULL);
	}
	g_free(configdir);

	/* Open config file */
	if (!g_key_file_load_from_file(sakura.cfg, sakura.configfile, 0, &gerror)) {
		/* If there's no file, ignore the error. A new one is created */
		if (gerror->code==G_KEY_FILE_ERROR_UNKNOWN_ENCODING || gerror->code==G_KEY_FILE_ERROR_INVALID_VALUE) {
			fprintf(stderr, "Not valid config file format\n");
			exit(EXIT_FAILURE);
		}
	}
	
	/* Add GFile monitor to control file external changes */
	GFile *cfgfile = g_file_new_for_path(sakura.configfile);
	GFileMonitor *mon_cfgfile = g_file_monitor_file (cfgfile, 0, NULL, NULL);
	g_signal_connect(G_OBJECT(mon_cfgfile), "changed", G_CALLBACK(sakura_conf_changed), NULL);
	
	gchar *cfgtmp = NULL;

	/* We can safely ignore errors from g_key_file_get_value(), since if the
	 * call to g_key_file_has_key() was successful, the key IS there. From the
	 * glib docs I don't know if we can ignore errors from g_key_file_has_key,
	 * too. I think we can: the only possible error is that the config file
	 * doesn't exist, but we have just read it!
	 */

	for( i=0; i<NUM_COLORSETS; i++) {
		char temp_name[20]; 

		sprintf(temp_name, "colorset%d_fore", i+1);
		if (!g_key_file_has_key(sakura.cfg, cfg_group, temp_name, NULL)) {
			sakura_set_config_string(temp_name, "rgb(192,192,192)");
		}
		cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, temp_name, NULL);
		gdk_rgba_parse(&sakura.forecolors[i], cfgtmp);
		g_free(cfgtmp);

		sprintf(temp_name, "colorset%d_back", i+1);
		if (!g_key_file_has_key(sakura.cfg, cfg_group, temp_name, NULL)) {
			sakura_set_config_string(temp_name, "rgba(0,0,0,1)");
		}
		cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, temp_name, NULL);
		gdk_rgba_parse(&sakura.backcolors[i], cfgtmp);
		g_free(cfgtmp);

		sprintf(temp_name, "colorset%d_curs", i+1);
		if (!g_key_file_has_key(sakura.cfg, cfg_group, temp_name, NULL)) {
			sakura_set_config_string(temp_name, "rgb(255,255,255)");
		}
		cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, temp_name, NULL);
		gdk_rgba_parse(&sakura.curscolors[i], cfgtmp);
		g_free(cfgtmp);

		sprintf(temp_name, "colorset%d_key", i+1);
		if (!g_key_file_has_key(sakura.cfg, cfg_group, temp_name, NULL)) {
			sakura_set_config_key(temp_name, cs_keys[i]);
		}
		sakura.set_colorset_keys[i]= sakura_get_config_key(temp_name);
	}

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "last_colorset", NULL)) {
		sakura_set_config_integer("last_colorset", 1);
	}
	sakura.last_colorset = g_key_file_get_integer(sakura.cfg, cfg_group, "last_colorset", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "background", NULL)) {
		sakura_set_config_string("background", "none");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "background", NULL);
	if (strcmp(cfgtmp, "none")==0) {
		sakura.background=NULL;
	} else {
		sakura.background=g_strdup(cfgtmp);
	}
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "font", NULL)) {
		sakura_set_config_string("font", DEFAULT_FONT);
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "font", NULL);
	sakura.font = pango_font_description_from_string(cfgtmp);
	free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "show_always_first_tab", NULL)) {
		sakura_set_config_string("show_always_first_tab", "No");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "show_always_first_tab", NULL);
	sakura.first_tab = (strcmp(cfgtmp, "Yes")==0) ? true : false;
	free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "scrollbar", NULL)) {
		sakura_set_config_boolean("scrollbar", FALSE);
	}
	sakura.show_scrollbar = g_key_file_get_boolean(sakura.cfg, cfg_group, "scrollbar", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "resize_grip", NULL)) {
		sakura_set_config_boolean("resize_grip", FALSE);
	}
	sakura.show_resize_grip = g_key_file_get_boolean(sakura.cfg, cfg_group, "resize_grip", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "closebutton", NULL)) {
		sakura_set_config_boolean("closebutton", TRUE);
	}
	sakura.show_closebutton = g_key_file_get_boolean(sakura.cfg, cfg_group, "closebutton", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "tabs_on_bottom", NULL)) {
		sakura_set_config_boolean("tabs_on_bottom", FALSE);
	}
	sakura.tabs_on_bottom = g_key_file_get_boolean(sakura.cfg, cfg_group, "tabs_on_bottom", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "less_questions", NULL)) {
		sakura_set_config_boolean("less_questions", FALSE);
	}
	sakura.less_questions = g_key_file_get_boolean(sakura.cfg, cfg_group, "less_questions", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "audible_bell", NULL)) {
		sakura_set_config_string("audible_bell", "Yes");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "audible_bell", NULL);
	sakura.audible_bell= (strcmp(cfgtmp, "Yes")==0) ? 1 : 0;
	g_free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "visible_bell", NULL)) {
		sakura_set_config_string("visible_bell", "No");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "visible_bell", NULL);
	sakura.visible_bell= (strcmp(cfgtmp, "Yes")==0) ? 1 : 0;
	g_free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "blinking_cursor", NULL)) {
		sakura_set_config_string("blinking_cursor", "No");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "blinking_cursor", NULL);
	sakura.blinking_cursor= (strcmp(cfgtmp, "Yes")==0) ? 1 : 0;
	g_free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "allow_bold", NULL)) {
		sakura_set_config_string("allow_bold", "Yes");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "allow_bold", NULL);
	sakura.allow_bold= (strcmp(cfgtmp, "Yes")==0) ? 1 : 0;
	g_free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "cursor_type", NULL)) {
		sakura_set_config_string("cursor_type", "VTE_CURSOR_SHAPE_BLOCK");
	}
	sakura.cursor_type = g_key_file_get_integer(sakura.cfg, cfg_group, "cursor_type", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "word_chars", NULL)) {
		sakura_set_config_string("word_chars", DEFAULT_WORD_CHARS);
	}
	sakura.word_chars = g_key_file_get_value(sakura.cfg, cfg_group, "word_chars", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "palette", NULL)) {
		sakura_set_config_string("palette", DEFAULT_PALETTE);
	}
	cfgtmp = g_key_file_get_string(sakura.cfg, cfg_group, "palette", NULL);
	if (strcmp(cfgtmp, "linux")==0) {
		sakura.palette=linux_palette;
	} else if (strcmp(cfgtmp, "xterm")==0) {
		sakura.palette=xterm_palette;
	} else if (strcmp(cfgtmp, "tango")==0) {
		sakura.palette=tango_palette;
	} else if (strcmp(cfgtmp, "solarized_dark")==0) {
		sakura.palette=solarized_dark_palette;
	} else {
		sakura.palette=solarized_light_palette;
	}
	g_free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "add_tab_accelerator", NULL)) {
		sakura_set_config_integer("add_tab_accelerator", DEFAULT_ADD_TAB_ACCELERATOR);
	}
	sakura.add_tab_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "add_tab_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "del_tab_accelerator", NULL)) {
		sakura_set_config_integer("del_tab_accelerator", DEFAULT_DEL_TAB_ACCELERATOR);
	}
	sakura.del_tab_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "del_tab_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "switch_tab_accelerator", NULL)) {
		sakura_set_config_integer("switch_tab_accelerator", DEFAULT_SWITCH_TAB_ACCELERATOR);
	}
	sakura.switch_tab_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "switch_tab_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "move_tab_accelerator", NULL)) {
		sakura_set_config_integer("move_tab_accelerator", DEFAULT_MOVE_TAB_ACCELERATOR);
	}
	sakura.move_tab_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "move_tab_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "copy_accelerator", NULL)) {
		sakura_set_config_integer("copy_accelerator", DEFAULT_COPY_ACCELERATOR);
	}
	sakura.copy_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "copy_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "scrollbar_accelerator", NULL)) {
		sakura_set_config_integer("scrollbar_accelerator", DEFAULT_SCROLLBAR_ACCELERATOR);
	}
	sakura.scrollbar_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "scrollbar_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "open_url_accelerator", NULL)) {
		sakura_set_config_integer("open_url_accelerator", DEFAULT_OPEN_URL_ACCELERATOR);
	}
	sakura.open_url_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "open_url_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "font_size_accelerator", NULL)) {
		sakura_set_config_integer("font_size_accelerator", DEFAULT_FONT_SIZE_ACCELERATOR);
	}
	sakura.font_size_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "font_size_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "set_tab_name_accelerator", NULL)) {
		sakura_set_config_integer("set_tab_name_accelerator", DEFAULT_SET_TAB_NAME_ACCELERATOR);
	}
	sakura.set_tab_name_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "set_tab_name_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "add_tab_key", NULL)) {
		sakura_set_config_key("add_tab_key", DEFAULT_ADD_TAB_KEY);
	}
	sakura.add_tab_key = sakura_get_config_key("add_tab_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "del_tab_key", NULL)) {
		sakura_set_config_key("del_tab_key", DEFAULT_DEL_TAB_KEY);
	}
	sakura.del_tab_key = sakura_get_config_key("del_tab_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "prev_tab_key", NULL)) {
		sakura_set_config_key("prev_tab_key", DEFAULT_PREV_TAB_KEY);
	}
	sakura.prev_tab_key = sakura_get_config_key("prev_tab_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "next_tab_key", NULL)) {
		sakura_set_config_key("next_tab_key", DEFAULT_NEXT_TAB_KEY);
	}
	sakura.next_tab_key = sakura_get_config_key("next_tab_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "copy_key", NULL)) {
		sakura_set_config_key( "copy_key", DEFAULT_COPY_KEY);
	}
	sakura.copy_key = sakura_get_config_key("copy_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "paste_key", NULL)) {
		sakura_set_config_key("paste_key", DEFAULT_PASTE_KEY);
	}
	sakura.paste_key = sakura_get_config_key("paste_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "scrollbar_key", NULL)) {
		sakura_set_config_key("scrollbar_key", DEFAULT_SCROLLBAR_KEY);
	}
	sakura.scrollbar_key = sakura_get_config_key("scrollbar_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "set_tab_name_key", NULL)) {
		sakura_set_config_key("set_tab_name_key", DEFAULT_SET_TAB_NAME_KEY);
	}
	sakura.set_tab_name_key = sakura_get_config_key("set_tab_name_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "fullscreen_key", NULL)) {
		sakura_set_config_key("fullscreen_key", DEFAULT_FULLSCREEN_KEY);
	}
	sakura.fullscreen_key = sakura_get_config_key("fullscreen_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "set_colorset_accelerator", NULL)) {
		sakura_set_config_integer("set_colorset_accelerator", DEFAULT_SELECT_COLORSET_ACCELERATOR);
	}
	sakura.set_colorset_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "set_colorset_accelerator", NULL);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "icon_file", NULL)) {
		sakura_set_config_string("icon_file", ICON_FILE);
	}
	/* We don't need a global because it's not configurable within sakura */

	sakura.provider = gtk_css_provider_new();

	sakura.main_window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(sakura.main_window), "sakura");
	gtk_window_set_has_resize_grip(GTK_WINDOW(sakura.main_window), sakura.show_resize_grip);

	/* Add datadir path to icon name */
	char *icon = g_key_file_get_value(sakura.cfg, cfg_group, "icon_file", NULL);
	char *icon_path = g_strdup_printf(DATADIR "/pixmaps/%s", icon);
	gtk_window_set_icon_from_file(GTK_WINDOW(sakura.main_window), icon_path, &gerror);
	g_free(icon); g_free(icon_path); icon=NULL; icon_path=NULL;

	/* Default terminal size*/
	sakura.columns = DEFAULT_COLUMNS;
	sakura.rows = DEFAULT_ROWS;

	sakura.notebook=gtk_notebook_new();

	/* Figure out if we have rgba capabilities. */
	GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (sakura.main_window));
	GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
	if (visual != NULL && gdk_screen_is_composited (screen)) {
		gtk_widget_set_visual (GTK_WIDGET (sakura.main_window), visual);
		sakura.has_rgba = true;
	} else {
		/* Probably not needed, as is likely the default initializer */
		sakura.has_rgba = false;
	}

	/* Command line options initialization */

	/* Set argv for forked childs. Real argv vector starts at argv[1] because we're
	   using G_SPAWN_FILE_AND_ARGV_ZERO to be able to launch login shells */
	sakura.argv[0]=g_strdup(g_getenv("SHELL"));
	if (option_login) {
		sakura.argv[1]=g_strdup_printf("-%s", g_getenv("SHELL"));
	} else {
		sakura.argv[1]=g_strdup(g_getenv("SHELL"));
	}
	sakura.argv[2]=NULL;

	if (option_title) {
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), option_title);
	}

	if (option_columns) {
		sakura.columns = option_columns;
	}

	if (option_rows) {
		sakura.rows = option_rows;
	}

	if (option_font) {
		sakura.font=pango_font_description_from_string(option_font);
	}

	/* These options are exclusive */
	if (option_fullscreen) {
		sakura_fullscreen(NULL, NULL);
	} else if (option_maximize) {
		gtk_window_maximize(GTK_WINDOW(sakura.main_window));
	}

	sakura.label_count=1;
	sakura.fullscreen=FALSE;
	sakura.resized=FALSE;
	sakura.keep_fc=false;
	sakura.externally_modified=false;

	gerror=NULL;
	sakura.http_regexp=g_regex_new(HTTP_REGEXP, G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY, &gerror);

	gtk_container_add(GTK_CONTAINER(sakura.main_window), sakura.notebook);

	/* Init notebook */
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(sakura.notebook), TRUE);

	sakura_init_popup();

	g_signal_connect(G_OBJECT(sakura.main_window), "delete_event", G_CALLBACK(sakura_delete_event), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "destroy", G_CALLBACK(sakura_destroy_window), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "key-press-event", G_CALLBACK(sakura_key_press), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "configure-event", G_CALLBACK(sakura_resized_window), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "show", G_CALLBACK(sakura_window_show_event), NULL);
}


static void
sakura_init_popup()
{
	GtkWidget *item_new_tab, *item_set_name, *item_close_tab, *item_copy,
	          *item_paste, *item_select_font, *item_select_colors,
	          *item_select_background, *item_set_title, *item_fullscreen,
	          *item_toggle_scrollbar, *item_options,
	          *item_show_first_tab, *item_audible_bell, *item_visible_bell,
	          *item_blinking_cursor, *item_allow_bold, *item_other_options, 
			  *item_cursor, *item_cursor_block, *item_cursor_underline, *item_cursor_ibeam,
	          *item_palette, *item_palette_tango, *item_palette_linux, *item_palette_xterm,
			  *item_palette_solarized_dark, *item_palette_solarized_light,
	          *item_show_close_button, *item_tabs_on_bottom, *item_less_questions,
			  *item_toggle_resize_grip;
	//GtkAction *action_open_link, *action_copy_link, *action_new_tab, *action_set_name, *action_close_tab,
	//          *action_copy, *action_paste, *action_select_font, *action_select_colors,
	//          *action_select_background, *action_clear_background, *action_set_title,
	//          *action_fullscreen;
	GtkWidget *options_menu, *other_options_menu, *cursor_menu, *palette_menu;

#if 0
	/* Define actions */
	/* TODO: Should we add icons with g_menu_item_set_icon? */
	action_open_link=gtk_action_new("open_link", _("Open link..."), NULL, NULL);
	action_copy_link=gtk_action_new("copy_link", _("Copy link..."), NULL, NULL);
	action_new_tab=gtk_action_new("new_tab", _("New tab"), NULL, NULL);
	action_set_name=gtk_action_new("set_name", _("Set tab name..."), NULL, NULL);
	action_close_tab=gtk_action_new("close_tab", _("Close tab"), NULL, NULL);
	action_fullscreen=gtk_action_new("fullscreen", _("Full screen"), NULL, NULL);
	action_copy=gtk_action_new("copy", _("Copy"), NULL, NULL);
	action_paste=gtk_action_new("paste", _("Paste"), NULL, NULL);
	action_select_font=gtk_action_new("select_font", _("Select font..."), NULL, NULL);
	action_select_colors=gtk_action_new("select_colors", _("Select colors..."), NULL, NULL);
	action_select_background=gtk_action_new("select_background", _("Select background..."), NULL, NULL);
	action_clear_background=gtk_action_new("clear_background", _("Clear background"), NULL, NULL);
	action_set_title=gtk_action_new("set_title", _("Set window title..."), NULL, NULL);

	/* Create menuitems */
	sakura.item_open_link=gtk_action_create_menu_item(action_open_link);
	sakura.item_copy_link=gtk_action_create_menu_item(action_copy_link);
	item_new_tab=gtk_action_create_menu_item(action_new_tab);
	item_set_name=gtk_action_create_menu_item(action_set_name);
	item_close_tab=gtk_action_create_menu_item(action_close_tab);
	item_fullscreen=gtk_action_create_menu_item(action_fullscreen);
	item_copy=gtk_action_create_menu_item(action_copy);
	item_paste=gtk_action_create_menu_item(action_paste);
	item_select_font=gtk_action_create_menu_item(action_select_font);
	item_select_colors=gtk_action_create_menu_item(action_select_colors);
	item_select_background=gtk_action_create_menu_item(action_select_background);
	sakura.item_clear_background=gtk_action_create_menu_item(action_clear_background);
	item_set_title=gtk_action_create_menu_item(action_set_title);
#endif

	/*FIXME: Use new menu declaration style: gtk_menu_new_from_model */
	sakura.item_open_link=gtk_menu_item_new_with_label(_("Open link"));
	sakura.item_copy_link=gtk_menu_item_new_with_label(_("Copy link"));
	item_new_tab=gtk_menu_item_new_with_label(_("New tab"));
	item_set_name=gtk_menu_item_new_with_label(_("Set tab name..."));
	item_close_tab=gtk_menu_item_new_with_label(_("Close tab"));
	item_fullscreen=gtk_menu_item_new_with_label(("Full screen"));
	item_copy=gtk_menu_item_new_with_label(_("Copy"));
	item_paste=gtk_menu_item_new_with_label(_("Paste"));
	item_select_font=gtk_menu_item_new_with_label(_("Select font..."));
	item_select_colors=gtk_menu_item_new_with_label(_("Select colors..."));
	item_select_background=gtk_menu_item_new_with_label(_("Select background..."));
	sakura.item_clear_background=gtk_menu_item_new_with_label(_("Clear background"));
	item_set_title=gtk_menu_item_new_with_label(_("Set window title..."));

	item_options=gtk_menu_item_new_with_label(_("Options"));

	item_other_options=gtk_menu_item_new_with_label(_("More"));
	item_show_first_tab=gtk_check_menu_item_new_with_label(_("Always show tab bar"));
	item_tabs_on_bottom=gtk_check_menu_item_new_with_label(_("Tabs at bottom"));
	item_show_close_button=gtk_check_menu_item_new_with_label(_("Show close button on tabs"));
	item_toggle_scrollbar=gtk_check_menu_item_new_with_label(_("Show scrollbar"));
	item_toggle_resize_grip=gtk_check_menu_item_new_with_label(_("Show resize grip"));
	item_less_questions=gtk_check_menu_item_new_with_label(_("Don't show exit dialog"));
	item_audible_bell=gtk_check_menu_item_new_with_label(_("Set audible bell"));
	item_visible_bell=gtk_check_menu_item_new_with_label(_("Set visible bell"));
	item_blinking_cursor=gtk_check_menu_item_new_with_label(_("Set blinking cursor"));
	item_allow_bold=gtk_check_menu_item_new_with_label(_("Enable bold font"));
	item_cursor=gtk_menu_item_new_with_label(_("Set cursor type"));
	item_cursor_block=gtk_radio_menu_item_new_with_label(NULL, _("Block"));
	item_cursor_underline=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_cursor_block), _("Underline"));
	item_cursor_ibeam=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_cursor_block), _("IBeam"));
	item_palette=gtk_menu_item_new_with_label(_("Set palette"));
	item_palette_tango=gtk_radio_menu_item_new_with_label(NULL, "Tango");
	item_palette_linux=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_palette_tango), "Linux");
	item_palette_xterm=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_palette_tango), "Xterm");
	item_palette_solarized_dark=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_palette_tango), "Solarized dark");
	item_palette_solarized_light=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_palette_tango), "Solarized light");

	/* Show defaults in menu items */
	gchar *cfgtmp = NULL;

	if (sakura.first_tab) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_first_tab), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_first_tab), FALSE);
	}

	if (sakura.show_closebutton) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_close_button), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_close_button), FALSE);
	}

	if (sakura.tabs_on_bottom) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_tabs_on_bottom), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_tabs_on_bottom), FALSE);
	}

	if (sakura.less_questions) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_less_questions), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_less_questions), FALSE);
	}

	if (sakura.show_scrollbar) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_scrollbar), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_scrollbar), FALSE);
	}

	if (sakura.show_resize_grip) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_resize_grip), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_resize_grip), FALSE);
	}

	if (sakura.audible_bell) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_audible_bell), TRUE);
	}

	if (sakura.visible_bell) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_visible_bell), TRUE);
	}

	if (sakura.blinking_cursor) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_blinking_cursor), TRUE);
	}

	if (sakura.allow_bold) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_allow_bold), TRUE);
	}

	switch (sakura.cursor_type){
		case VTE_CURSOR_SHAPE_BLOCK:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_cursor_block), TRUE);
			break;
		case VTE_CURSOR_SHAPE_UNDERLINE:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_cursor_underline), TRUE);
			break;
		case VTE_CURSOR_SHAPE_IBEAM:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_cursor_ibeam), TRUE);
	}

	cfgtmp = g_key_file_get_string(sakura.cfg, cfg_group, "palette", NULL);
	if (strcmp(cfgtmp, "linux")==0) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_palette_linux), TRUE);
	} else if (strcmp(cfgtmp, "tango")==0) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_palette_tango), TRUE);
	} else if (strcmp(cfgtmp, "xterm")==0) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_palette_xterm), TRUE);
	} else if (strcmp(cfgtmp, "solarized_dark")==0) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_palette_solarized_dark), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_palette_solarized_light), TRUE);
	}
	g_free(cfgtmp);

	sakura.open_link_separator=gtk_separator_menu_item_new();

	sakura.menu=gtk_menu_new();
	//sakura.labels_menu=gtk_menu_new();

	/* Add items to popup menu */
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.item_open_link);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.item_copy_link);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.open_link_separator);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_new_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_set_name);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_close_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_fullscreen);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_copy);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_paste);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.item_clear_background);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_options);

	options_menu=gtk_menu_new();
	other_options_menu=gtk_menu_new();
	cursor_menu=gtk_menu_new();
	palette_menu=gtk_menu_new();

	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_set_title);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_select_colors);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_select_font);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_select_background);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_other_options);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_show_first_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_tabs_on_bottom);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_show_close_button);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_toggle_scrollbar);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_toggle_resize_grip);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_less_questions);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_audible_bell);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_visible_bell);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_blinking_cursor);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_allow_bold);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_cursor);
	gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item_cursor_block);
	gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item_cursor_underline);
	gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item_cursor_ibeam);
	gtk_menu_shell_append(GTK_MENU_SHELL(other_options_menu), item_palette);
	gtk_menu_shell_append(GTK_MENU_SHELL(palette_menu), item_palette_tango);
	gtk_menu_shell_append(GTK_MENU_SHELL(palette_menu), item_palette_linux);
	gtk_menu_shell_append(GTK_MENU_SHELL(palette_menu), item_palette_xterm);
	gtk_menu_shell_append(GTK_MENU_SHELL(palette_menu), item_palette_solarized_dark);
	gtk_menu_shell_append(GTK_MENU_SHELL(palette_menu), item_palette_solarized_light);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_options), options_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_other_options), other_options_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_cursor), cursor_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_palette), palette_menu);

	/* ... and finally assign callbacks to menuitems */
	g_signal_connect(G_OBJECT(item_new_tab), "activate", G_CALLBACK(sakura_new_tab), NULL);
	g_signal_connect(G_OBJECT(item_set_name), "activate", G_CALLBACK(sakura_set_name_dialog), NULL);
	g_signal_connect(G_OBJECT(item_close_tab), "activate", G_CALLBACK(sakura_close_tab), NULL);
	g_signal_connect(G_OBJECT(item_select_font), "activate", G_CALLBACK(sakura_font_dialog), NULL);
	g_signal_connect(G_OBJECT(item_select_background), "activate", G_CALLBACK(sakura_select_background_dialog), NULL);
	g_signal_connect(G_OBJECT(item_copy), "activate", G_CALLBACK(sakura_copy), NULL);
	g_signal_connect(G_OBJECT(item_paste), "activate", G_CALLBACK(sakura_paste), NULL);
	g_signal_connect(G_OBJECT(item_select_colors), "activate", G_CALLBACK(sakura_color_dialog), NULL);

	g_signal_connect(G_OBJECT(item_show_first_tab), "activate", G_CALLBACK(sakura_show_first_tab), NULL);
	g_signal_connect(G_OBJECT(item_tabs_on_bottom), "activate", G_CALLBACK(sakura_tabs_on_bottom), NULL);
	g_signal_connect(G_OBJECT(item_less_questions), "activate", G_CALLBACK(sakura_less_questions), NULL);
	g_signal_connect(G_OBJECT(item_show_close_button), "activate", G_CALLBACK(sakura_show_close_button), NULL);
	g_signal_connect(G_OBJECT(item_toggle_scrollbar), "activate", G_CALLBACK(sakura_show_scrollbar), NULL);
	g_signal_connect(G_OBJECT(item_toggle_resize_grip), "activate", G_CALLBACK(sakura_show_resize_grip), NULL);
	g_signal_connect(G_OBJECT(item_audible_bell), "activate", G_CALLBACK(sakura_audible_bell), NULL);
	g_signal_connect(G_OBJECT(item_visible_bell), "activate", G_CALLBACK(sakura_visible_bell), NULL);
	g_signal_connect(G_OBJECT(item_blinking_cursor), "activate", G_CALLBACK(sakura_blinking_cursor), NULL);
	g_signal_connect(G_OBJECT(item_allow_bold), "activate", G_CALLBACK(sakura_allow_bold), NULL);
	g_signal_connect(G_OBJECT(item_set_title), "activate", G_CALLBACK(sakura_set_title_dialog), NULL);
	g_signal_connect(G_OBJECT(item_cursor_block), "activate", G_CALLBACK(sakura_set_cursor), "block");
	g_signal_connect(G_OBJECT(item_cursor_underline), "activate", G_CALLBACK(sakura_set_cursor), "underline");
	g_signal_connect(G_OBJECT(item_cursor_ibeam), "activate", G_CALLBACK(sakura_set_cursor), "ibeam");
	g_signal_connect(G_OBJECT(item_palette_tango), "activate", G_CALLBACK(sakura_set_palette), "tango");
	g_signal_connect(G_OBJECT(item_palette_linux), "activate", G_CALLBACK(sakura_set_palette), "linux");
	g_signal_connect(G_OBJECT(item_palette_xterm), "activate", G_CALLBACK(sakura_set_palette), "xterm");
	g_signal_connect(G_OBJECT(item_palette_solarized_dark), "activate", G_CALLBACK(sakura_set_palette), "solarized_dark");
	g_signal_connect(G_OBJECT(item_palette_solarized_light), "activate", G_CALLBACK(sakura_set_palette), "solarized_light");

	g_signal_connect(G_OBJECT(sakura.item_open_link), "activate", G_CALLBACK(sakura_open_url), NULL);
	g_signal_connect(G_OBJECT(sakura.item_copy_link), "activate", G_CALLBACK(sakura_copy_url), NULL);
	g_signal_connect(G_OBJECT(sakura.item_clear_background), "activate", G_CALLBACK(sakura_clear), NULL);
	g_signal_connect(G_OBJECT(item_fullscreen), "activate", G_CALLBACK(sakura_fullscreen), NULL);


	gtk_widget_show_all(sakura.menu);

	/* We don't want to see this if there's no background image */
	if (!sakura.background) {
		gtk_widget_hide(sakura.item_clear_background);
	}
}


static void
sakura_destroy()
{
	SAY("Destroying sakura");

	/* Delete all existing tabs */
	while (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) >= 1) {
		sakura_del_tab(-1);
	}

	g_key_file_free(sakura.cfg);

	pango_font_description_free(sakura.font);

	if (sakura.background)
		free(sakura.background);

	free(sakura.configfile);

	gtk_main_quit();

}


static void
sakura_set_size(void)
{
	struct terminal *term;
	gint pad_x, pad_y;
	gint char_width, char_height;
	guint npages;
	gint min_width, natural_width;
	gint page;


	term = sakura_get_page_term(sakura, 0);
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Mayhaps an user resize happened. Check if row and columns have changed */
	if (sakura.resized) {
		sakura.columns=vte_terminal_get_column_count(VTE_TERMINAL(term->vte));
		sakura.rows=vte_terminal_get_row_count(VTE_TERMINAL(term->vte));
		SAY("New columns %ld and rows %ld", sakura.columns, sakura.rows);
		sakura.resized=FALSE;
	}

	gtk_widget_style_get(term->vte, "inner-border", &term->border, NULL);
	pad_x = term->border->left + term->border->right;
	pad_y = term->border->top + term->border->bottom;
	SAY("padding x %d y %d", pad_x, pad_y);
	char_width = vte_terminal_get_char_width(VTE_TERMINAL(term->vte));
	char_height = vte_terminal_get_char_height(VTE_TERMINAL(term->vte));

	sakura.width = pad_x + (char_width * sakura.columns);
	sakura.height = pad_y + (char_height * sakura.rows);

	if (npages>=2 || sakura.first_tab) {
		//gint min_height, natural_height; 
		//gtk_widget_get_preferred_height(sakura.notebook, &min_height, &natural_height);
		//SAY("NOTEBOOK min height %d natural height %d", min_height, natural_height);
		/* Deprecated. TODO: Remove 
		guint16 hb, vb;
		hb=gtk_notebook_get_tab_hborder(GTK_NOTEBOOK(sakura.notebook));
		vb=gtk_notebook_get_tab_vborder(GTK_NOTEBOOK(sakura.notebook));
		SAY("notebook borders h %d v %d", hb, vb);*/

		/* TODO: Yeah i know, this is utterly shit. Remove this ugly hack and set geometry hints*/
		if (!sakura.show_scrollbar) 
			//sakura.height += min_height - 10;
			sakura.height += 10;
		else
			//sakura.height += min_height - 47;
			sakura.height += 47;

		sakura.width += 8;
		sakura.width += /* (hb*2)+*/ (pad_x*2);
	}

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	gtk_widget_get_preferred_width(term->scrollbar, &min_width, &natural_width);
	SAY("SCROLLBAR min width %d natural width %d", min_width, natural_width);
	if(sakura.show_scrollbar) {
		sakura.width += min_width;
	}

	/* GTK does not ignore resize for maximized windows on some systems,
	so we do need check if it's maximized or not */
	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(sakura.main_window));
	if(gdk_window != NULL) {
		if(gdk_window_get_state(gdk_window) & GDK_WINDOW_STATE_MAXIMIZED) {
			SAY("window is maximized, will not resize");
			return;
		}
	}

	gtk_window_resize(GTK_WINDOW(sakura.main_window), sakura.width, sakura.height);
	SAY("RESIZED TO %d %d", sakura.width, sakura.height);
}


static void
sakura_set_font()
{
	gint n_pages;
	struct terminal *term;
	int i;

	n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Set the font for all tabs */
	for (i = (n_pages - 1); i >= 0; i--) {
		term = sakura_get_page_term(sakura, i);
		vte_terminal_set_font(VTE_TERMINAL(term->vte), sakura.font);
	}
}


static void
sakura_move_tab(gint direction)
{
	gint page, n_pages;
	GtkWidget *child;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	child=gtk_notebook_get_nth_page(GTK_NOTEBOOK(sakura.notebook), page);

	if (direction==FORWARD) {
		if (page!=n_pages-1)
			gtk_notebook_reorder_child(GTK_NOTEBOOK(sakura.notebook), child, page+1);
	} else {
		if (page!=0)
			gtk_notebook_reorder_child(GTK_NOTEBOOK(sakura.notebook), child, page-1);
	}
}


/* Find the notebook page for the vte terminal passed as a parameter */
static gint sakura_find_tab(VteTerminal *vte_term)
{
	gint matched_page, page, n_pages;
	struct terminal *term;

	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	
	matched_page = -1;
	page = 0;

	do {
		term = sakura_get_page_term(sakura, page);
		if ((VteTerminal *)term->vte == vte_term) {
			matched_page=page;
		}
		page++;
	} while (page < n_pages);

	return (matched_page);
}


static void
sakura_set_tab_label_text(const gchar *title, gint page)
{
	struct terminal *term;
	gchar *chopped_title;

	term = sakura_get_page_term(sakura, page);

	if ( (title!=NULL) && (g_strcmp0(title, "") !=0) ) {
		/* Chop to max size. TODO: Should it be configurable by the user? */
		chopped_title = g_strndup(title, TAB_MAX_SIZE); 
		/* Honor the minimum tab label size */
		while (strlen(chopped_title)< TAB_MIN_SIZE) {
			char *old_ptr = chopped_title;
			chopped_title = g_strconcat(chopped_title, " ", NULL);
			free(old_ptr);
		}
		gtk_label_set_text(GTK_LABEL(term->label), chopped_title);
		free(chopped_title);
	} else { /* Use the default values */
		gtk_label_set_text(GTK_LABEL(term->label), term->label_text);
	}
}


static void
sakura_add_tab()
{
	struct terminal *term;
	GtkWidget *tab_hbox;
	GtkWidget *close_button;
	int index;
	int npages;
	gchar *cwd = NULL;


	term = g_new0( struct terminal, 1 );
	term->hbox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	term->vte=vte_terminal_new();

	/* Create label for tabs */
	term->label_text=g_strdup_printf(_("Terminal %d"), sakura.label_count++);
	term->label=gtk_label_new(term->label_text);
	term->label_set_byuser=false;
	term->colorset=sakura.last_colorset-1;
	tab_hbox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_pack_start(GTK_BOX(tab_hbox), term->label, FALSE, FALSE, 0);

	/* If the tab close button is enabled, create and add it to the tab */
	if (sakura.show_closebutton) {
		close_button=gtk_button_new();
		gtk_widget_set_name(close_button, "closebutton");
		gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);

		GtkWidget *image=gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_MENU);
		gtk_container_add (GTK_CONTAINER (close_button), image);
		gtk_box_pack_start(GTK_BOX(tab_hbox), close_button, FALSE, FALSE, 0);
	}

	if (sakura.tabs_on_bottom) {
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(sakura.notebook), GTK_POS_BOTTOM);
	}

	gtk_widget_show_all(tab_hbox);

	/* Init vte */
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(term->vte), SCROLL_LINES);
	vte_terminal_match_add_gregex(VTE_TERMINAL(term->vte), sakura.http_regexp, 0);
	vte_terminal_set_mouse_autohide(VTE_TERMINAL(term->vte), TRUE);

	term->scrollbar=gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, vte_terminal_get_adjustment(VTE_TERMINAL(term->vte)));

	gtk_box_pack_start(GTK_BOX(term->hbox), term->vte, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(term->hbox), term->scrollbar, FALSE, FALSE, 0);

	/* Select the directory to use for the new tab */
	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	if(index >= 0) {
		struct terminal *prev_term;
		prev_term = sakura_get_page_term( sakura, index );
		cwd = sakura_get_term_cwd( prev_term );

		term->colorset = prev_term->colorset;
	}
	if (!cwd)
		cwd = g_get_current_dir();

	/* Keep values when adding tabs */
	sakura.keep_fc=true;

	if ((index=gtk_notebook_append_page(GTK_NOTEBOOK(sakura.notebook), term->hbox, tab_hbox))==-1) {
		sakura_error("Cannot create a new tab");
		exit(1);
	}

	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(sakura.notebook), term->hbox, TRUE);
	// TODO: Set group id to support detached tabs
	// gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(sakura.notebook), term->hbox, TRUE);

	sakura_set_page_term(sakura, index, term );

	/* vte signals */
	g_signal_connect(G_OBJECT(term->vte), "increase-font-size", G_CALLBACK(sakura_increase_font), NULL);
	g_signal_connect(G_OBJECT(term->vte), "decrease-font-size", G_CALLBACK(sakura_decrease_font), NULL);
	g_signal_connect(G_OBJECT(term->vte), "child-exited", G_CALLBACK(sakura_child_exited), NULL);
	g_signal_connect(G_OBJECT(term->vte), "eof", G_CALLBACK(sakura_eof), NULL);
	g_signal_connect(G_OBJECT(term->vte), "window-title-changed", G_CALLBACK(sakura_title_changed), NULL);
	g_signal_connect_swapped(G_OBJECT(term->vte), "button-press-event", G_CALLBACK(sakura_button_press), sakura.menu);

	/* Notebook signals */
	g_signal_connect(G_OBJECT(sakura.notebook), "page-removed", G_CALLBACK(sakura_page_removed), NULL);
	if (sakura.show_closebutton) {
		g_signal_connect(G_OBJECT(close_button), "clicked", G_CALLBACK(sakura_closebutton_clicked), term->hbox);
	}

	/* TODO: Use this with vte_spawn_sync. With fork_command__full is overwritten */
	char *command_env[2]={"TERM=xterm-256color",0};
	/* First tab */
	npages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)); 
	if (npages == 1) {
		if (sakura.first_tab) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		} else {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}

		gtk_notebook_set_show_border(GTK_NOTEBOOK(sakura.notebook), FALSE);
		sakura_set_font();
		/* Set size before showing the widgets but after setting the font */
		sakura_set_size();

		gtk_widget_show_all(sakura.notebook);
		if (!sakura.show_scrollbar) {
			gtk_widget_hide(term->scrollbar);
		}

		if (option_geometry) {
			if (!gtk_window_parse_geometry(GTK_WINDOW(sakura.main_window), option_geometry)) {
				fprintf(stderr, "Invalid geometry.\n");
				gtk_widget_show(sakura.main_window);
			} else {
				gtk_widget_show(sakura.main_window);
				sakura.columns = vte_terminal_get_column_count(VTE_TERMINAL(term->vte));
				sakura.rows = vte_terminal_get_row_count(VTE_TERMINAL(term->vte));
			}
		} else {
            gtk_widget_show(sakura.main_window);
		}

		/* Set WINDOWID env variable */
		GdkWindow *gwin = gtk_widget_get_window (sakura.main_window);
		if (gwin != NULL) {
			guint winid = gdk_x11_window_get_xid (gwin);
			gchar *winidstr = g_strdup_printf ("0x%x", winid);
			g_setenv ("WINDOWID", winidstr, FALSE);
			g_free (winidstr);
		}

		int command_argc=0; char **command_argv;
		if (option_execute||option_xterm_execute) {
			GError *gerror = NULL;
			gchar *path;

			if(option_execute) {
				/* -x option */
				if (!g_shell_parse_argv(option_execute, &command_argc, &command_argv, &gerror)) {
					switch (gerror->code) {
						case G_SHELL_ERROR_EMPTY_STRING:
							sakura_error("Empty exec string");
							exit(1);
							break;
						case G_SHELL_ERROR_BAD_QUOTING: 
							sakura_error("Cannot parse command line arguments: mangled quoting");
							exit(1);
							break;
						case G_SHELL_ERROR_FAILED:
							sakura_error("Error in exec option command line arguments");
							exit(1);
					}
				} 
			} else {
				/* -e option - last in the command line, takes all extra arguments */
				if (option_xterm_args) {
					gchar *command_joined;
					command_joined = g_strjoinv(" ", option_xterm_args);
					if (!g_shell_parse_argv(command_joined, &command_argc, &command_argv, &gerror)) {
						switch (gerror->code) {
							case G_SHELL_ERROR_EMPTY_STRING:
								sakura_error("Empty exec string");
								exit(1);
								break;
							case G_SHELL_ERROR_BAD_QUOTING: 
								sakura_error("Cannot parse command line arguments: mangled quoting");
								exit(1);
							case G_SHELL_ERROR_FAILED:
								sakura_error("Error in exec option command line arguments");
								exit(1);
						}
					}
					g_free(command_joined);
				}
			}

			/* Check if the command is valid */
			if (command_argc > 0) {
				path=g_find_program_in_path(command_argv[0]);
				if (path) {
					if (!vte_terminal_fork_command_full(VTE_TERMINAL(term->vte), VTE_PTY_NO_HELPER, NULL,
				 	    command_argv, command_env, G_SPAWN_SEARCH_PATH, NULL, NULL, &term->pid, &gerror)) {
						SAY("error: %s", gerror->message);
					}
				} else {
					sakura_error("%s command not found", command_argv[0]);
					command_argc=0;
					//exit(1);
				}
				free(path);
				g_strfreev(command_argv); g_strfreev(option_xterm_args);
			}
		} // else { /* No execute option */
		
		/* Only fork if there is no execute option or if it has failed */	
		if ( (!option_execute && !option_xterm_args) || (command_argc==0)) {	
			if (option_hold==TRUE) {
				sakura_error("Hold option given without any command");
				option_hold=FALSE;
			}
			vte_terminal_fork_command_full(VTE_TERMINAL(term->vte), VTE_PTY_NO_HELPER, cwd, sakura.argv, command_env,
						       G_SPAWN_SEARCH_PATH|G_SPAWN_FILE_AND_ARGV_ZERO, NULL, NULL, &term->pid, NULL);
		}
	/* Not the first tab */
	} else {
		sakura_set_font();
		gtk_widget_show_all(term->hbox);
		if (!sakura.show_scrollbar) {
			gtk_widget_hide(term->scrollbar);
		}

		if (npages==2) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
			sakura_set_size();
		}
		/* Call set_current page after showing the widget: gtk ignores this
		 * function in the window is not visible *sigh*. Gtk documentation
		 * says this is for "historical" reasons. Me arse */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), index);
		vte_terminal_fork_command_full(VTE_TERMINAL(term->vte), VTE_PTY_NO_HELPER, cwd, sakura.argv, command_env,
					       G_SPAWN_SEARCH_PATH|G_SPAWN_FILE_AND_ARGV_ZERO, NULL, NULL, &term->pid, NULL);
	}

	free(cwd);

	/* Configuration for the newly created terminal */
	GdkRGBA white={255, 255, 255, 1};
	vte_terminal_set_color_background_rgba(VTE_TERMINAL (term->vte), &white);
	vte_terminal_set_backspace_binding(VTE_TERMINAL(term->vte), VTE_ERASE_ASCII_DELETE);
	vte_terminal_set_colors_rgba(VTE_TERMINAL(term->vte), 
							  &sakura.forecolors[term->colorset],
							  &sakura.backcolors[term->colorset],
							  sakura.palette, PALETTE_SIZE);
	vte_terminal_set_color_cursor_rgba(VTE_TERMINAL(term->vte), &sakura.curscolors[term->colorset]);
	if (sakura.has_rgba) {
		vte_terminal_set_opacity(VTE_TERMINAL (term->vte), (int)((sakura.backcolors[term->colorset].alpha)*65535));
	}

	if (sakura.background) {
		sakura_set_bgimage(sakura.background);
	}

	if (sakura.word_chars) {
		vte_terminal_set_word_chars( VTE_TERMINAL (term->vte), sakura.word_chars );
	}

	/* Get rid of these nasty bells */
	vte_terminal_set_audible_bell (VTE_TERMINAL(term->vte), sakura.audible_bell ? TRUE : FALSE);
	vte_terminal_set_visible_bell (VTE_TERMINAL(term->vte), sakura.visible_bell ? TRUE : FALSE);

	/* Disable stupid blinking cursor */
	vte_terminal_set_cursor_blink_mode (VTE_TERMINAL(term->vte), sakura.blinking_cursor ? VTE_CURSOR_BLINK_ON : VTE_CURSOR_BLINK_OFF);

	/* Enable bold text by default */
	vte_terminal_set_allow_bold (VTE_TERMINAL(term->vte), sakura.allow_bold ? TRUE : FALSE);

	/* Change cursor */	
	vte_terminal_set_cursor_shape (VTE_TERMINAL(term->vte), sakura.cursor_type);

	/* Grrrr. Why the fucking label widget in the notebook STEAL the fucking focus? */
	gtk_widget_grab_focus(term->vte);

	/* FIXME: Possible race here. Find some way to force to process all configure
	 * events before setting keep_fc again to false */
	sakura.keep_fc=false;
}


/* Delete the notebook tab passed as a parameter */
static void
sakura_del_tab(gint page)
{
	struct terminal *term;
	gint npages;

	term = sakura_get_page_term(sakura, page);
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* When there's only one tab use the shell title, if provided */
	if (npages==2) {
		const char *title;

		term = sakura_get_page_term(sakura, 0);
		title = vte_terminal_get_window_title(VTE_TERMINAL(term->vte));
		if (title!=NULL) 
			gtk_window_set_title(GTK_WINDOW(sakura.main_window), title);
	}

	term = sakura_get_page_term(sakura, page);

	/* Do the first tab checks BEFORE deleting the tab, to ensure correct
	 * sizes are calculated when the tab is deleted */
	if ( npages == 2) {
		if (sakura.first_tab) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		} else {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}
		sakura.keep_fc=true;
	}

	gtk_widget_hide(term->hbox);
	gtk_notebook_remove_page(GTK_NOTEBOOK(sakura.notebook), page);

	/* Find the next page, if it exists, and grab focus */
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) > 0) {
		page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
		term = sakura_get_page_term(sakura, page);
		gtk_widget_grab_focus(term->vte);
	}
}


static void
sakura_set_bgimage(char *infile)
{
	GError *gerror=NULL;
	GdkPixbuf *pixbuf=NULL;
	gint page;
	struct terminal *term;

	if (!infile) SAY("File parameter is NULL");

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	/* Check file existence and type */
	if (g_file_test(infile, G_FILE_TEST_IS_REGULAR)) {

		pixbuf = gdk_pixbuf_new_from_file (infile, &gerror);
		if (!pixbuf) {
			sakura_error("Error loading image file: %s\n", gerror->message);
		} else {
			vte_terminal_set_background_image(VTE_TERMINAL(term->vte), pixbuf);
			vte_terminal_set_background_saturation(VTE_TERMINAL(term->vte), TRUE);
			vte_terminal_set_background_transparent(VTE_TERMINAL(term->vte),FALSE);

			sakura_set_config_string("background", infile);
		}
	}
}


static void
sakura_set_config_key(const gchar *key, guint value)
{
	char *valname;

	valname=gdk_keyval_name(value);
	g_key_file_set_string(sakura.cfg, cfg_group, key, valname);
	sakura.config_modified=TRUE;
	//FIXME: free() valname?
} 


static guint
sakura_get_config_key(const gchar *key)
{
	gchar *value;
	guint retval=GDK_KEY_VoidSymbol;

	value=g_key_file_get_string(sakura.cfg, cfg_group, key, NULL);
	if (value!=NULL){
		retval=gdk_keyval_from_name(value);
		g_free(value);
	}

	/* For backwards compatibility with integer values */
	/* If gdk_keyval_from_name fail, it seems to be integer value*/
	if ((retval==GDK_KEY_VoidSymbol)||(retval==0)) {
		retval=g_key_file_get_integer(sakura.cfg, cfg_group, key, NULL);
	}

	return retval;
}


static void
sakura_error(const char *format, ...)
{
	GtkWidget *dialog;
	va_list args;
	char* buff;

	va_start(args, format);
	buff = malloc(sizeof(char)*ERROR_BUFFER_LENGTH);
	vsnprintf(buff, sizeof(char)*ERROR_BUFFER_LENGTH, format, args);
	va_end(args);

	dialog = gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_DESTROY_WITH_PARENT,
	                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", buff);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error message"));
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	free(buff);
}


/* This function is used to fix bug #1393939 */
static void
sakura_sanitize_working_directory()
{
	const gchar *home_directory = g_getenv("HOME");
	if (home_directory == NULL) {
		home_directory = g_get_home_dir();
	}

	if (home_directory != NULL) {
		chdir(home_directory);
	}
}


int
main(int argc, char **argv)
{
	gchar *localedir;
	GError *error=NULL;
	GOptionContext *context;
	int i;
	int n;
	char **nargv;
	int nargc;
	gboolean have_e;

	/* Localization */
	setlocale(LC_ALL, "");
	localedir=g_strdup_printf("%s/locale", DATADIR);
	textdomain(GETTEXT_PACKAGE);
	bindtextdomain(GETTEXT_PACKAGE, localedir);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	g_free(localedir);

	/* Rewrites argv to include a -- after the -e argument this is required to make
	 * sure GOption doesn't grab any arguments meant for the command being called */

	/* Initialize nargv */
	nargv = (char**)calloc((argc+1), sizeof(char*));
	n=0; nargc=argc;
	have_e=FALSE;

	for(i=0; i<argc; i++) {
		if(!have_e && g_strcmp0(argv[i],"-e") == 0)
		{
			nargv[n]="-e";
			n++;
			nargv[n]="--";
			nargc++;
			have_e = TRUE;
		} else {
			nargv[n]=g_strdup(argv[i]);
		}
		n++;
	}

	/* Options parsing */
	context = g_option_context_new (_("- vte-based terminal emulator"));
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_group_set_translation_domain(gtk_get_option_group(TRUE), GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group(TRUE));
	if (!g_option_context_parse (context, &nargc, &nargv, &error)) {
		fprintf(stderr, "%s\n", error->message);
		exit(1);
	}
	
	if (option_workdir && chdir(option_workdir)) {
		fprintf(stderr, _("Cannot change working directory\n"));
		exit(1);
	}

	if (option_version) {
		fprintf(stderr, _("sakura version is %s\n"), VERSION);
		exit(1);
	}

	if (option_ntabs <= 0) {
		option_ntabs=1;
	}

	g_option_context_free(context);

	gtk_init(&nargc, &nargv);

	g_strfreev(nargv);

	/* Init stuff */
	sakura_init();

	/* Add first tab */
	for (i=0; i<option_ntabs; i++)
		sakura_add_tab();
	
	sakura_sanitize_working_directory();

	gtk_main();

	return 0;
}
