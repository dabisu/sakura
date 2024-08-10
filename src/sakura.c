/*******************************************************************************
 *  Filename: sakura.c
 *  Description: VTE-based terminal emulator
 *
 *           Copyright (C) 2006-2021  David Gómez <david@pleyades.net>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#include <stdio.h>
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
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <vte/vte.h>

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

/* 16 color palettes in GdkRGBA format (red, green, blue, alpha) */

const GdkRGBA gruvbox_palette[PALETTE_SIZE] = {
	{0.156863, 0.156863, 0.156863, 1.000000},
	{0.800000, 0.141176, 0.113725, 1.000000},
	{0.596078, 0.592157, 0.101961, 1.000000},
	{0.843137, 0.600000, 0.129412, 1.000000},
	{0.270588, 0.521569, 0.533333, 1.000000},
	{0.694118, 0.384314, 0.525490, 1.000000},
	{0.407843, 0.615686, 0.415686, 1.000000},
	{0.658824, 0.600000, 0.517647, 1.000000},
	{0.572549, 0.513725, 0.454902, 1.000000},
	{0.984314, 0.286275, 0.203922, 1.000000},
	{0.721569, 0.733333, 0.149020, 1.000000},
	{0.980392, 0.741176, 0.184314, 1.000000},
	{0.513725, 0.647059, 0.596078, 1.000000},
	{0.827451, 0.525490, 0.607843, 1.000000},
	{0.556863, 0.752941, 0.486275, 1.000000},
	{0.921569, 0.858824, 0.698039, 1.000000}
};

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

const GdkRGBA solarized_palette[PALETTE_SIZE] = {
	{0.027451, 0.211765, 0.258824, 1}, // 0 base02
	{0.862745, 0.196078, 0.184314, 1}, // 1 red
	{0.521569, 0.600000, 0.000000, 1}, // 2 green
	{0.709804, 0.537255, 0.000000, 1}, // 3 yellow
	{0.149020, 0.545098, 0.823529, 1}, // 4 blue
	{0.827451, 0.211765, 0.509804, 1}, // 5 magenta
	{0.164706, 0.631373, 0.596078, 1}, // 6 cyan
	{0.933333, 0.909804, 0.835294, 1}, // 7 base2
	{0.000000, 0.168627, 0.211765, 1}, // 8 base03 (bg)
	{0.796078, 0.294118, 0.086275, 1}, // 9 orange
	{0.345098, 0.431373, 0.458824, 1}, // 10 base01
	{0.396078, 0.482353, 0.513725, 1}, // 11 base00
	{0.513725, 0.580392, 0.588235, 1}, // 12 base0 (fg)
	{0.423529, 0.443137, 0.768627, 1}, // 13 violet
	{0.576471, 0.631373, 0.631373, 1}, // 14 base1
	{0.992157, 0.964706, 0.890196, 1}  // 15 base3
};

const GdkRGBA nord_palette[PALETTE_SIZE] = {
	{0.0,        0.0,        0.0234375,  1.0},
	{0.74609375, 0.37890625, 0.4140625,  1.0},
	{0.63671875, 0.7421875,  0.546875,   1.0},
	{0.91796875, 0.79296875, 0.54296875, 1.0},
	{0.50390625, 0.62890625, 0.75390625, 1.0},
	{0.703125,   0.5546875,  0.67578125, 1.0},
	{0.53125,    0.75,       0.8125,     1.0},
	{0.89453125, 0.91015625, 0.9375,     1.0},
	{0.296875,   0.3359375,  0.4140625,  1.0},
	{0.74609375, 0.37890625, 0.4140625,  1.0},
	{0.63671875, 0.7421875,  0.546875,   1.0},
	{0.91796875, 0.79296875, 0.54296875, 1.0},
	{0.50390625, 0.62890625, 0.75390625, 1.0},
	{0.703125,   0.5546875,  0.67578125, 1.0},
	{0.55859375, 0.734375,   0.73046875, 1.0},
	{0.921875,   0.93359375, 0.953125,   1.0}
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

const GdkRGBA rxvt_palette[PALETTE_SIZE] = {
	{0,        0,        0,        1},
	{0.803921, 0,        0,        1},
	{0,        0.803921, 0,        1},
	{0.803921, 0.803921, 0,        1},
	{0,        0,        0.803921, 1},
	{0.803921, 0,        0.803921, 1},
	{0,        0.803921, 0.803921, 1},
	{0.980392, 0.921568, 0.843137, 1},
	{0.250980, 0.250980, 0.250980, 1},
	{1,        0,        0,        1},
	{0,        1,        0,        1},
	{1,        1,        0,        1},
	{0,        0,        1,        1},
	{1,        0,        1,        1},
	{0,        1,        1,        1},
	{1,        1,        1,        1}
};

const GdkRGBA hybrid_palette[PALETTE_SIZE] = {
	{0.1568627450980392  , 0.16470588235294117 , 0.1803921568627451  , 1} ,
	{0.6470588235294118  , 0.25882352941176473 , 0.25882352941176473 , 1} ,
	{0.5490196078431373  , 0.5803921568627451  , 0.25098039215686274 , 1} ,
	{0.8705882352941177  , 0.5764705882352941  , 0.37254901960784315 , 1} ,
	{0.37254901960784315 , 0.5058823529411764  , 0.615686274509804   , 1} ,
	{0.5215686274509804  , 0.403921568627451   , 0.5607843137254902  , 1} ,
	{0.3686274509803922  , 0.5529411764705883  , 0.5294117647058824  , 1} ,
	{0.4392156862745098  , 0.47058823529411764 , 0.5019607843137255  , 1} ,
	{0.21568627450980393 , 0.23137254901960785 , 0.2549019607843137  , 1} ,
	{0.8                 , 0.4                 , 0.4                 , 1} ,
	{0.7098039215686275  , 0.7411764705882353  , 0.40784313725490196 , 1} ,
	{0.9411764705882353  , 0.7764705882352941  , 0.4549019607843137  , 1} ,
	{0.5058823529411764  , 0.6352941176470588  , 0.7450980392156863  , 1} ,
	{0.6980392156862745  , 0.5803921568627451  , 0.7333333333333333  , 1} ,
	{0.5411764705882353  , 0.7450980392156863  , 0.7176470588235294  , 1} ,
	{0.7725490196078432  , 0.7843137254901961  , 0.7764705882352941  , 1}
};

const char *palettes_names[]= {"Solarized", "Tango", "Gruvbox", "Nord", "Xterm", "Linux", "Rxvt", "Hybrid", NULL};
const GdkRGBA *palettes[] = {solarized_palette, tango_palette, gruvbox_palette, nord_palette, xterm_palette, linux_palette, rxvt_palette, hybrid_palette, NULL};
#define DEFAULT_PALETTE 1 /* Tango palette */

/* Color schemes (fg&bg) for sakura. Each colorset can use a different scheme */
struct scheme {
	gchar *name;
	GdkRGBA bg;
	GdkRGBA fg;
};

#define NUM_SCHEMES 5
#define DEFAULT_SCHEME 1
struct scheme predefined_schemes[NUM_SCHEMES] = {
	{"Custom", {0, 0, 0, 1}, {1, 1, 1, 1}}, /* Custom values are ignored, we use the ones chosen by the user */
	{"White on black", {0, 0, 0, 1}, {1, 1, 1, 1}},
	{"Green on black", {0, 0, 0, 1}, {0.4, 1, 0, 1}},
	{"Solarized dark", {0.000000, 0.168627, 0.211765, 1}, {0.513725, 0.580392, 0.588235, 1}},
	{"Solarized light", {0.992157, 0.964706, 0.890196, 1}, {0.396078, 0.482353, 0.513725, 1}}
};

/* CSS definitions. Global CSS is empty, just drop here you CSS to personalize widgets */
#define SAKURA_CSS ""

#define FADE_WINDOW_CSS "\
window#fade_window {\
	background-color: black;\
} "

#define FADE_WINDOW_OPACITY 0.5

#define NUM_COLORSETS 6
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>



/* Tab bar visibility */
typedef enum {
	SHOW_TAB_BAR_ALWAYS,
	SHOW_TAB_BAR_MULTIPLE,
	SHOW_TAB_BAR_NEVER
} ShowTabBar;


/* Global sakura data */
static struct {
	GtkWidget *main_window;
	GtkWidget *notebook;
	GtkWidget *menu;
	GtkWidget *fade_window;  /* Window used for fading effect */
	PangoFontDescription *font;
	GdkRGBA forecolors[NUM_COLORSETS];
	GdkRGBA backcolors[NUM_COLORSETS];
	GdkRGBA curscolors[NUM_COLORSETS];
	guint schemes[NUM_COLORSETS];  /* Selected color scheme for each colorset */
	const GdkRGBA *palette;
	guint palette_idx;
	gint last_colorset;
	char *current_match;
	guint width;
	guint height;
	glong columns;
	glong rows;
	gint scroll_lines;
	VteCursorShape cursor_type;
	ShowTabBar show_tab_bar;         /* Show the tab bar: always, multiple, never */
	bool show_scrollbar;
	bool show_closebutton;
	bool new_tab_after_current;
	bool tabs_on_bottom;
	bool less_questions;
        bool copy_on_select;
	bool urgent_bell;
	bool audible_bell;
	bool blinking_cursor;
	bool fullscreen;
	bool config_modified;            /* Configuration has been modified */
	bool externally_modified;        /* Configuration file has been modified by another process */
	bool resized;
	bool disable_numbered_tabswitch; /* For disabling direct tabswitching key */
	bool use_fading;                 /* Fade the window when the focus change */
	bool scrollable_tabs;
	bool bold_is_bright;             /* Show bold characters as bright */
	bool dont_save;                  /* Don't save config file */
	bool first_run;                  /* To only execute commands first time sakura is launched */
	GtkWidget *item_copy_link;       /* We include here only the items which need to be hidden */
	GtkWidget *item_open_link;
	GtkWidget *item_open_mail;
	GtkWidget *open_link_separator;
	GKeyFile *cfg;
	char *configfile;
	char *icon;
	char *shell_path;
	char *main_title;		/* Main window static title from user input */
	char *term;
	gchar *tab_default_title;
	gint add_tab_accelerator;
	gint del_tab_accelerator;
	gint switch_tab_accelerator;
	gint move_tab_accelerator;
	gint copy_accelerator;
	gint scrollbar_accelerator;
	gint open_url_accelerator;
	gint font_size_accelerator;
	gint set_tab_name_accelerator;
	gint search_accelerator;
	gint set_colorset_accelerator;
	gint add_tab_key;
	gint del_tab_key;
	gint prev_tab_key;
	gint next_tab_key;
	gint copy_key;
	gint paste_key;
	gint scrollbar_key;
	gint set_tab_name_key;
	gint search_key;
	gint fullscreen_key;
	gint increase_font_size_key;
	gint decrease_font_size_key;
	gint set_colorset_keys[NUM_COLORSETS];
	gint paste_button;
	gint menu_button;
	VteRegex *http_vteregexp, *mail_vteregexp;
	char *word_chars;                /* Exceptions for word selection */
	char *argv[3];
} sakura;

/* Data associated to each sakura tab */
struct sakura_tab {
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vte;      /* Reference to VTE terminal */
	GtkWidget *scrollbar;
	GtkBorder padding;   /* inner-property data */
	bool label_set_byuser;
	int colorset;
	GPid pid;           /* pid of the forked process */
	gulong exit_handler_id;
};


#define ICON_FILE "terminal-tango.svg"
#define SCROLL_LINES 4096
#define DEFAULT_SCROLL_LINES 4096
#define HTTP_REGEXP "(ftp|http)s?://[^ \t\n\b]+[^.,!? \t\n\b()<>{}«»„“”‚‘’\\[\\]\'\"]"
#define MAIL_REGEXP "[^ \t\n\b()<>{}«»„“”‚‘’\\[\\]\'\"][^ \t\n\b]*@([^ \t\n\b()<>{}«»„“”‚‘’\\[\\]\'\"]+\\.)+([a-zA-Z]{2,})"
#define DEFAULT_CONFIGFILE "sakura.conf"
#define DEFAULT_COLUMNS 80
#define DEFAULT_ROWS 24
#define DEFAULT_MIN_WIDTH_CHARS 20
#define DEFAULT_MIN_HEIGHT_CHARS 1
#define DEFAULT_FONT "Ubuntu Mono,monospace 13"
#define FONT_MINIMAL_SIZE (PANGO_SCALE*6)
#define DEFAULT_WORD_CHARS "-,./?%&#_~:"
#define TAB_MAX_SIZE 40
#define TAB_MIN_SIZE 6
#define FORWARD 1
#define BACKWARDS 2
#define DEFAULT_ADD_TAB_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_DEL_TAB_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_SWITCH_TAB_ACCELERATOR  (GDK_MOD1_MASK)
#define DEFAULT_MOVE_TAB_ACCELERATOR (GDK_MOD1_MASK|GDK_SHIFT_MASK)
#define DEFAULT_COPY_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_SCROLLBAR_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_OPEN_URL_ACCELERATOR (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_FONT_SIZE_ACCELERATOR (GDK_CONTROL_MASK)
#define DEFAULT_SET_TAB_NAME_ACCELERATOR (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_SEARCH_ACCELERATOR (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_SELECT_COLORSET_ACCELERATOR (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_ADD_TAB_KEY  GDK_KEY_T
#define DEFAULT_DEL_TAB_KEY  GDK_KEY_W
#define DEFAULT_PREV_TAB_KEY  GDK_KEY_Left
#define DEFAULT_NEXT_TAB_KEY  GDK_KEY_Right
#define DEFAULT_COPY_KEY  GDK_KEY_C
#define DEFAULT_PASTE_KEY  GDK_KEY_V
#define DEFAULT_SCROLLBAR_KEY  GDK_KEY_S
#define DEFAULT_SET_TAB_NAME_KEY  GDK_KEY_N
#define DEFAULT_SEARCH_KEY  GDK_KEY_F
#define DEFAULT_FULLSCREEN_KEY  GDK_KEY_F11
#define DEFAULT_INCREASE_FONT_SIZE_KEY GDK_KEY_plus
#define DEFAULT_DECREASE_FONT_SIZE_KEY GDK_KEY_minus
#define DEFAULT_SCROLLABLE_TABS TRUE
#define DEFAULT_PASTE_BUTTON 2
#define DEFAULT_MENU_BUTTON 3

/* make this an array instead of #defines to get a compile time
 * error instead of a runtime if NUM_COLORSETS changes */
static int cs_keys[NUM_COLORSETS] =
		{GDK_KEY_F1, GDK_KEY_F2, GDK_KEY_F3, GDK_KEY_F4, GDK_KEY_F5, GDK_KEY_F6};

#define ERROR_BUFFER_LENGTH 256
const char cfg_group[] = "sakura";

/* Get a set sakura tab data from/to our GObject (notebook) */
static GQuark term_data_id = 0;
#define  sakura_get_sktab( sakura, page_idx )  \
    (struct sakura_tab*)g_object_get_qdata(  \
            G_OBJECT( gtk_notebook_get_nth_page( (GtkNotebook*)sakura.notebook, page_idx ) ), term_data_id);

#define  sakura_set_sktab( sakura, page_idx, sk_tab )  \
    g_object_set_qdata_full( \
            G_OBJECT( gtk_notebook_get_nth_page( (GtkNotebook*)sakura.notebook, page_idx) ), \
            term_data_id, sk_tab, (GDestroyNotify)g_free);

/* Configuration macros */
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


/* Spawn callback */
void sakura_spawm_callback (VteTerminal *, GPid, GError, gpointer);
/* VTE callbacks */
static gboolean sakura_term_buttonpressed_cb (GtkWidget *, GdkEventButton *, gpointer);
static gboolean sakura_term_buttonreleased_cb (GtkWidget *, GdkEventButton *, gpointer);
static void     sakura_beep_cb (GtkWidget *, void *);
static void     sakura_increase_font_cb (GtkWidget *, void *);
static void     sakura_decrease_font_cb (GtkWidget *, void *);
static void     sakura_child_exited_cb (GtkWidget *, void *);
static void     sakura_eof_cb (GtkWidget *, void *);
static void     sakura_title_changed_cb (GtkWidget *, void *);
static gboolean sakura_delete_event_cb (GtkWidget *, void *);
static void     sakura_destroy_window_cb (GtkWidget *, void *);
/* Main window callbacks */
static gboolean sakura_key_press_cb (GtkWidget *, GdkEventKey *, gpointer);
static gboolean sakura_resized_window_cb (GtkWidget *, GdkEventConfigure *, void *);
static gboolean sakura_focus_in_cb (GtkWidget *, GdkEvent *, void *);
static gboolean sakura_focus_out_cb (GtkWidget *, GdkEvent *, void *);
static void     sakura_conf_changed_cb (GtkWidget *, void *);
static void     sakura_show_event_cb (GtkWidget *, gpointer);
/* Notebook, notebook labels and notebook buttons callbacks */
static void     sakura_switch_page_cb (GtkWidget *, GtkWidget *, guint, void *);
static void     sakura_page_removed_cb (GtkWidget *, void *);
static gboolean sakura_notebook_scroll_cb (GtkWidget *, GdkEventScroll *);
static gboolean sakura_label_clicked_cb (GtkWidget *, GdkEventButton *, void *);
static gboolean sakura_notebook_focus_cb (GtkWindow *, GdkEvent *, void *);
static void     sakura_closebutton_clicked_cb (GtkWidget *, void *);
/* Menuitem callbacks */
static void     sakura_font_dialog_cb (GtkWidget *, void *);
static void     sakura_set_name_dialog_cb (GtkWidget *, void *);
static void     sakura_color_dialog_cb (GtkWidget *, void *);
//static void     sakura_set_title_dialog (GtkWidget *, void *);
static void     sakura_new_tab_cb (GtkWidget *, void *);
static void     sakura_close_tab_cb (GtkWidget *, void *);
static void     sakura_fullscreen_cb (GtkWidget *, void *);
static void     sakura_open_url_cb (GtkWidget *, void *);
static void     sakura_open_mail_cb (GtkWidget *, void *);
static void     sakura_copy_url_cb (GtkWidget *, void *);
static void     sakura_copy_cb (GtkWidget *, void *);
static void     sakura_paste_cb (GtkWidget *, void *);
static void     sakura_show_tab_bar_cb (GtkWidget *, void *);
static void     sakura_tabs_on_bottom_cb (GtkWidget *, void *);
static void     sakura_less_questions_cb (GtkWidget *, void *);
static void     sakura_copy_on_select_cb (GtkWidget *, void *);
static void     sakura_new_tab_after_current_cb (GtkWidget *, void *);
static void     sakura_show_scrollbar_cb (GtkWidget *, void *);
static void     sakura_disable_numbered_tabswitch_cb (GtkWidget *, void *);
//static void     sakura_use_fading_cb (GtkWidget *, void *);
static void     sakura_setname_entry_changed_cb (GtkWidget *, void *);
static void     sakura_set_cursor_cb (GtkWidget *, void *);
static void     sakura_blinking_cursor_cb (GtkWidget *, void *);
static void     sakura_audible_bell_cb (GtkWidget *, void *);
static void     sakura_urgent_bell_cb (GtkWidget *, void *);

/* Misc */
static void     sakura_error (const char *, ...);
static void     sakura_build_command (int *, char ***);
static char *   sakura_get_term_cwd (struct sakura_tab *);
static guint    sakura_tokeycode (guint key);
static void     sakura_set_keybind (const gchar *, guint);
static guint    sakura_get_keybind (const gchar *);
static void     sakura_sanitize_working_directory (void);

/* Functions */
static void     sakura_init ();
static void     sakura_init_popup ();
static void     sakura_add_tab ();
static void     sakura_del_tab (gint);
static void     sakura_close_tab (gint); /* Save config, del tab and destroy sakura */
static void     sakura_destroy ();
static void     sakura_move_tab (gint);
static gint     sakura_find_tab (VteTerminal *);
static void     sakura_set_font ();
static void     sakura_set_tab_label_text (const gchar *, gint);
static void     sakura_set_size (void);
static void     sakura_config_done ();
static void     sakura_set_colorset (int);
static void     sakura_set_colors (void);
static void     sakura_search_dialog (void);
static void     sakura_search (const char *, bool);
static void     sakura_copy (void);
static void     sakura_paste (void);
static void     sakura_paste_primary (void);
static void     sakura_show_scrollbar (void);


/* Globals for command line parameters */
static const char *option_font;
static const char *option_workdir;
static const char *option_execute;
static const char *option_title;
static gchar **option_xterm_args;
static gboolean option_xterm_execute=FALSE;
static gboolean option_version=FALSE;
static gint option_ntabs=1;
static gint option_login = FALSE;
static const char *option_icon;
static int option_rows, option_columns;
static gboolean option_hold=FALSE;
static char *option_config_file;
static gboolean option_fullscreen;
static gboolean option_maximize;
static gint option_colorset;


static GOptionEntry entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &option_version, N_("Print version number"), NULL },
	{ "title", 't', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &option_title, N_("Set window title"), NULL},
	{ "font", 'f', 0, G_OPTION_ARG_STRING, &option_font, N_("Select initial terminal font"), NULL },
	{ "ntabs", 'n', 0, G_OPTION_ARG_INT, &option_ntabs, N_("Select initial number of tabs"), NULL },
	{ "working-directory", 'd', 0, G_OPTION_ARG_STRING, &option_workdir, N_("Set working directory"), NULL },
	{ "execute", 'x', 0, G_OPTION_ARG_STRING, &option_execute, N_("Execute command"), NULL },
	{ "xterm-execute", 'e', 0, G_OPTION_ARG_NONE, &option_xterm_execute, N_("Execute command (last option in the command line)"), NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &option_xterm_args, NULL, NULL },
	{ "login", 'l', 0, G_OPTION_ARG_NONE, &option_login, N_("Login shell"), NULL },
	{ "icon", 'i', 0, G_OPTION_ARG_STRING, &option_icon, N_("Set window icon"), NULL },
	{ "columns", 'c', 0, G_OPTION_ARG_INT, &option_columns, N_("Set columns number"), NULL },
	{ "rows", 'r', 0, G_OPTION_ARG_INT, &option_rows, N_("Set rows number"), NULL },
	{ "hold", 'h', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &option_hold, N_("Hold window after execute command"), NULL },
	{ "maximize", 'm', 0, G_OPTION_ARG_NONE, &option_maximize, N_("Maximize window"), NULL },
	{ "fullscreen", 's', 0, G_OPTION_ARG_NONE, &option_fullscreen, N_("Fullscreen mode"), NULL },
	{ "config-file", 0, 0, G_OPTION_ARG_FILENAME, &option_config_file, N_("Use alternate configuration file"), NULL },
	{ "colorset", 0, 0, G_OPTION_ARG_INT, &option_colorset, N_("Select initial colorset"), NULL },
	{ NULL }
};


/*************************/
/* Main window callbacks */
/*************************/

static gboolean
sakura_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	gint page, npages;
	guint topage = 0;

	if (event->type != GDK_KEY_PRESS) return FALSE;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Use keycodes instead of keyvals. With keyvals, key bindings work only in US/ISO8859-1 and similar locales */
	guint keycode = event->hardware_keycode;

	/* Get the GDK accel mask to compare with our accelerators */
	GdkModifierType accel_mask = gtk_accelerator_get_default_mod_mask();

	/* Add/delete tab keybinding pressed */
	if ((event->state & accel_mask) == sakura.add_tab_accelerator && keycode == sakura_tokeycode(sakura.add_tab_key)) {
		sakura_add_tab();
		return TRUE;
	} else if ((event->state & accel_mask) == sakura.del_tab_accelerator && keycode == sakura_tokeycode(sakura.del_tab_key)) {
		/* Delete current tab */
		sakura_close_tab(page);
		return TRUE;
	}

	/* Switch tab keybinding pressed (numbers or next/prev) */
	//if ((event->state & accel_mask) == sakura.switch_tab_accelerator) {
	 /* If we use accel_mask, GDK_MOD4_MASK (windows key) it's not detected... */
        if ((event->state & sakura.switch_tab_accelerator) == sakura.switch_tab_accelerator) {

		/* Just propagate the event if there is only one tab */
		if (npages < 2) return FALSE;

		if ((keycode >= sakura_tokeycode(GDK_KEY_1)) && (keycode <= sakura_tokeycode( GDK_KEY_9))) {

			/* User has explicitly disabled this branch, make sure to propagate the event */
			if (sakura.disable_numbered_tabswitch) return FALSE;

			if      (sakura_tokeycode(GDK_KEY_1) == keycode) topage = 0;
			else if (sakura_tokeycode(GDK_KEY_2) == keycode) topage = 1;
			else if (sakura_tokeycode(GDK_KEY_3) == keycode) topage = 2;
			else if (sakura_tokeycode(GDK_KEY_4) == keycode) topage = 3;
			else if (sakura_tokeycode(GDK_KEY_5) == keycode) topage = 4;
			else if (sakura_tokeycode(GDK_KEY_6) == keycode) topage = 5;
			else if (sakura_tokeycode(GDK_KEY_7) == keycode) topage = 6;
			else if (sakura_tokeycode(GDK_KEY_8) == keycode) topage = 7;
			else if (sakura_tokeycode(GDK_KEY_9) == keycode) topage = 8;
			if (topage <= npages)
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), topage);
			return TRUE;
		} else if (keycode == sakura_tokeycode(sakura.prev_tab_key)) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==0) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), npages-1);
			} else {
				gtk_notebook_prev_page(GTK_NOTEBOOK(sakura.notebook));
			}
			return TRUE;
		} else if (keycode == sakura_tokeycode(sakura.next_tab_key)) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook)) == (npages-1)) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), 0);
			} else {
				gtk_notebook_next_page(GTK_NOTEBOOK(sakura.notebook));
			}
			return TRUE;
		}
	}

	/* Move tab keybinding pressed */
	if ((event->state & accel_mask) == sakura.move_tab_accelerator) {
		if (keycode == sakura_tokeycode(sakura.prev_tab_key)) {
			sakura_move_tab(BACKWARDS);
			return TRUE;
		} else if (keycode == sakura_tokeycode(sakura.next_tab_key)) {
			sakura_move_tab(FORWARD);
			return TRUE;
		}
	}

	/* Copy/paste keybinding pressed */
	if ((event->state & accel_mask) == sakura.copy_accelerator) {
		if (keycode == sakura_tokeycode(sakura.copy_key)) {
			sakura_copy();
			return TRUE;
		} else if (keycode == sakura_tokeycode(sakura.paste_key)) {
			sakura_paste();
			return TRUE;
		}
	}

	/* Show scrollbar keybinding pressed */
	if ((event->state & accel_mask) == sakura.scrollbar_accelerator) {
		if (keycode == sakura_tokeycode(sakura.scrollbar_key)) {
			sakura_show_scrollbar();
			return TRUE;
		}
	}

	/* Set tab name keybinding pressed */
	if ((event->state & accel_mask) == sakura.set_tab_name_accelerator) {
		if (keycode == sakura_tokeycode(sakura.set_tab_name_key)) {
			sakura_set_name_dialog_cb(NULL, NULL);
			return TRUE;
		}
	}

	/* Search keybinding pressed */
	if ((event->state & accel_mask) == sakura.search_accelerator) {
		if (keycode == sakura_tokeycode(sakura.search_key)) {
			sakura_search_dialog();
			return TRUE;
		}
	}

	/* Increase/decrease font size keybinding pressed */
	if ((event->state & accel_mask) == sakura.font_size_accelerator) {
		if (keycode == sakura_tokeycode(sakura.increase_font_size_key)) {
			sakura_increase_font_cb(NULL, NULL);
			return TRUE;
		} else if (keycode == sakura_tokeycode(sakura.decrease_font_size_key)) {
			sakura_decrease_font_cb(NULL, NULL);
			return TRUE;
		}
	}

	/* F11 (fullscreen) pressed */
	if (keycode == sakura_tokeycode(sakura.fullscreen_key)) {
		sakura_fullscreen_cb(NULL, NULL);
		return TRUE;
	}

	/* Change in colorset */
	if ((event->state & accel_mask) == sakura.set_colorset_accelerator) {
		int i;
		for (i=0; i<NUM_COLORSETS; i++) {
			if (keycode == sakura_tokeycode(sakura.set_colorset_keys[i])) {
				sakura_set_colorset(i);
				return TRUE;
			}
		}
	}
	return FALSE;
}


static gboolean
sakura_resized_window_cb (GtkWidget *widget, GdkEventConfigure *event, void *data)
{
	if (event->width != sakura.width || event->height != sakura.height) {
		//SAY("Configure event received. Current w %d h %d ConfigureEvent w %d h %d",
		//sakura.width, sakura.height, event->width, event->height);
		gtk_widget_hide(sakura.fade_window);
		sakura.resized = TRUE;
	}

	return FALSE;
}

/* Use focus-in-event to unmap the fade window */
static gboolean
sakura_focus_in_cb (GtkWidget *widget, GdkEvent *event, void *data)
{
	if (event->type != GDK_FOCUS_CHANGE) return FALSE;
	//if (!sakura.use_fading) return FALSE;

	/* Got the focus, hide the fade */
	//gtk_widget_hide(sakura.fade_window);

	/* Reset urgency hint */
	gtk_window_set_urgency_hint(GTK_WINDOW(sakura.main_window), FALSE);

	return FALSE;
}


/* Use focus-out-event to map the fade window */
static gboolean
sakura_focus_out_cb (GtkWidget *widget, GdkEvent *event, void *data)
{
	gint ax, ay, mx, my, x, y;

	if (event->type != GDK_FOCUS_CHANGE) return FALSE;
	if (!sakura.use_fading) return FALSE;

	/* No fade when the menu is displayed */
	if (gtk_widget_is_visible(sakura.menu)) return FALSE;

	/* Give the right size and position to the fade_window to cover all the main window */
	gtk_widget_translate_coordinates(sakura.notebook, sakura.main_window, 0, 0, &ax, &ay);
	gtk_window_get_position(GTK_WINDOW(sakura.main_window), &mx, &my);
	gint titlebar_height = ay-my;
	gtk_window_move(GTK_WINDOW(sakura.fade_window), mx, my+titlebar_height);
	//SAY("FADE ax %d ay %d x %d y %d titlebar_h %d", ax, ay, mx, my, titlebar_height);

	/* Same size as main window */
	gtk_window_get_size(GTK_WINDOW(sakura.main_window), &x, &y);
	gtk_window_resize(GTK_WINDOW(sakura.fade_window), x, y);

	//gtk_widget_show_all(sakura.fade_window);

	return FALSE;
}


static void
sakura_show_event_cb (GtkWidget *widget, gpointer data)
{
	/* Set size when the window is first shown */
	sakura_set_size();
}


/* Callback called when sakura configuration file is modified by an external process */
static void
sakura_conf_changed_cb (GtkWidget *widget, void *data)
{
	sakura.externally_modified = true;
}



/**********************/
/* Notebook callbacks */
/**********************/


/* Handler for notebook scroll-event - switches tabs by scroll direction */
static gboolean
sakura_notebook_scroll_cb (GtkWidget *widget, GdkEventScroll *event)
{
	/* This callback cause undesirable scroll (when the mouse is over the vte window) when using
	 * input methods like hime. Disable it by now */

	/*
	gint page, npages;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	switch (event->direction) {
		case GDK_SCROLL_DOWN:
			gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), --page >= 0 ? page : npages - 1);
			break;
		case GDK_SCROLL_UP:
			gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), ++page < npages ? page : 0);
			break;
		case GDK_SCROLL_LEFT:
		case GDK_SCROLL_RIGHT:
		case GDK_SCROLL_SMOOTH:
			break;
	}
	*/

	return FALSE;
}


/* Callback called when the user switches tabs or closes a tab (but not when a tab is added) */
static void
sakura_switch_page_cb (GtkWidget *widget, GtkWidget *widget_page, guint page_num, void *data)
{
	struct sakura_tab *sk_tab;

	/* Don't use gtk_notebook_get_current_page in the callbacks, it returns the previous page */

	sk_tab = sakura_get_sktab(sakura, page_num);

	/* Update the window title when a new tab is selected, but don't when an user title has been set */
	//if (!sakura.tab_default_title && !sakura.main_title)
	if (!sakura.main_title) {
		if (g_strcmp0(gtk_label_get_text(GTK_LABEL(sk_tab->label)),"")!=0) {
			gtk_window_set_title(GTK_WINDOW(sakura.main_window), gtk_label_get_text(GTK_LABEL(sk_tab->label)));
		}
	}

}


static void
sakura_page_removed_cb (GtkWidget *widget, void *data)
{
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==1) {
		/* If the first tab is disabled, window size changes and we need to recalculate its size */
		sakura_set_size();
	}
}


/* Callback for focus-in-event to the notebook widget */
static gboolean
sakura_notebook_focus_cb (GtkWindow *window, GdkEvent *event, void *data)
{
	struct sakura_tab *sk_tab; gint page;

	if (event->type != GDK_FOCUS_CHANGE) return FALSE;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	/* When clicking several times in the label, terminal loses its focus.
	 * So, when the notebook got the focus, make sure the terminal HAS te focus */
	gtk_widget_grab_focus(sk_tab->vte);

	return FALSE;
}


/* Callback for clicking in the tabs close buttons */
static void
sakura_closebutton_clicked_cb (GtkWidget *widget, void *data)
{
	GtkWidget *hbox = (GtkWidget *)data;
	gint page;

	page = gtk_notebook_page_num(GTK_NOTEBOOK(sakura.notebook), hbox);

	sakura_close_tab(page);
}


/* Callback for clicking in the tabs labels */
static gboolean
sakura_label_clicked_cb (GtkWidget *widget, GdkEventButton *button_event, void *data)
{
	GtkWidget *hbox = (GtkWidget *)data;
	struct sakura_tab *sk_tab;
	gint page;

	page = gtk_notebook_page_num(GTK_NOTEBOOK(sakura.notebook), hbox);
	sk_tab = sakura_get_sktab(sakura, page);

	/* Not interested in non button press events */
	if (button_event->type != GDK_BUTTON_PRESS)
		return FALSE;

	/* Left button click. We HAVE to propagate the event, or things like tab moving won't work */
	if (button_event->button == 1) {
		gtk_widget_grab_focus(sk_tab->vte);
		return FALSE;
	}

	/* Ignore right click and propagate the event */
	if (button_event->button == 3)
		return FALSE;

	/* The middle button was clicked, so close the tab */
	sakura_close_tab(page);

	return TRUE;
}


/*****************/
/* VTE callbacks */
/*****************/


/* Callback for button release on the vte terminal. Used for copy-on-selection to clipboard */
static gboolean
sakura_term_buttonreleased_cb (GtkWidget *widget, GdkEventButton *button_event, gpointer user_data)
{

	if (button_event->type != GDK_BUTTON_RELEASE)
		return FALSE;

	if (sakura.copy_on_select)
		if (button_event->button == 1)
			sakura_copy();

	return FALSE;
}


static gboolean
sakura_term_buttonpressed_cb (GtkWidget *widget, GdkEventButton *button_event, gpointer user_data)
{
	struct sakura_tab *sk_tab;
	gint page, tag;

	if (button_event->type != GDK_BUTTON_PRESS)
		return FALSE;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	/* Find out if cursor it's over a matched expression...*/
	sakura.current_match = vte_terminal_match_check_event(VTE_TERMINAL(sk_tab->vte), (GdkEvent *) button_event, &tag);

	/* Left button with accelerator: open the URL if any */
	if (button_event->button == 1 &&
	    ((button_event->state & sakura.open_url_accelerator) == sakura.open_url_accelerator) &&
	    sakura.current_match) {

		sakura_open_url_cb(NULL, NULL);

		return TRUE;
	}

	/* Paste when paste button is pressed */
	if (sakura.copy_on_select) {
		if (button_event->button == sakura.paste_button) {
			sakura_paste_primary(); /* This is the expected X11 behaviour, to copy the PRIMARY clipboard with the middle click. 
						   TODO: Maybe add an option to use the secondary one? */

			/* Do not propagate. vte has his own copy-on-select and we'll end with duplicates pastes */
			return TRUE;
		}
	}

	/* Show the popup menu when menu button is pressed */
	if (button_event->button == sakura.menu_button) {
		GtkMenu *menu;

		menu = GTK_MENU (user_data);

		if (sakura.current_match) {
			/* Show the extra options in the menu */

			char *matches;
			/* Is it a mail address? */
			if (vte_terminal_event_check_regex_simple(VTE_TERMINAL(sk_tab->vte), (GdkEvent *) button_event,
								  &sakura.mail_vteregexp, 1, 0, &matches)) {
				gtk_widget_show(sakura.item_open_mail);
				gtk_widget_hide(sakura.item_open_link);
			} else {
				gtk_widget_show(sakura.item_open_link);
				gtk_widget_hide(sakura.item_open_mail);
			}
			gtk_widget_show(sakura.item_copy_link);
			gtk_widget_show(sakura.open_link_separator);

			g_free(matches);
		} else {
			/* Hide all the options */
			gtk_widget_hide(sakura.item_open_mail);
			gtk_widget_hide(sakura.item_open_link);
			gtk_widget_hide(sakura.item_copy_link);
			gtk_widget_hide(sakura.open_link_separator);
		}

		gtk_menu_popup_at_pointer(menu, (GdkEvent *) button_event);

		return TRUE;
	}

	return FALSE;
}


static void
sakura_beep_cb (GtkWidget *widget, void *data)
{
	/* Remove the urgency hint. This is necessary to signal the window manager  */
	/* that a new urgent event happened when the urgent hint is set after this. */
	/* TODO: this is already set in focus_in, so DO we really need it here? */
	gtk_window_set_urgency_hint(GTK_WINDOW(sakura.main_window), FALSE);

	/* If the window is active(focused), ignore and don't set the urgency hint */
	if (!gtk_window_is_active(GTK_WINDOW(sakura.main_window))) {
		if (sakura.urgent_bell) {
			gtk_window_set_urgency_hint(GTK_WINDOW(sakura.main_window), TRUE);
	}
	}

}


static void
sakura_increase_font_cb (GtkWidget *widget, void *data)
{
	gint new_size;

	/* Increment font size one unit */
	new_size = pango_font_description_get_size(sakura.font)+PANGO_SCALE;

	pango_font_description_set_size(sakura.font, new_size);
	sakura_set_font();
	sakura_set_size();
	sakura_set_config_string("font", pango_font_description_to_string(sakura.font));
}


static void
sakura_decrease_font_cb (GtkWidget *widget, void *data)
{
	gint new_size;

	/* Decrement font size one unit */
	new_size = pango_font_description_get_size(sakura.font)-PANGO_SCALE;

	/* Set a minimal size */
	if (new_size >= FONT_MINIMAL_SIZE) {
		pango_font_description_set_size(sakura.font, new_size);
		sakura_set_font();
		sakura_set_size();
		sakura_set_config_string("font", pango_font_description_to_string(sakura.font));
	}
}


static void
sakura_child_exited_cb (GtkWidget *widget, void *data)
{
	gint page, npages;
	struct sakura_tab *sk_tab;

	page = gtk_notebook_page_num(GTK_NOTEBOOK(sakura.notebook),
				gtk_widget_get_parent(widget));
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	/* Only write configuration to disk if it's the last tab */
	if (npages==1) {
		sakura_config_done();
	}

	if (option_hold==TRUE) {
		SAY("hold option has been activated");
		return;
	}

	/* Child should be automatically reaped because we don't use G_SPAWN_DO_NOT_REAP_CHILD flag */
	g_spawn_close_pid(sk_tab->pid);

	sakura_del_tab(page);

	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	if (npages == 0)
		sakura_destroy();
}


static void
sakura_eof_cb (GtkWidget *widget, void *data)
{
	SAY("Got EOF signal");
}

/* This handler is called when vte window title changes (i.e.: cwd changes),
 * and it is used to change window and notebook pages titles */
static void
sakura_title_changed_cb (GtkWidget *widget, void *data)
{
	struct sakura_tab *sk_tab;
	const char *tabtitle;
	gint modified_page;
	VteTerminal *vte_term=(VteTerminal *)widget;

	modified_page = sakura_find_tab(vte_term);
	sk_tab = sakura_get_sktab(sakura, modified_page);

	tabtitle = vte_terminal_get_window_title(VTE_TERMINAL(sk_tab->vte));

	/* User set values overrides any other one */
	if (!sk_tab->label_set_byuser) {
		sakura_set_tab_label_text(tabtitle, modified_page);
		if (!sakura.main_title) gtk_window_set_title(GTK_WINDOW(sakura.main_window), tabtitle);
	}

}


static gboolean
sakura_delete_event_cb (GtkWidget *widget, void *data)
{
	struct sakura_tab *sk_tab;
	GtkWidget *dialog;
	gint response;
	gint npages;
	gint i;
	pid_t pgid;

	if (!sakura.less_questions) {
		npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

		/* Check for each tab if there are running processes. Use tcgetpgrp to compare to the shell PGID */
		for (i=0; i < npages; i++) {

			sk_tab = sakura_get_sktab(sakura, i);
			pgid = tcgetpgrp(vte_pty_get_fd(vte_terminal_get_pty(VTE_TERMINAL(sk_tab->vte))));

			/* If running processes are found, we ask one time and exit */
			if ( (pgid != -1) && (pgid != sk_tab->pid)) {
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
sakura_destroy_window_cb (GtkWidget *widget, void *data)
{
	sakura_destroy();
}


/**********************/
/* Menuitem callbacks */
/**********************/


static void
sakura_font_dialog_cb (GtkWidget *widget, void *data)
{
	GtkWidget *font_dialog;
	gint response;

	font_dialog = gtk_font_chooser_dialog_new(_("Select font"), GTK_WINDOW(sakura.main_window));
	gtk_font_chooser_set_font_desc(GTK_FONT_CHOOSER(font_dialog), sakura.font);

	response = gtk_dialog_run(GTK_DIALOG(font_dialog));

	if (response == GTK_RESPONSE_OK) {
		pango_font_description_free(sakura.font);
		sakura.font = gtk_font_chooser_get_font_desc(GTK_FONT_CHOOSER(font_dialog));
		sakura_set_font();
		sakura_set_size();
		sakura_set_config_string("font", pango_font_description_to_string(sakura.font));
	}

	gtk_widget_destroy(font_dialog);
}


static void
sakura_set_name_dialog_cb (GtkWidget *widget, void *data)
{
	GtkWidget *input_dialog, *input_header;
	GtkWidget *entry, *label;
	GtkWidget *name_hbox; /* We need this for correct spacing */
	gint response;
	gint page;
	struct sakura_tab *sk_tab;
	const gchar *text;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	input_dialog=gtk_dialog_new_with_buttons(_("Set tab name"),
	                                         GTK_WINDOW(sakura.main_window),
                                                 GTK_DIALOG_MODAL|GTK_DIALOG_USE_HEADER_BAR,
	                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                         _("_Apply"), GTK_RESPONSE_ACCEPT,
	                                         NULL);

	/* Configure the new gtk header bar*/
	input_header = gtk_dialog_get_header_bar(GTK_DIALOG(input_dialog));
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(input_header), FALSE);

	gtk_dialog_set_default_response(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT);

	/* Create dialog contents */
	name_hbox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	entry=gtk_entry_new();
	label=gtk_label_new(_("New text"));
	/* Set tab label as entry default text (when first tab is not displayed, get_tab_label_text
	   returns a null value, so check accordingly */
	/* FIXME: Check why is returning NULL */
	text = gtk_notebook_get_tab_label_text(GTK_NOTEBOOK(sakura.notebook), sk_tab->hbox);
	if (text) {
		SAY("TEXT %s", text);
		gtk_entry_set_text(GTK_ENTRY(entry), text);
	}
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(name_hbox), label, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(name_hbox), entry, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(input_dialog))), name_hbox, FALSE, FALSE, 12);

	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(sakura_setname_entry_changed_cb), input_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT, FALSE);

	gtk_widget_show_all(name_hbox);

	response = gtk_dialog_run(GTK_DIALOG(input_dialog));

	if (response == GTK_RESPONSE_ACCEPT) {
		sakura_set_tab_label_text(gtk_entry_get_text(GTK_ENTRY(entry)), page);
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), gtk_entry_get_text(GTK_ENTRY(entry)));
		sk_tab->label_set_byuser=true; 
		sakura.main_title=NULL; /* Ignore the user-set window title if the user names the tab */
	}

	gtk_widget_destroy(input_dialog);
}



/* Callback for the color dialog signals. Used to UPDATE the contents of that dialog (passed as 'data') */
static void
sakura_color_dialog_changed_cb ( GtkWidget *widget, void *data)
{
	GtkDialog *dialog = (GtkDialog*) data;
	GtkColorButton *fore_button = g_object_get_data (G_OBJECT(dialog), "fore_button");
	GtkColorButton *back_button = g_object_get_data (G_OBJECT(dialog), "back_button");
	GtkColorButton *curs_button = g_object_get_data (G_OBJECT(dialog), "curs_button");
	GdkRGBA *forecolors = g_object_get_data (G_OBJECT(dialog), "fore");
	GdkRGBA *backcolors = g_object_get_data (G_OBJECT(dialog), "back");
	GdkRGBA *curscolors = g_object_get_data (G_OBJECT(dialog), "curs");
	GtkComboBox *cs_combo = g_object_get_data (G_OBJECT(dialog), "cs_combo");
	GtkComboBox *scheme_combo = g_object_get_data (G_OBJECT(dialog), "scheme_combo");
	GtkSpinButton *opacity_spin = g_object_get_data (G_OBJECT(dialog), "opacity_spin");
	GtkCheckButton *bib_checkbutton = g_object_get_data (G_OBJECT(dialog), "bib_checkbutton");

	gint current_cs = gtk_combo_box_get_active(cs_combo);

	/* If we come here as a result of a change in the active colorset, load the new colorset to the buttons.
	 * Else, the color buttons or opacity spin have gotten a new value, store that. */
	if ((GtkWidget *)cs_combo == widget ) {
		/* Spin opacity is a percentage, convert it*/
		gint new_opacity = (int) (backcolors[current_cs].alpha*100);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(fore_button), &forecolors[current_cs]);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(back_button), &backcolors[current_cs]);
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(curs_button), &curscolors[current_cs]);
		gtk_spin_button_set_value(opacity_spin, new_opacity);
		gtk_combo_box_set_active(GTK_COMBO_BOX(scheme_combo), sakura.schemes[current_cs]);
	} else if ((GtkWidget *)scheme_combo == widget) {
		/* Scheme has changed, update the buttons. No cursor and no alpha */
		int selected_scheme = gtk_combo_box_get_active(GTK_COMBO_BOX(scheme_combo));
		if (selected_scheme != 0) {
			float old_alpha = backcolors[current_cs].alpha; /* Keep the previous alpha */
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(fore_button), &predefined_schemes[selected_scheme].fg);
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(back_button), &predefined_schemes[selected_scheme].bg);
			forecolors[current_cs] = predefined_schemes[selected_scheme].fg;
			backcolors[current_cs] = predefined_schemes[selected_scheme].bg;
			backcolors[current_cs].alpha = old_alpha;
			sakura.schemes[current_cs] = selected_scheme;
		} /* else Custom, do nothing */
	} else if ((GtkWidget *)bib_checkbutton == widget) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bib_checkbutton))) {
			sakura.bold_is_bright = true;
		}
		else {
			sakura.bold_is_bright = false;
		}
	} else {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(fore_button), &forecolors[current_cs]);
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(back_button), &backcolors[current_cs]);
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(curs_button), &curscolors[current_cs]);
		gtk_spin_button_update(opacity_spin);
		backcolors[current_cs].alpha = gtk_spin_button_get_value(opacity_spin)/100;
		/* User changed colors. Set custom scheme */
		sakura.schemes[current_cs] = 0;
		gtk_combo_box_set_active(GTK_COMBO_BOX(scheme_combo), sakura.schemes[current_cs]);
	}

}


/* Dialog to select foreground, background and cursors colors, transparency and palette */
static void
sakura_color_dialog_cb (GtkWidget *widget, void *data)
{
	GtkWidget *color_dialog; GtkWidget *color_header;
	GtkWidget *cs_label, *scheme_label, *fore_label, *back_label, *curs_label, *opacity_label, *palette_label;
	GtkWidget *cs_combo, *scheme_combo, *fore_button, *back_button, *curs_button, *palette_combo, *opacity_spin;
	GtkWidget *cs_hbox, *scheme_hbox, *fore_hbox, *back_hbox, *curs_hbox, *opacity_hbox, *palette_hbox, *bib_hbox;
	GtkWidget *bib_checkbutton;
	GdkRGBA temp_fore[NUM_COLORSETS]; GdkRGBA temp_back[NUM_COLORSETS];	GdkRGBA temp_curs[NUM_COLORSETS];
	GtkAdjustment *spin_adj;
	struct sakura_tab *sk_tab;
	gint response;
	gint page, i;


	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	color_dialog = gtk_dialog_new_with_buttons(_("Select colors"), GTK_WINDOW(sakura.main_window),
	                                           GTK_DIALOG_MODAL|GTK_DIALOG_USE_HEADER_BAR,
	                                           _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Select"), GTK_RESPONSE_ACCEPT, NULL);

	/* Configure the new gtk header bar */
	color_header = gtk_dialog_get_header_bar(GTK_DIALOG(color_dialog));
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(color_header), FALSE);
	gtk_dialog_set_default_response(GTK_DIALOG(color_dialog), GTK_RESPONSE_ACCEPT);

	/* Add the combobox to select the current colorset */
	cs_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	cs_label = gtk_label_new(_("Colorset"));
	cs_combo = gtk_combo_box_text_new();
	gchar combo_text[3];
	for (i=0; i < NUM_COLORSETS; i++) {
		g_snprintf(combo_text, 2, "%d", i+1);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cs_combo), NULL, combo_text);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(cs_combo), sk_tab->colorset);

	/* Add the scheme combobox */
	scheme_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	scheme_label = gtk_label_new(_("Color scheme"));
	scheme_combo = gtk_combo_box_text_new();
	for (i=0; i < NUM_SCHEMES; i++) {
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(scheme_combo), NULL, predefined_schemes[i].name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(scheme_combo), sakura.schemes[sk_tab->colorset]);

	/* Foreground and background and cursor color buttons */
	fore_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	back_hbox = gtk_box_new(FALSE, 12);
	curs_hbox = gtk_box_new(FALSE, 12);
	fore_label = gtk_label_new(_("Foreground color"));
	back_label = gtk_label_new(_("Background color"));
	curs_label = gtk_label_new(_("Cursor color"));
	fore_button = gtk_color_button_new_with_rgba(&sakura.forecolors[sk_tab->colorset]);
	back_button = gtk_color_button_new_with_rgba(&sakura.backcolors[sk_tab->colorset]);
	curs_button = gtk_color_button_new_with_rgba(&sakura.curscolors[sk_tab->colorset]);

	/* Opacity control */
	opacity_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	spin_adj = gtk_adjustment_new ((sakura.backcolors[sk_tab->colorset].alpha)*100, 0.0, 100.0, 1.0, 5.0, 0);
	opacity_spin = gtk_spin_button_new(GTK_ADJUSTMENT(spin_adj), 1.0, 0);
	opacity_label = gtk_label_new(_("Opacity level (%)"));

	/* Palette combobox */
	palette_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	palette_label = gtk_label_new(_("Palette"));
	palette_combo = gtk_combo_box_text_new();
	for (i=0; i < (sizeof(palettes_names)) / (sizeof(palettes_names[0])); i++) {
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(palette_combo), NULL, palettes_names[i]);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(palette_combo), sakura.palette_idx);

	/* Bold is bright checkbutton */
	bib_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	bib_checkbutton = gtk_check_button_new_with_label(_("Use bright colors for bold text"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bib_checkbutton), sakura.bold_is_bright);

	gtk_box_pack_start(GTK_BOX(cs_hbox), cs_label, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(cs_hbox), cs_combo, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(scheme_hbox), scheme_label, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(scheme_hbox), scheme_combo, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(fore_hbox), fore_label, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(fore_hbox), fore_button, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(back_hbox), back_label, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(back_hbox), back_button, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(curs_hbox), curs_label, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(curs_hbox), curs_button, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(opacity_hbox), opacity_label, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(opacity_hbox), opacity_spin, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(palette_hbox), palette_label, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(palette_hbox), palette_combo, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(bib_hbox), bib_checkbutton, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), cs_hbox, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), scheme_hbox, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), fore_hbox, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), back_hbox, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), curs_hbox, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), opacity_hbox, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), palette_hbox, FALSE, FALSE, 6);
	gtk_box_pack_end(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog))), bib_hbox, FALSE, FALSE, 6);

	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(color_dialog)));

	/* When the user switches the colorset, callback needs access to these selector widgets */
	g_object_set_data(G_OBJECT(color_dialog), "cs_combo", cs_combo);
	g_object_set_data(G_OBJECT(color_dialog), "scheme_combo", scheme_combo);
	g_object_set_data(G_OBJECT(color_dialog), "fore_button", fore_button);
	g_object_set_data(G_OBJECT(color_dialog), "back_button", back_button);
	g_object_set_data(G_OBJECT(color_dialog), "curs_button", curs_button);
	g_object_set_data(G_OBJECT(color_dialog), "opacity_spin", opacity_spin);
	g_object_set_data(G_OBJECT(color_dialog), "fore", temp_fore);
	g_object_set_data(G_OBJECT(color_dialog), "back", temp_back);
	g_object_set_data(G_OBJECT(color_dialog), "curs", temp_curs);
	g_object_set_data(G_OBJECT(color_dialog), "bib_checkbutton", bib_checkbutton);

	g_signal_connect(G_OBJECT(cs_combo), "changed", G_CALLBACK(sakura_color_dialog_changed_cb), color_dialog);
	g_signal_connect(G_OBJECT(scheme_combo), "changed", G_CALLBACK(sakura_color_dialog_changed_cb), color_dialog);
	g_signal_connect(G_OBJECT(fore_button), "color-set", G_CALLBACK(sakura_color_dialog_changed_cb), color_dialog);
	g_signal_connect(G_OBJECT(back_button), "color-set", G_CALLBACK(sakura_color_dialog_changed_cb), color_dialog);
	g_signal_connect(G_OBJECT(curs_button), "color-set", G_CALLBACK(sakura_color_dialog_changed_cb), color_dialog);
	g_signal_connect(G_OBJECT(opacity_spin), "changed", G_CALLBACK(sakura_color_dialog_changed_cb), color_dialog);
	g_signal_connect(G_OBJECT(bib_checkbutton), "toggled", G_CALLBACK(sakura_color_dialog_changed_cb), color_dialog);

	for (i=0; i<NUM_COLORSETS; i++) {
		temp_fore[i] = sakura.forecolors[i];
		temp_back[i] = sakura.backcolors[i];
		temp_curs[i] = sakura.curscolors[i];
	}

	response = gtk_dialog_run(GTK_DIALOG(color_dialog));

	if (response==GTK_RESPONSE_ACCEPT) {
		/* Save all colorsets to both the global struct and configuration.*/
		for (i=0; i<NUM_COLORSETS; i++) {
			char name[20];
			gchar *cfgtmp;

			sakura.forecolors[i] = temp_fore[i];
			sakura.backcolors[i] = temp_back[i];
			sakura.curscolors[i] = temp_curs[i];

			sprintf(name, "colorset%d_fore", i+1);
			cfgtmp = gdk_rgba_to_string(&sakura.forecolors[i]);
			sakura_set_config_string(name, cfgtmp);
			g_free(cfgtmp);

			sprintf(name, "colorset%d_back", i+1);
			cfgtmp = gdk_rgba_to_string(&sakura.backcolors[i]);
			sakura_set_config_string(name, cfgtmp);
			g_free(cfgtmp);

			sprintf(name, "colorset%d_curs", i+1);
			cfgtmp = gdk_rgba_to_string(&sakura.curscolors[i]);
			sakura_set_config_string(name, cfgtmp);
			g_free(cfgtmp);

			sprintf(name, "colorset%d_scheme", i+1);
			sakura_set_config_integer(name, sakura.schemes[i]);
		}

		/* Set the current tab's colorset to the last selected one in the dialog.
		 * This is probably what the new user expects, and the experienced user hopefully will not mind. */
		sk_tab->colorset = gtk_combo_box_get_active(GTK_COMBO_BOX(cs_combo));
		sakura_set_config_integer("last_colorset", sk_tab->colorset+1);

		/* Set the selected palette */
		guint palette_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(palette_combo));
		sakura.palette = palettes[palette_idx];
		sakura.palette_idx = palette_idx;
		sakura_set_config_integer("palette", sakura.palette_idx);

		/* Set bold is bright option */
		sakura_set_config_boolean("bold_is_bright", sakura.bold_is_bright);

		/* Apply the new colorsets to all tabs */
		sakura_set_colors();
	}

	gtk_widget_destroy(color_dialog);
}


#if 0
static void
sakura_set_title_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *title_dialog, *title_header;
	GtkWidget *entry, *label;
	GtkWidget *title_hbox;
	gint response;

	title_dialog=gtk_dialog_new_with_buttons(_("Set window title"),
	                                         GTK_WINDOW(sakura.main_window),
	                                         GTK_DIALOG_MODAL|GTK_DIALOG_USE_HEADER_BAR,
	                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                         _("_Apply"), GTK_RESPONSE_ACCEPT,
	                                          NULL);

	/* Configure the new gtk header bar*/
	title_header=gtk_dialog_get_header_bar(GTK_DIALOG(title_dialog));
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(title_header), FALSE);
	gtk_dialog_set_default_response(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT);

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
		/* Bug #257391 shadow reaches here too... */
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	gtk_widget_destroy(title_dialog);
}
#endif


static void
sakura_copy_url_cb (GtkWidget *widget, void *data)
{
	GtkClipboard* clip;

	clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clip, sakura.current_match, -1 );
	//clip = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	//gtk_clipboard_set_text(clip, sakura.current_match, -1 );

}


static void
sakura_open_url_cb (GtkWidget *widget, void *data)
{
	GError *error=NULL;
	gchar *browser=NULL;

	SAY("Opening %s", sakura.current_match);

	browser = g_strdup(g_getenv("BROWSER"));

	if (!browser) {
		if ( !(browser = g_find_program_in_path("xdg-open")) ) {
			sakura_error("Browser not found");
		}
	}

	if (browser) {
		gchar * argv[] = {browser, sakura.current_match, NULL};
		if (!g_spawn_async(".", argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
			sakura_error("Couldn't exec \"%s %s\": %s", browser, sakura.current_match, error->message);
			g_error_free(error);
		}

		g_free(browser);
	}
}


static void
sakura_open_mail_cb (GtkWidget *widget, void *data)
{
	GError *error = NULL;
	gchar *program = NULL;

	if ( (program = g_find_program_in_path("xdg-email")) ) {
		gchar * argv[] = { program, sakura.current_match, NULL };
		if (!g_spawn_async(".", argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
			sakura_error("Couldn't exec \"%s %s\": %s", program, sakura.current_match, error->message);
		}
		g_free(program);
	}
}


static void
sakura_show_tab_bar_cb (GtkWidget *widget, void *data)
{
	char *setting_string = (char *)data;
	char *config_string;
	gboolean show_tabs;

	if (strcmp(setting_string, "always")==0) {
		sakura.show_tab_bar = SHOW_TAB_BAR_ALWAYS;
		config_string = "always";
		show_tabs = TRUE;
	} else if (strcmp(setting_string, "multiple")==0) {
		sakura.show_tab_bar = SHOW_TAB_BAR_MULTIPLE;
		config_string = "multiple";
		show_tabs = (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) != 1);
	} else if (strcmp(setting_string, "never")==0) {
		sakura.show_tab_bar = SHOW_TAB_BAR_NEVER;
		config_string = "never";
		show_tabs = FALSE;
	}

	sakura_set_config_string("show_tab_bar", config_string);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), show_tabs);

	sakura_set_size();
}


static void
sakura_tabs_on_bottom_cb (GtkWidget *widget, void *data)
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
sakura_less_questions_cb (GtkWidget *widget, void *data)
{

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		sakura.less_questions = TRUE;
		sakura_set_config_boolean("less_questions", TRUE);
	} else {
		sakura.less_questions = FALSE;
		sakura_set_config_boolean("less_questions", FALSE);
	}
}


static void
sakura_copy_on_select_cb (GtkWidget *widget, void *data)
{
        sakura.copy_on_select = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
        if (sakura.copy_on_select) {
                sakura_set_config_boolean("copy_on_select", TRUE);
        } else {
                sakura_set_config_boolean("copy_on_select", FALSE);
        }
}


static void
sakura_new_tab_after_current_cb (GtkWidget *widget, void *data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		sakura.new_tab_after_current=TRUE;
		sakura_set_config_boolean("new_tab_after_current", TRUE);
	} else {
		sakura.new_tab_after_current=FALSE;
		sakura_set_config_boolean("new_tab_after_current", FALSE);
	}
}


static void
sakura_show_scrollbar_cb (GtkWidget *widget, void *data)
{
	sakura_show_scrollbar();
}


static void
sakura_urgent_bell_cb (GtkWidget *widget, void *data)
{
	sakura.urgent_bell = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	if (sakura.urgent_bell) {
		sakura_set_config_string("urgent_bell", "Yes");
	} else {
		sakura_set_config_string("urgent_bell", "No");
	}
}


static void
sakura_audible_bell_cb (GtkWidget *widget, void *data)
{
	gint page;
	struct sakura_tab *sk_tab;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		vte_terminal_set_audible_bell (VTE_TERMINAL(sk_tab->vte), TRUE);
		sakura_set_config_string("audible_bell", "Yes");
	} else {
		vte_terminal_set_audible_bell (VTE_TERMINAL(sk_tab->vte), FALSE);
		sakura_set_config_string("audible_bell", "No");
	}
}


static void
sakura_blinking_cursor_cb (GtkWidget *widget, void *data)
{
	gint page;
	struct sakura_tab *sk_tab;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		vte_terminal_set_cursor_blink_mode (VTE_TERMINAL(sk_tab->vte), VTE_CURSOR_BLINK_ON);
		sakura_set_config_string("blinking_cursor", "Yes");
	} else {
		vte_terminal_set_cursor_blink_mode (VTE_TERMINAL(sk_tab->vte), VTE_CURSOR_BLINK_OFF);
		sakura_set_config_string("blinking_cursor", "No");
	}
}



static void
sakura_set_cursor_cb (GtkWidget *widget, void *data)
{
	struct sakura_tab *sk_tab;
	int n_pages, i;

	char *cursor_string = (char *)data;
	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {

		if (strcmp(cursor_string, "block")==0) {
			sakura.cursor_type=VTE_CURSOR_SHAPE_BLOCK;
		} else if (strcmp(cursor_string, "underline")==0) {
			sakura.cursor_type=VTE_CURSOR_SHAPE_UNDERLINE;
		} else if (strcmp(cursor_string, "ibeam")==0) {
			sakura.cursor_type=VTE_CURSOR_SHAPE_IBEAM;
		}

		for (i = (n_pages - 1); i >= 0; i--) {
			sk_tab = sakura_get_sktab(sakura, i);
			vte_terminal_set_cursor_shape(VTE_TERMINAL(sk_tab->vte), sakura.cursor_type);
		}

		sakura_set_config_integer("cursor_type", sakura.cursor_type);
	}
}


static void
sakura_setname_entry_changed_cb (GtkWidget *widget, void *data)
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
sakura_copy_cb (GtkWidget *widget, void *data)
{
	sakura_copy();
}


/* Parameters are never used */
static void
sakura_paste_cb (GtkWidget *widget, void *data)
{
	sakura_paste();
}


static void
sakura_new_tab_cb (GtkWidget *widget, void *data)
{
	sakura_add_tab();
}


static void
sakura_close_tab_cb (GtkWidget *widget, void *data)
{
	gint page;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));

	sakura_close_tab(page);
}



static void
sakura_fullscreen_cb (GtkWidget *widget, void *data)
{
	if (sakura.fullscreen != TRUE) {
		sakura.fullscreen = TRUE;
		gtk_window_fullscreen(GTK_WINDOW(sakura.main_window));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(sakura.main_window));
		sakura.fullscreen = FALSE;
	}
}


static void
sakura_disable_numbered_tabswitch_cb (GtkWidget *widget, void *data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		sakura.disable_numbered_tabswitch = true;
		sakura_set_config_boolean("disable_numbered_tabswitch", TRUE);
	} else {
		sakura.disable_numbered_tabswitch = false;
		sakura_set_config_boolean("disable_numbered_tabswitch", FALSE);
	}
}


#if 0
static void
sakura_use_fading_cb (GtkWidget *widget, void *data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		sakura.use_fading = true;
		sakura_set_config_boolean("use_fading", TRUE);
	} else {
		sakura.use_fading = false;
		sakura_set_config_boolean("use_fading", FALSE);
	}
}
#endif


/**************************/
/******* Functions ********/
/**************************/

static void
sakura_init()
{
	GError *gerror=NULL;
	char* configdir = NULL;
	int i;

	term_data_id = g_quark_from_static_string("sakura_term");

	/*** Config file initialization ***/

	sakura.cfg = g_key_file_new();
	sakura.config_modified=false;

	configdir = g_build_filename( g_get_user_config_dir(), "sakura", NULL );
	if ( ! g_file_test( g_get_user_config_dir(), G_FILE_TEST_EXISTS) )
		g_mkdir( g_get_user_config_dir(), 0755 );
	if ( ! g_file_test( configdir, G_FILE_TEST_EXISTS) )
		g_mkdir( configdir, 0755 );
	if (option_config_file) {
		sakura.configfile = g_build_filename(configdir, option_config_file, NULL);
	} else {
		/* Use more standard-conforming path for config files, if available. */
		sakura.configfile = g_build_filename(configdir, DEFAULT_CONFIGFILE, NULL);
	}
	g_free(configdir);

	/* Open config file */
	if (!g_key_file_load_from_file(sakura.cfg, sakura.configfile, 0, &gerror)) {
		/* If there's no file, ignore the error. A new one is created */
		if (gerror->code==G_KEY_FILE_ERROR_UNKNOWN_ENCODING || gerror->code==G_KEY_FILE_ERROR_INVALID_VALUE) {
			g_error_free(gerror);
			fprintf(stderr, "Not valid config file format\n");
			exit(EXIT_FAILURE);
		}
	}

	/* Add GFile monitor to control file external changes */
	GFile *cfgfile = g_file_new_for_path(sakura.configfile);
	GFileMonitor *mon_cfgfile = g_file_monitor_file (cfgfile, 0, NULL, NULL);
	g_signal_connect(G_OBJECT(mon_cfgfile), "changed", G_CALLBACK(sakura_conf_changed_cb), NULL);

	gchar *cfgtmp = NULL;

	/* We can safely ignore errors from g_key_file_get_value(), since if the
	 * call to g_key_file_has_key() was successful, the key IS there. From the
	 * glib docs I don't know if we can ignore errors from g_key_file_has_key,
	 * too. I think we can: the only possible error is that the config file
	 * doesn't exist, but we have just read it!
	 */

	for (i=0; i<NUM_COLORSETS; i++) {
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

		sprintf(temp_name, "colorset%d_scheme", i+1);
		if (!g_key_file_has_key(sakura.cfg, cfg_group, temp_name, NULL)) {
			sakura_set_config_integer(temp_name, DEFAULT_SCHEME);
		}
		sakura.schemes[i] = g_key_file_get_integer(sakura.cfg, cfg_group, temp_name, NULL);

		sprintf(temp_name, "colorset%d_key", i+1);
		if (!g_key_file_has_key(sakura.cfg, cfg_group, temp_name, NULL)) {
			sakura_set_keybind(temp_name, cs_keys[i]);
		}
		sakura.set_colorset_keys[i]= sakura_get_keybind(temp_name);
	}

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "last_colorset", NULL)) {
		sakura_set_config_integer("last_colorset", 1);
	}
	sakura.last_colorset = g_key_file_get_integer(sakura.cfg, cfg_group, "last_colorset", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "bold_is_bright", NULL)) {
		sakura_set_config_boolean("bold_is_bright", FALSE);
	}
	sakura.bold_is_bright = g_key_file_get_boolean(sakura.cfg, cfg_group, "bold_is_bright", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "scroll_lines", NULL)) {
		g_key_file_set_integer(sakura.cfg, cfg_group, "scroll_lines", DEFAULT_SCROLL_LINES);
	}
	sakura.scroll_lines = g_key_file_get_integer(sakura.cfg, cfg_group, "scroll_lines", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "font", NULL)) {
		sakura_set_config_string("font", DEFAULT_FONT);
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "font", NULL);
	sakura.font = pango_font_description_from_string(cfgtmp);
	free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "show_tab_bar", NULL)) {
		/* legacy option "show_always_first_tab" now sets "show_tab_bar = always | multiple" */
		if (g_key_file_has_key(sakura.cfg, cfg_group, "show_always_first_tab", NULL)) {
			cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "show_always_first_tab", NULL);
			sakura_set_config_string("show_tab_bar", (strcmp(cfgtmp, "Yes")==0) ? "always" : "multiple");
			free(cfgtmp);
		} else {
			sakura_set_config_string("show_tab_bar", "multiple");
		}
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "show_tab_bar", NULL);
	if (strcmp(cfgtmp, "always")==0) {
		sakura.show_tab_bar = SHOW_TAB_BAR_ALWAYS;
	} else if (strcmp(cfgtmp, "multiple")==0) {
		sakura.show_tab_bar = SHOW_TAB_BAR_MULTIPLE;
	} else if (strcmp(cfgtmp, "never")==0) {
		sakura.show_tab_bar = SHOW_TAB_BAR_NEVER;
	} else {
		fprintf(stderr, "Invalid configuration value: show_tab_bar=%s (valid values: always|multiple|never)\n", cfgtmp);
		sakura.show_tab_bar = SHOW_TAB_BAR_MULTIPLE;
	}
	free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "scrollbar", NULL)) {
		sakura_set_config_boolean("scrollbar", FALSE);
	}
	sakura.show_scrollbar = g_key_file_get_boolean(sakura.cfg, cfg_group, "scrollbar", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "closebutton", NULL)) {
		sakura_set_config_boolean("closebutton", TRUE);
	}
	sakura.show_closebutton = g_key_file_get_boolean(sakura.cfg, cfg_group, "closebutton", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "new_tab_after_current", NULL)) {
		sakura_set_config_boolean("new_tab_after_current", TRUE);
	}
	sakura.new_tab_after_current = g_key_file_get_boolean(sakura.cfg, cfg_group, "new_tab_after_current", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "tabs_on_bottom", NULL)) {
		sakura_set_config_boolean("tabs_on_bottom", FALSE);
	}
	sakura.tabs_on_bottom = g_key_file_get_boolean(sakura.cfg, cfg_group, "tabs_on_bottom", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "less_questions", NULL)) {
		sakura_set_config_boolean("less_questions", FALSE);
	}
	sakura.less_questions = g_key_file_get_boolean(sakura.cfg, cfg_group, "less_questions", NULL);

        if (!g_key_file_has_key(sakura.cfg, cfg_group, "copy_on_select", NULL)) {
                sakura_set_config_boolean("copy_on_select", FALSE);
        }
        sakura.copy_on_select = g_key_file_get_boolean(sakura.cfg, cfg_group, "copy_on_select", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "disable_numbered_tabswitch", NULL)) {
		sakura_set_config_boolean("disable_numbered_tabswitch", FALSE);
	}
	sakura.disable_numbered_tabswitch = g_key_file_get_boolean(sakura.cfg, cfg_group, "disable_numbered_tabswitch", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "use_fading", NULL)) {
		sakura_set_config_boolean("use_fading", FALSE);
	}
	sakura.use_fading = g_key_file_get_boolean(sakura.cfg, cfg_group, "use_fading", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "scrollable_tabs", NULL)) {
		sakura_set_config_boolean("scrollable_tabs", TRUE);
	}
	sakura.scrollable_tabs = g_key_file_get_boolean(sakura.cfg, cfg_group, "scrollable_tabs", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "urgent_bell", NULL)) {
		sakura_set_config_string("urgent_bell", "Yes");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "urgent_bell", NULL);
	sakura.urgent_bell= (strcmp(cfgtmp, "Yes")==0) ? 1 : 0;
	g_free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "audible_bell", NULL)) {
		sakura_set_config_string("audible_bell", "Yes");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "audible_bell", NULL);
	sakura.audible_bell= (strcmp(cfgtmp, "Yes")==0) ? 1 : 0;
	g_free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "blinking_cursor", NULL)) {
		sakura_set_config_string("blinking_cursor", "No");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "blinking_cursor", NULL);
	sakura.blinking_cursor= (strcmp(cfgtmp, "Yes")==0) ? 1 : 0;
	g_free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "cursor_type", NULL)) {
		sakura_set_config_string("cursor_type", "VTE_CURSOR_SHAPE_BLOCK");
	}
	sakura.cursor_type = g_key_file_get_integer(sakura.cfg, cfg_group, "cursor_type", NULL);

	/* Only in config file */
	if (!g_key_file_has_key(sakura.cfg, cfg_group, "word_chars", NULL)) {
		sakura_set_config_string("word_chars", DEFAULT_WORD_CHARS);
	}
	sakura.word_chars = g_key_file_get_value(sakura.cfg, cfg_group, "word_chars", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "palette", NULL)) {
		sakura_set_config_integer("palette", DEFAULT_PALETTE);
	}
	gerror=NULL;
	sakura.palette_idx = g_key_file_get_integer(sakura.cfg, cfg_group, "palette", &gerror);
	/* Backwards compatibility after changing (v.3.7.1) "palette" type from string to int. Remove after some versions */
	if (gerror && gerror->code == G_KEY_FILE_ERROR_INVALID_VALUE) {
		sakura.palette_idx = DEFAULT_PALETTE;
		sakura_set_config_integer("palette", DEFAULT_PALETTE);
		g_error_free(gerror);
	}
	sakura.palette = palettes[sakura.palette_idx];

	/* Keybindings are only in the config file */
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

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "search_accelerator", NULL)) {
		sakura_set_config_integer("search_accelerator", DEFAULT_SEARCH_ACCELERATOR);
	}
	sakura.search_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "search_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "add_tab_key", NULL)) {
		sakura_set_keybind("add_tab_key", DEFAULT_ADD_TAB_KEY);
	}
	sakura.add_tab_key = sakura_get_keybind("add_tab_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "del_tab_key", NULL)) {
		sakura_set_keybind("del_tab_key", DEFAULT_DEL_TAB_KEY);
	}
	sakura.del_tab_key = sakura_get_keybind("del_tab_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "prev_tab_key", NULL)) {
		sakura_set_keybind("prev_tab_key", DEFAULT_PREV_TAB_KEY);
	}
	sakura.prev_tab_key = sakura_get_keybind("prev_tab_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "next_tab_key", NULL)) {
		sakura_set_keybind("next_tab_key", DEFAULT_NEXT_TAB_KEY);
	}
	sakura.next_tab_key = sakura_get_keybind("next_tab_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "copy_key", NULL)) {
		sakura_set_keybind( "copy_key", DEFAULT_COPY_KEY);
	}
	sakura.copy_key = sakura_get_keybind("copy_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "paste_key", NULL)) {
		sakura_set_keybind("paste_key", DEFAULT_PASTE_KEY);
	}
	sakura.paste_key = sakura_get_keybind("paste_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "scrollbar_key", NULL)) {
		sakura_set_keybind("scrollbar_key", DEFAULT_SCROLLBAR_KEY);
	}
	sakura.scrollbar_key = sakura_get_keybind("scrollbar_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "set_tab_name_key", NULL)) {
		sakura_set_keybind("set_tab_name_key", DEFAULT_SET_TAB_NAME_KEY);
	}
	sakura.set_tab_name_key = sakura_get_keybind("set_tab_name_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "search_key", NULL)) {
		sakura_set_keybind("search_key", DEFAULT_SEARCH_KEY);
	}
	sakura.search_key = sakura_get_keybind("search_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "increase_font_size_key", NULL)) {
		sakura_set_keybind("increase_font_size_key", DEFAULT_INCREASE_FONT_SIZE_KEY);
	}
	sakura.increase_font_size_key = sakura_get_keybind("increase_font_size_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "decrease_font_size_key", NULL)) {
		sakura_set_keybind("decrease_font_size_key", DEFAULT_DECREASE_FONT_SIZE_KEY);
	}
	sakura.decrease_font_size_key = sakura_get_keybind("decrease_font_size_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "fullscreen_key", NULL)) {
		sakura_set_keybind("fullscreen_key", DEFAULT_FULLSCREEN_KEY);
	}
	sakura.fullscreen_key = sakura_get_keybind("fullscreen_key");

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "set_colorset_accelerator", NULL)) {
		sakura_set_config_integer("set_colorset_accelerator", DEFAULT_SELECT_COLORSET_ACCELERATOR);
	}
	sakura.set_colorset_accelerator = g_key_file_get_integer(sakura.cfg, cfg_group, "set_colorset_accelerator", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "icon_file", NULL)) {
		sakura_set_config_string("icon_file", ICON_FILE);
	}
	sakura.icon = g_key_file_get_string(sakura.cfg, cfg_group, "icon_file", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "paste_button", NULL)) {
		sakura_set_config_integer("paste_button", DEFAULT_PASTE_BUTTON);
	}
	sakura.paste_button = g_key_file_get_integer(sakura.cfg, cfg_group, "paste_button", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "menu_button", NULL)) {
		sakura_set_config_integer("menu_button", DEFAULT_MENU_BUTTON);
	}
	sakura.menu_button = g_key_file_get_integer(sakura.cfg, cfg_group, "menu_button", NULL);

	/* NULL if not found. Don't add a new one */ /* Only in config file */
	sakura.tab_default_title = g_key_file_get_string(sakura.cfg, cfg_group, "tab_default_title", NULL);

	sakura.dont_save = g_key_file_get_boolean(sakura.cfg, cfg_group, "dont_save", NULL);

	/* Default terminal size */
	if (!g_key_file_has_key(sakura.cfg, cfg_group, "window_columns", NULL)) {
		sakura_set_config_integer("window_columns", DEFAULT_COLUMNS);
	}
	sakura.columns = g_key_file_get_integer(sakura.cfg, cfg_group, "window_columns", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "window_rows", NULL)) {
		sakura_set_config_integer("window_rows", DEFAULT_ROWS);
	}
	sakura.rows = g_key_file_get_integer(sakura.cfg, cfg_group, "window_rows", NULL);

	/* Optional only, no need to set it if not found */
	sakura.shell_path = g_key_file_get_string(sakura.cfg, cfg_group, "shell_path", NULL);
	
	/* Default terminal. Only in config file */
	sakura.term = g_key_file_get_value(sakura.cfg, cfg_group, "term", NULL);

	/*** Sakura window initialization ***/

	/* Use always GTK header bar*/
	g_object_set(gtk_settings_get_default(), "gtk-dialogs-use-header", TRUE, NULL);

	/* Create our windows */
	sakura.main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(sakura.main_window), "sakura");

	sakura.fade_window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_set_name(sakura.fade_window, "fade_window");
	gtk_window_set_position(GTK_WINDOW(sakura.fade_window), GTK_WIN_POS_NONE);
	gtk_widget_set_opacity(sakura.fade_window, FADE_WINDOW_OPACITY);
	gtk_window_set_transient_for(GTK_WINDOW(sakura.fade_window), GTK_WINDOW(sakura.main_window));

	/* Add CSS styles for main and fade window*/
	GtkCssProvider *provider = gtk_css_provider_new();
	GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(sakura.main_window));
	gtk_css_provider_load_from_data(provider, SAKURA_CSS, -1, NULL);
	gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(provider);

	provider = gtk_css_provider_new();
	screen = gtk_widget_get_screen(GTK_WIDGET(sakura.fade_window));
	gtk_css_provider_load_from_data(provider, FADE_WINDOW_CSS, -1, NULL);
	gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(provider);

	/* Create notebook and set style */
	sakura.notebook = gtk_notebook_new();
	gtk_notebook_set_scrollable((GtkNotebook*)sakura.notebook, sakura.scrollable_tabs);

	/* Adding mask, for handle scroll events */
	gtk_widget_add_events(sakura.notebook, GDK_SCROLL_MASK);
	
	/* Figure out if we have rgba capabilities. Without this transparency won't work as expected */
	screen = gtk_widget_get_screen (GTK_WIDGET (sakura.main_window));
	GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
	if (visual != NULL && gdk_screen_is_composited (screen)) {
		gtk_widget_set_visual (GTK_WIDGET (sakura.main_window), visual);
	}
	
	/*** Command line options initialization ***/

	/* Set argv for forked childs. Real argv vector starts at argv[1] because we're
	   using G_SPAWN_FILE_AND_ARGV_ZERO to be able to launch login shells */
	/* If the shell_path has been set in the config file it takes priority over the envvar */
	if (sakura.shell_path != NULL) {
		sakura.argv[0] = g_strdup(sakura.shell_path);
		sakura.argv[1] = g_strdup(sakura.shell_path);
	} else {
		sakura.argv[0] = g_strdup(g_getenv("SHELL"));
		if (option_login) {
			sakura.argv[1] = g_strdup_printf("-%s", g_getenv("SHELL"));
		} else {
			sakura.argv[1] = g_strdup(g_getenv("SHELL"));
		}
	}
	sakura.argv[2]=NULL;

	/* Add datadir path to icon name and set icon */
	gchar *icon_path; gerror=NULL;
	if (option_icon) {
		icon_path = g_strdup_printf("%s", option_icon);
	} else {
		icon_path = g_strdup_printf(DATADIR "/pixmaps/%s", sakura.icon);
	}
	gtk_window_set_icon_from_file(GTK_WINDOW(sakura.main_window), icon_path, &gerror);
	g_free(icon_path); icon_path=NULL;
	if (gerror) g_error_free(gerror);

	/* More options */
	if (option_title) {
		sakura.main_title = g_strdup_printf("%s", option_title);
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), sakura.main_title);
	} else {
		sakura.main_title = NULL;
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

	if (option_colorset && option_colorset>0 && option_colorset <= NUM_COLORSETS) {
		sakura.last_colorset = option_colorset;
	}

	/* These options are exclusive */
	if (option_fullscreen) {
		sakura_fullscreen_cb(NULL, NULL);
	} else if (option_maximize) {
		gtk_window_maximize(GTK_WINDOW(sakura.main_window));
	}

	sakura.fullscreen = FALSE;
	sakura.resized = FALSE;
	sakura.externally_modified = false;
	sakura.first_run=true;

	gerror = NULL;
	sakura.http_vteregexp = vte_regex_new_for_match(HTTP_REGEXP, strlen(HTTP_REGEXP), PCRE2_MULTILINE, &gerror);
	if (!sakura.http_vteregexp) {
		SAY("http_regexp: %s", gerror->message);
		g_error_free(gerror);
	}
	gerror=NULL;
	sakura.mail_vteregexp = vte_regex_new_for_match(MAIL_REGEXP, strlen(MAIL_REGEXP), PCRE2_MULTILINE, &gerror);
	if (!sakura.mail_vteregexp) {
		SAY("mail_regexp: %s", gerror->message);
		g_error_free(gerror);
	}

	gtk_container_add(GTK_CONTAINER(sakura.main_window), sakura.notebook);

	sakura_init_popup();

	g_signal_connect(G_OBJECT(sakura.main_window), "delete_event", G_CALLBACK(sakura_delete_event_cb), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "destroy", G_CALLBACK(sakura_destroy_window_cb), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "key-press-event", G_CALLBACK(sakura_key_press_cb), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "configure-event", G_CALLBACK(sakura_resized_window_cb), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "focus-out-event", G_CALLBACK(sakura_focus_out_cb), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "focus-in-event", G_CALLBACK(sakura_focus_in_cb), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "show", G_CALLBACK(sakura_show_event_cb), NULL);
}


static void
sakura_init_popup()
{
	GtkWidget *item_new_tab, *item_set_name, *item_close_tab, *item_copy,
	          *item_paste, *item_fullscreen, *item_select_font, *item_select_colors,
	          *item_show_tab_bar,
	          *item_show_tab_bar_always, *item_show_tab_bar_multiple, *item_show_tab_bar_never,
	          *item_toggle_scrollbar, *item_options,
	          *item_urgent_bell, *item_audible_bell, *item_blinking_cursor,
	          *item_cursor, *item_cursor_block, *item_cursor_underline, *item_cursor_ibeam,
		  *item_tabs_on_bottom, *item_less_questions, *item_copy_on_select,
	          *item_disable_numbered_tabswitch, *item_new_tab_after_current; // *item_use_fading;
	GtkWidget *options_menu, *show_tab_bar_menu, *cursor_menu;

	sakura.item_open_mail = gtk_menu_item_new_with_label(_("Open mail"));
	sakura.item_open_link = gtk_menu_item_new_with_label(_("Open link"));
	sakura.item_copy_link = gtk_menu_item_new_with_label(_("Copy link"));
	item_new_tab = gtk_menu_item_new_with_label(_("New tab"));
	item_set_name = gtk_menu_item_new_with_label(_("Set tab name..."));
	item_close_tab = gtk_menu_item_new_with_label(_("Close tab"));
	item_fullscreen = gtk_menu_item_new_with_label(_("Full screen"));
	item_copy = gtk_menu_item_new_with_label(_("Copy"));
	item_paste = gtk_menu_item_new_with_label(_("Paste"));

	item_options = gtk_menu_item_new_with_label(_("Options"));

	item_select_font = gtk_menu_item_new_with_label(_("Select font..."));
	item_select_colors = gtk_menu_item_new_with_label(_("Select colors..."));
	item_show_tab_bar = gtk_menu_item_new_with_label(_("Show tab bar"));
	item_show_tab_bar_always = gtk_radio_menu_item_new_with_label(NULL, _("Always"));
	item_show_tab_bar_multiple = gtk_radio_menu_item_new_with_label_from_widget(
		GTK_RADIO_MENU_ITEM(item_show_tab_bar_always), _("When there's more than one tab"));
	item_show_tab_bar_never = gtk_radio_menu_item_new_with_label_from_widget(
		GTK_RADIO_MENU_ITEM(item_show_tab_bar_always), _("Never"));
	item_tabs_on_bottom = gtk_check_menu_item_new_with_label(_("Tabs at bottom"));
	item_new_tab_after_current = gtk_check_menu_item_new_with_label(_("New tab after current tab"));
	item_toggle_scrollbar = gtk_check_menu_item_new_with_label(_("Show scrollbar"));
	item_less_questions = gtk_check_menu_item_new_with_label(_("Fewer questions at exit time"));
        item_copy_on_select = gtk_check_menu_item_new_with_label(_("Automatically copy selected text"));
	item_urgent_bell = gtk_check_menu_item_new_with_label(_("Set urgent bell"));
	item_audible_bell = gtk_check_menu_item_new_with_label(_("Set audible bell"));
	item_blinking_cursor = gtk_check_menu_item_new_with_label(_("Set blinking cursor"));
	item_disable_numbered_tabswitch = gtk_check_menu_item_new_with_label(_("Disable numbered tabswitch"));
	//item_use_fading = gtk_check_menu_item_new_with_label(_("Enable focus fade"));
	item_cursor = gtk_menu_item_new_with_label(_("Set cursor type"));
	item_cursor_block = gtk_radio_menu_item_new_with_label(NULL, _("Block"));
	item_cursor_underline = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_cursor_block), _("Underline"));
	item_cursor_ibeam = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_cursor_block), _("IBeam"));

	/* Show defaults in menu items */
	switch (sakura.show_tab_bar) {
		case SHOW_TAB_BAR_ALWAYS:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_tab_bar_always), TRUE);
			break;
		case SHOW_TAB_BAR_MULTIPLE:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_tab_bar_multiple), TRUE);
			break;
		case SHOW_TAB_BAR_NEVER:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_tab_bar_never), TRUE);
	}

	if (sakura.new_tab_after_current) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_new_tab_after_current), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_new_tab_after_current), FALSE);
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

        if (sakura.copy_on_select) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_copy_on_select), TRUE);
        }

	if (sakura.show_scrollbar) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_scrollbar), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_scrollbar), FALSE);
	}

	if (sakura.disable_numbered_tabswitch) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_disable_numbered_tabswitch), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_disable_numbered_tabswitch), FALSE);
	}

	//if (sakura.use_fading) {
	//	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_use_fading), TRUE);
	//} else {
	//	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_use_fading), FALSE);
	//}

	if (sakura.urgent_bell) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_urgent_bell), TRUE);
	}

	if (sakura.audible_bell) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_audible_bell), TRUE);
	}

	if (sakura.blinking_cursor) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_blinking_cursor), TRUE);
	}

	switch (sakura.cursor_type) {
		case VTE_CURSOR_SHAPE_BLOCK:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_cursor_block), TRUE);
			break;
		case VTE_CURSOR_SHAPE_UNDERLINE:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_cursor_underline), TRUE);
			break;
		case VTE_CURSOR_SHAPE_IBEAM:
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_cursor_ibeam), TRUE);
	}

	sakura.open_link_separator = gtk_separator_menu_item_new();

	sakura.menu = gtk_menu_new();

	/* Add items to popup menu */
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.item_open_mail);
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
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_options);

	options_menu = gtk_menu_new();
	show_tab_bar_menu = gtk_menu_new();
	cursor_menu = gtk_menu_new();

	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_select_colors);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_select_font);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_show_tab_bar);
	gtk_menu_shell_append(GTK_MENU_SHELL(show_tab_bar_menu), item_show_tab_bar_always);
	gtk_menu_shell_append(GTK_MENU_SHELL(show_tab_bar_menu), item_show_tab_bar_multiple);
	gtk_menu_shell_append(GTK_MENU_SHELL(show_tab_bar_menu), item_show_tab_bar_never);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_tabs_on_bottom);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_new_tab_after_current);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_toggle_scrollbar);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_less_questions);
        gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_copy_on_select);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_urgent_bell);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_audible_bell);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_disable_numbered_tabswitch);
	//gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_use_fading);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_blinking_cursor);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_cursor);
	gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item_cursor_block);
	gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item_cursor_underline);
	gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item_cursor_ibeam);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_options), options_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_show_tab_bar), show_tab_bar_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_cursor), cursor_menu);

	/* ... and finally assign callbacks to menuitems */
	g_signal_connect(G_OBJECT(item_new_tab), "activate", G_CALLBACK(sakura_new_tab_cb), NULL);
	g_signal_connect(G_OBJECT(item_set_name), "activate", G_CALLBACK(sakura_set_name_dialog_cb), NULL);
	g_signal_connect(G_OBJECT(item_close_tab), "activate", G_CALLBACK(sakura_close_tab_cb), NULL);
	g_signal_connect(G_OBJECT(item_select_font), "activate", G_CALLBACK(sakura_font_dialog_cb), NULL);
	g_signal_connect(G_OBJECT(item_copy), "activate", G_CALLBACK(sakura_copy_cb), NULL);
	g_signal_connect(G_OBJECT(item_paste), "activate", G_CALLBACK(sakura_paste_cb), NULL);
	g_signal_connect(G_OBJECT(item_select_colors), "activate", G_CALLBACK(sakura_color_dialog_cb), NULL);

	g_signal_connect(G_OBJECT(item_show_tab_bar_always), "activate", G_CALLBACK(sakura_show_tab_bar_cb), "always");
	g_signal_connect(G_OBJECT(item_show_tab_bar_multiple), "activate", G_CALLBACK(sakura_show_tab_bar_cb), "multiple");
	g_signal_connect(G_OBJECT(item_show_tab_bar_never), "activate", G_CALLBACK(sakura_show_tab_bar_cb), "never");
	g_signal_connect(G_OBJECT(item_tabs_on_bottom), "activate", G_CALLBACK(sakura_tabs_on_bottom_cb), NULL);
	g_signal_connect(G_OBJECT(item_less_questions), "activate", G_CALLBACK(sakura_less_questions_cb), NULL);
        g_signal_connect(G_OBJECT(item_copy_on_select), "activate", G_CALLBACK(sakura_copy_on_select_cb), NULL);
        g_signal_connect(G_OBJECT(item_new_tab_after_current), "activate", G_CALLBACK(sakura_new_tab_after_current_cb), NULL);
	g_signal_connect(G_OBJECT(item_toggle_scrollbar), "activate", G_CALLBACK(sakura_show_scrollbar_cb), NULL);
	g_signal_connect(G_OBJECT(item_urgent_bell), "activate", G_CALLBACK(sakura_urgent_bell_cb), NULL);
	g_signal_connect(G_OBJECT(item_audible_bell), "activate", G_CALLBACK(sakura_audible_bell_cb), NULL);
	g_signal_connect(G_OBJECT(item_blinking_cursor), "activate", G_CALLBACK(sakura_blinking_cursor_cb), NULL);
	g_signal_connect(G_OBJECT(item_disable_numbered_tabswitch), "activate", G_CALLBACK(sakura_disable_numbered_tabswitch_cb), NULL);
	//g_signal_connect(G_OBJECT(item_use_fading), "activate", G_CALLBACK(sakura_use_fading_cb), NULL);
	g_signal_connect(G_OBJECT(item_cursor_block), "activate", G_CALLBACK(sakura_set_cursor_cb), "block");
	g_signal_connect(G_OBJECT(item_cursor_underline), "activate", G_CALLBACK(sakura_set_cursor_cb), "underline");
	g_signal_connect(G_OBJECT(item_cursor_ibeam), "activate", G_CALLBACK(sakura_set_cursor_cb), "ibeam");

	g_signal_connect(G_OBJECT(sakura.item_open_mail), "activate", G_CALLBACK(sakura_open_mail_cb), NULL);
	g_signal_connect(G_OBJECT(sakura.item_open_link), "activate", G_CALLBACK(sakura_open_url_cb), NULL);
	g_signal_connect(G_OBJECT(sakura.item_copy_link), "activate", G_CALLBACK(sakura_copy_url_cb), NULL);
	g_signal_connect(G_OBJECT(item_fullscreen), "activate", G_CALLBACK(sakura_fullscreen_cb), NULL);

	gtk_widget_show_all(sakura.menu);
}


static void
sakura_destroy()
{
	/* Delete all existing tabs */
	while (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) >= 1) {
		sakura_del_tab(-1);
	}

	g_key_file_free(sakura.cfg);

	pango_font_description_free(sakura.font);

	free(sakura.configfile);

	gtk_main_quit();
}


static void
sakura_search_dialog ()
{
	GtkWidget *title_dialog, *title_header;
	GtkWidget *entry, *label;
	GtkWidget *title_hbox;
	gint response;

	title_dialog=gtk_dialog_new_with_buttons(_("Search"),
	                                         GTK_WINDOW(sakura.main_window),
	                                         GTK_DIALOG_MODAL|GTK_DIALOG_USE_HEADER_BAR,
	                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                         _("_Apply"), GTK_RESPONSE_ACCEPT,
	                                          NULL);

	/* Configure the new gtk header bar*/
	title_header = gtk_dialog_get_header_bar(GTK_DIALOG(title_dialog));
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(title_header), FALSE);
	gtk_dialog_set_default_response(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT);

	entry = gtk_entry_new();
	label = gtk_label_new(_("Search"));
	title_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(title_hbox), label, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(title_hbox), entry, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(title_dialog))), title_hbox, FALSE, FALSE, 12);

	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(sakura_setname_entry_changed_cb), title_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT, FALSE);

	gtk_widget_show_all(title_hbox);

	response = gtk_dialog_run(GTK_DIALOG(title_dialog));
	if (response == GTK_RESPONSE_ACCEPT) {
		sakura_search(gtk_entry_get_text(GTK_ENTRY(entry)), 0);
	}
	gtk_widget_destroy(title_dialog);
}


void
sakura_search (const char *pattern, bool reverse)
{
	GError *error=NULL;
	VteRegex *regex;
	gint page;
	struct sakura_tab *sk_tab;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	vte_terminal_search_set_wrap_around(VTE_TERMINAL(sk_tab->vte), TRUE);

	regex=vte_regex_new_for_search(pattern, (gssize) strlen(pattern), PCRE2_MULTILINE|PCRE2_CASELESS, &error);
	if (!regex) { /* Ubuntu-fucking-morons (17.10/18.04/18.10) package a broken VTE without PCRE2, and search fails */
		      /* For more info about their moronity please look at https://github.com/gnunn1/tilix/issues/916   */
		sakura_error(error->message);
		g_error_free(error);
	} else {
		vte_terminal_search_set_regex(VTE_TERMINAL(sk_tab->vte), regex, 0);

		if (!vte_terminal_search_find_next(VTE_TERMINAL(sk_tab->vte))) {
			vte_terminal_unselect_all(VTE_TERMINAL(sk_tab->vte));
			vte_terminal_search_find_next(VTE_TERMINAL(sk_tab->vte));
		}

		if (regex) vte_regex_unref(regex);
	}
}


static void
sakura_copy ()
{
	gint page;
	struct sakura_tab *sk_tab;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	if (vte_terminal_get_has_selection(VTE_TERMINAL(sk_tab->vte))) {
		vte_terminal_copy_clipboard_format(VTE_TERMINAL(sk_tab->vte), VTE_FORMAT_TEXT);
	}
}


static void
sakura_paste ()
{
	gint page;
	struct sakura_tab *sk_tab;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	vte_terminal_paste_clipboard(VTE_TERMINAL(sk_tab->vte));
}


static void
sakura_paste_primary ()
{
	gint page;
	struct sakura_tab *sk_tab;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	vte_terminal_paste_primary(VTE_TERMINAL(sk_tab->vte));
}


static void
sakura_show_scrollbar (void)
{
	gint page, n_pages;
	struct sakura_tab *sk_tab;
	int i;

	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	if (!g_key_file_get_boolean(sakura.cfg, cfg_group, "scrollbar", NULL)) {
		sakura.show_scrollbar = true;
		sakura_set_config_boolean("scrollbar", TRUE);
	} else {
		sakura.show_scrollbar = false;
		sakura_set_config_boolean("scrollbar", FALSE);
	}

	/* Toggle/Untoggle the scrollbar for all tabs */
	for (i = (n_pages - 1); i >= 0; i--) {
		sk_tab = sakura_get_sktab(sakura, i);
		if (!sakura.show_scrollbar)
			gtk_widget_hide(sk_tab->scrollbar);
		else
			gtk_widget_show(sk_tab->scrollbar);
	}
	sakura_set_size();
}


static void
sakura_set_size (void)
{
	struct sakura_tab *sk_tab;
	gint pad_x, pad_y;
	gint char_width, char_height;
	guint npages;
	gint min_width, natural_width;
	gint page;


	sk_tab = sakura_get_sktab(sakura, 0);
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Mayhaps an user resize happened. Check if row and columns have changed */
	if (sakura.resized) {
		sakura.columns = vte_terminal_get_column_count(VTE_TERMINAL(sk_tab->vte));
		sakura.rows = vte_terminal_get_row_count(VTE_TERMINAL(sk_tab->vte));
		SAY("New columns %ld and rows %ld", sakura.columns, sakura.rows);
		sakura.resized = FALSE;
	}

	gtk_style_context_get_padding(gtk_widget_get_style_context(sk_tab->vte),
		gtk_widget_get_state_flags(sk_tab->vte),
		&sk_tab->padding);
	pad_x = sk_tab->padding.left + sk_tab->padding.right;
	pad_y = sk_tab->padding.top + sk_tab->padding.bottom;
	//SAY("padding x %d y %d", pad_x, pad_y);
	char_width = vte_terminal_get_char_width(VTE_TERMINAL(sk_tab->vte));
	char_height = vte_terminal_get_char_height(VTE_TERMINAL(sk_tab->vte));

	sakura.width = pad_x + (char_width * sakura.columns);
	sakura.height = pad_y + (char_height * sakura.rows);

	if (sakura.show_tab_bar == SHOW_TAB_BAR_ALWAYS || (sakura.show_tab_bar == SHOW_TAB_BAR_MULTIPLE && npages > 1)) {

		/* TODO: Yeah i know, this is utter shit. Remove this ugly hack and set geometry hints*/
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
	sk_tab = sakura_get_sktab(sakura, page);

	gtk_widget_get_preferred_width(sk_tab->scrollbar, &min_width, &natural_width);
	//SAY("SCROLLBAR min width %d natural width %d", min_width, natural_width);
	if (sakura.show_scrollbar) {
		sakura.width += min_width;
	}

	/* GTK does not ignore resize for maximized windows on some systems,
	so we do need check if it's maximized or not */
	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(sakura.main_window));
	if (gdk_window != NULL) {
		if (gdk_window_get_state(gdk_window) & GDK_WINDOW_STATE_MAXIMIZED) {
			SAY("window is maximized, will not resize");
			return;
		}
	}

	gtk_window_resize(GTK_WINDOW(sakura.main_window), sakura.width, sakura.height);
	SAY("Resized to %d %d", sakura.width, sakura.height);
}


static void
sakura_set_font()
{
	gint n_pages;
	struct sakura_tab *sk_tab;
	int i;

	n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Set the font for all tabs */
	for (i = (n_pages - 1); i >= 0; i--) {
		sk_tab = sakura_get_sktab(sakura, i);
		vte_terminal_set_font(VTE_TERMINAL(sk_tab->vte), sakura.font);
	}
}

/* Set colorset when colosert keybinding is used */
static void
sakura_set_colorset (int cs)
{
	gint page;
	struct sakura_tab *sk_tab;

	if (cs < 0 || cs >= NUM_COLORSETS)
		return;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);
	sk_tab->colorset = cs;

	sakura_set_config_integer("last_colorset", sk_tab->colorset+1);

	sakura_set_colors();
}


/* Set the terminal colors for all notebook tabs */
static void
sakura_set_colors ()
{
	int i;
	int n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	struct sakura_tab *sk_tab;

	for (i = (n_pages - 1); i >= 0; i--) {
		sk_tab = sakura_get_sktab(sakura, i);

		/* Set fore, back, cursor color and palette for the terminal's colorset */
		vte_terminal_set_colors(VTE_TERMINAL(sk_tab->vte),
		                        &sakura.forecolors[sk_tab->colorset],
		                        &sakura.backcolors[sk_tab->colorset],
		                        sakura.palette, PALETTE_SIZE);
		vte_terminal_set_color_cursor(VTE_TERMINAL(sk_tab->vte), &sakura.curscolors[sk_tab->colorset]);

		/* Use background color to make text visible when the cursor is over it */
		vte_terminal_set_color_cursor_foreground(VTE_TERMINAL(sk_tab->vte), &sakura.backcolors[sk_tab->colorset]);

		vte_terminal_set_bold_is_bright(VTE_TERMINAL(sk_tab->vte), sakura.bold_is_bright);

	}

	/* Main window opacity must be set. Otherwise vte widget will remain opaque */
	gtk_widget_set_opacity(sakura.main_window, sakura.backcolors[sk_tab->colorset].alpha);
}


static void
sakura_move_tab(gint direction)
{
	gint page, n_pages;
	GtkWidget *child;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(sakura.notebook), page);

	if (direction == FORWARD) {
		if (page != n_pages-1)
			gtk_notebook_reorder_child(GTK_NOTEBOOK(sakura.notebook), child, page+1);
	} else {
		if (page != 0)
			gtk_notebook_reorder_child(GTK_NOTEBOOK(sakura.notebook), child, page-1);
	}
}


/* Find the notebook page for the vte terminal passed as a parameter */
static gint
sakura_find_tab(VteTerminal *vte_term)
{
	gint matched_page, page, n_pages;
	struct sakura_tab *sk_tab;

	n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	matched_page = -1;
	page = 0;

	do {
		sk_tab = sakura_get_sktab(sakura, page);
		if ((VteTerminal *)sk_tab->vte == vte_term) {
			matched_page=page;
		}
		page++;
	} while (page < n_pages);

	return (matched_page);
}


static void
sakura_set_tab_label_text(const gchar *title, gint page)
{
	struct sakura_tab *sk_tab;
	gchar *chopped_title;
	gchar *default_label_text;

	sk_tab = sakura_get_sktab(sakura, page);

	if ((title != NULL) && (g_strcmp0(title, "") != 0)) {
		/* Chop to max size */
		chopped_title = g_strndup(title, TAB_MAX_SIZE);
		/* Honor the minimum tab label size */
		while (strlen(chopped_title)< TAB_MIN_SIZE) {
			char *old_ptr = chopped_title;
			chopped_title = g_strconcat(chopped_title, " ", NULL);
			free(old_ptr);
		}
		gtk_label_set_text(GTK_LABEL(sk_tab->label), chopped_title);
		free(chopped_title);
	} else { /* Use the default values */
		default_label_text = g_strdup_printf(_("Terminal %d"), page);
		gtk_label_set_text(GTK_LABEL(sk_tab->label), default_label_text);
		free(default_label_text);
	}
}


/* Callback for vte_terminal_spawn_async */
void
sakura_spawn_callback (VteTerminal *vte, GPid pid, GError *error, gpointer user_data)
{
	struct sakura_tab *sk_tab = (struct sakura_tab *) user_data;

	if (pid == -1) { /* Fork has failed */
		SAY("Error: %s", error->message);
	} else {
		sk_tab->pid=pid;
	}
}


static void
sakura_add_tab()
{
	struct sakura_tab *sk_tab;
	GtkWidget *tab_title_hbox; GtkWidget *close_button; /* We could put them inside struct sakura_tab, but it is not necessary */
	GtkWidget *event_box;
	gint index, page, npages;
	gchar *cwd = NULL; gchar *default_label_text = NULL;

	sk_tab = g_new0(struct sakura_tab, 1);

	/* Create the tab label */
	sk_tab->label = gtk_label_new(NULL);
	gtk_label_set_ellipsize(GTK_LABEL(sk_tab->label), PANGO_ELLIPSIZE_END);

	/* Create hbox for our label & button */
	tab_title_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_hexpand(tab_title_hbox, TRUE);

	/* Label widgets has no window associated, so we need an event box to catch click events */
	event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(event_box), sk_tab->label);
	gtk_widget_set_events(event_box, GDK_BUTTON_PRESS_MASK);

	/* Expand&fill the event_box to get click events all along the tab */
	gtk_box_pack_start(GTK_BOX(tab_title_hbox), event_box, TRUE, TRUE, 0);

	/* If the tab close button is enabled, create and add it to the tab */
	if (sakura.show_closebutton) {
		close_button = gtk_button_new();
		/* Adding scroll-event to button, to propagate it to notebook (fix for scroll event when pointer is above the button) */
		gtk_widget_add_events(close_button, GDK_SCROLL_MASK);

		gtk_widget_set_name(close_button, "closebutton");
		gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);

		GtkWidget *image = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_MENU);
		gtk_container_add (GTK_CONTAINER (close_button), image);
		gtk_box_pack_start(GTK_BOX(tab_title_hbox), close_button, FALSE, FALSE, 0);
	}

	if (sakura.tabs_on_bottom) {
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(sakura.notebook), GTK_POS_BOTTOM);
	}

	gtk_widget_show_all(tab_title_hbox);

	/* Create new vte terminal, scrollbar, and pack it */
	sk_tab->vte = vte_terminal_new();
	sk_tab->scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(sk_tab->vte)));
	sk_tab->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(sk_tab->hbox), sk_tab->vte, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(sk_tab->hbox), sk_tab->scrollbar, FALSE, FALSE, 0);

	sk_tab->colorset = sakura.last_colorset-1;

	/* -1 if there is no pages yet */
	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));

	/* Use previous terminal (if there is one) cwd and colorset */
	if (page >= 0) {
		struct sakura_tab *prev_term;
		prev_term = sakura_get_sktab(sakura, page);
		cwd = sakura_get_term_cwd(prev_term); /* FIXME: Use current_uri from vte */

		sk_tab->colorset = prev_term->colorset;
	}

	if (!cwd)
		cwd = g_get_current_dir();

	if (!sakura.new_tab_after_current) {
		if ((index=gtk_notebook_append_page(GTK_NOTEBOOK(sakura.notebook), sk_tab->hbox, tab_title_hbox))==-1) {
			sakura_error("Cannot create a new tab");
			exit(1);
		}
	} else {
		if ((index=gtk_notebook_insert_page(GTK_NOTEBOOK(sakura.notebook), sk_tab->hbox, tab_title_hbox, page+1))==-1) {
			sakura_error("Cannot create a new tab");
			exit(1);
		}
	}

	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(sakura.notebook), sk_tab->hbox, TRUE);

	sakura_set_sktab(sakura, index, sk_tab );

	/* vte signals */
	g_signal_connect(G_OBJECT(sk_tab->vte), "bell", G_CALLBACK(sakura_beep_cb), NULL);
	g_signal_connect(G_OBJECT(sk_tab->vte), "increase-font-size", G_CALLBACK(sakura_increase_font_cb), NULL);
	g_signal_connect(G_OBJECT(sk_tab->vte), "decrease-font-size", G_CALLBACK(sakura_decrease_font_cb), NULL);
	sk_tab->exit_handler_id = g_signal_connect(G_OBJECT(sk_tab->vte), "child-exited", G_CALLBACK(sakura_child_exited_cb), NULL);
	g_signal_connect(G_OBJECT(sk_tab->vte), "eof", G_CALLBACK(sakura_eof_cb), NULL);
	g_signal_connect(G_OBJECT(sk_tab->vte), "window-title-changed", G_CALLBACK(sakura_title_changed_cb), NULL);
	g_signal_connect_after(G_OBJECT(sk_tab->vte), "button-press-event", G_CALLBACK(sakura_term_buttonpressed_cb), sakura.menu);
	g_signal_connect_swapped(G_OBJECT(sk_tab->vte), "button-release-event", G_CALLBACK(sakura_term_buttonreleased_cb), sakura.menu);

	/* Label & button signals */
	/* We need the hbox to know which label/button was clicked */
	g_signal_connect(G_OBJECT(event_box), "button_press_event", G_CALLBACK(sakura_label_clicked_cb), sk_tab->hbox);
	if (sakura.show_closebutton) {
		g_signal_connect(G_OBJECT(close_button), "clicked", G_CALLBACK(sakura_closebutton_clicked_cb), sk_tab->hbox);
	}

	/* Allow the user to use a different TERM value */
	char *command_env[2]; command_env[1]=NULL;
	if (sakura.term != NULL) {
		command_env[0] = g_strdup_printf ("TERM=%s", sakura.term);
	} else {
		command_env[0] = g_strdup_printf ("TERM=xterm-256color");
	}

	/******* First tab **********/
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	if (npages == 1) {
		if (sakura.show_tab_bar == SHOW_TAB_BAR_ALWAYS) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		} else {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}

		gtk_notebook_set_show_border(GTK_NOTEBOOK(sakura.notebook), FALSE);

		/* Set geometry hints when the first tab is created */
		GdkGeometry sk_hints;

		sk_hints.base_width = vte_terminal_get_char_width(VTE_TERMINAL(sk_tab->vte));
		sk_hints.base_height = vte_terminal_get_char_height(VTE_TERMINAL(sk_tab->vte));
		sk_hints.min_width = vte_terminal_get_char_width(VTE_TERMINAL(sk_tab->vte)) * DEFAULT_MIN_WIDTH_CHARS;
		sk_hints.min_height = vte_terminal_get_char_height(VTE_TERMINAL(sk_tab->vte)) * DEFAULT_MIN_HEIGHT_CHARS;
		sk_hints.width_inc = vte_terminal_get_char_width(VTE_TERMINAL(sk_tab->vte));
		sk_hints.height_inc = vte_terminal_get_char_height(VTE_TERMINAL(sk_tab->vte));

		gtk_window_set_geometry_hints(GTK_WINDOW(sakura.main_window), GTK_WIDGET (sk_tab->vte), &sk_hints,
		                              GDK_HINT_RESIZE_INC | GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);

		sakura_set_font();
		sakura_set_colors();
		/* Set size before showing the widgets but after setting the font */
		sakura_set_size();

		/* Notebook signals. Per notebook signals only need to be defined once, so we put them here */
		g_signal_connect(sakura.notebook, "scroll-event", G_CALLBACK(sakura_notebook_scroll_cb), NULL);
		g_signal_connect(G_OBJECT(sakura.notebook), "switch-page", G_CALLBACK(sakura_switch_page_cb), NULL);
		g_signal_connect(G_OBJECT(sakura.notebook), "page-removed", G_CALLBACK(sakura_page_removed_cb), NULL);
		g_signal_connect(G_OBJECT(sakura.notebook), "focus-in-event", G_CALLBACK(sakura_notebook_focus_cb), NULL);

		gtk_widget_show_all(sakura.notebook);
		if (!sakura.show_scrollbar) {
			gtk_widget_hide(sk_tab->scrollbar);
		}

		gtk_widget_show(sakura.main_window);

		sakura_set_colors();
#ifdef GDK_WINDOWING_X11
		/* Set WINDOWID env variable */
		GdkDisplay *display = gdk_display_get_default();

		if (GDK_IS_X11_DISPLAY (display)) {
			GdkWindow *gwin = gtk_widget_get_window (sakura.main_window);
			if (gwin != NULL) {
				guint winid = gdk_x11_window_get_xid (gwin);
				gchar *winidstr = g_strdup_printf ("%d", winid);
				g_setenv ("WINDOWID", winidstr, FALSE);
				g_free (winidstr);
			}
		}
#endif

		int command_argc = 0; char **command_argv = NULL;

		/* Execute command for the fist tab if we have one */
		if (option_execute||option_xterm_execute) {
			char *path;

			sakura_build_command(&command_argc, &command_argv);

			/* If the command is valid, run it */
			if (command_argc > 0) {
				path = g_find_program_in_path(command_argv[0]);

				if (!path) {
					sakura_error("%s command not found", command_argv[0]);
					command_argc = 0;
				}
				vte_terminal_spawn_async(VTE_TERMINAL(sk_tab->vte), VTE_PTY_NO_HELPER, NULL, command_argv, command_env,
						       	         G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL, sakura_spawn_callback, sk_tab);

				free(path);
				g_strfreev(command_argv);
			}
		} 

		/* Fork shell if there is no execute option or if the command is not valid */
		if ( (!option_execute && !option_xterm_execute) || (command_argc==0)) {
			if (option_hold == TRUE) {
				sakura_error("Hold option given without any command");
				option_hold = FALSE;
			}
			vte_terminal_spawn_async(VTE_TERMINAL(sk_tab->vte), VTE_PTY_NO_HELPER, cwd, sakura.argv, command_env,
					        G_SPAWN_SEARCH_PATH|G_SPAWN_FILE_AND_ARGV_ZERO, NULL, NULL, NULL, -1, NULL, sakura_spawn_callback, sk_tab);
		}

	/********** Not the first tab ************/
	} else {
		sakura_set_font();
		sakura_set_colors();
		gtk_widget_show_all(sk_tab->hbox);
		if (!sakura.show_scrollbar) {
			gtk_widget_hide(sk_tab->scrollbar);
		}

		if (npages == 2 && sakura.show_tab_bar != SHOW_TAB_BAR_NEVER) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
			sakura_set_size();
		}
		/* Call set_current page after showing the widget: gtk ignores this
		 * function in the window is not visible *sigh*. Gtk documentation
		 * says this is for "historical" reasons. Me arse */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), index);

		int command_argc = 0; char **command_argv = NULL;

		/* Execute command (only in the first run) for additional tabs if we have one */
		if ((option_execute||option_xterm_execute) && sakura.first_run) {
			char *path;

			sakura_build_command(&command_argc, &command_argv);

			/* If the command is valid, run it */
			if (command_argc > 0) {
				path = g_find_program_in_path(command_argv[0]);

				if (!path) {
					sakura_error("%s command not found", command_argv[0]);
					command_argc = 0;
				}
				vte_terminal_spawn_async(VTE_TERMINAL(sk_tab->vte), VTE_PTY_NO_HELPER, NULL, command_argv, command_env,
						       	         G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL, sakura_spawn_callback, sk_tab);

				free(path);
				g_strfreev(command_argv);
			}
		}

		/* Fork shell if there is no execute option or if the command is not valid */
		if ( (!option_execute && !option_xterm_execute) || (command_argc==0)) {
			if (option_hold == TRUE) {
				sakura_error("Hold option given without any command");
				option_hold = FALSE;
			}
			vte_terminal_spawn_async(VTE_TERMINAL(sk_tab->vte), VTE_PTY_NO_HELPER, cwd, sakura.argv, command_env,
					        G_SPAWN_SEARCH_PATH|G_SPAWN_FILE_AND_ARGV_ZERO, NULL, NULL, NULL, -1, NULL, sakura_spawn_callback, sk_tab);
		}
	}

	free(cwd);

	/* Applying tab title pattern from config (https://answers.launchpad.net/sakura/+question/267951) */
	if (sakura.tab_default_title != NULL) {
		default_label_text = sakura.tab_default_title;
		sk_tab->label_set_byuser = true;
	} else {
		sk_tab->label_set_byuser=false;
	}

	/* Set the default title text (NULL is valid) */
	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	sakura_set_tab_label_text(default_label_text, page);

	/* Init vte terminal */
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(sk_tab->vte), sakura.scroll_lines);
	vte_terminal_match_add_regex(VTE_TERMINAL(sk_tab->vte), sakura.http_vteregexp, PCRE2_CASELESS);
	vte_terminal_match_add_regex(VTE_TERMINAL(sk_tab->vte), sakura.mail_vteregexp, PCRE2_CASELESS);
	vte_terminal_set_mouse_autohide(VTE_TERMINAL(sk_tab->vte), TRUE);
	vte_terminal_set_backspace_binding(VTE_TERMINAL(sk_tab->vte), VTE_ERASE_ASCII_DELETE);
	vte_terminal_set_word_char_exceptions(VTE_TERMINAL(sk_tab->vte), sakura.word_chars);
	vte_terminal_set_audible_bell (VTE_TERMINAL(sk_tab->vte), sakura.audible_bell ? TRUE : FALSE);
	vte_terminal_set_cursor_blink_mode (VTE_TERMINAL(sk_tab->vte), sakura.blinking_cursor ? VTE_CURSOR_BLINK_ON : VTE_CURSOR_BLINK_OFF);
	vte_terminal_set_cursor_shape (VTE_TERMINAL(sk_tab->vte), sakura.cursor_type);

}


/* Do all the work necessary before & after deleting the tab passed as a parameter */
static void
sakura_close_tab (gint page)
{
	gint npages, response; pid_t pgid;
	struct sakura_tab *sk_tab;
	GtkWidget *dialog;

	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	sk_tab = sakura_get_sktab(sakura, page);

	/* Only write configuration to disk if it's the last tab */
	if (npages == 1) {
		sakura_config_done();
	}

	/* Check if there are running processes for this tab. Use tcgetpgrp to compare to the shell PGID */
	pgid = tcgetpgrp(vte_pty_get_fd(vte_terminal_get_pty(VTE_TERMINAL(sk_tab->vte))));

	if ( (pgid != -1) && (pgid != sk_tab->pid) && (!sakura.less_questions) ) {
		dialog=gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                              _("There is a running process in this terminal.\n\nDo you really want to close it?"));
		response=gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		if (response==GTK_RESPONSE_YES)
			sakura_del_tab(page);

	} else /* No processes */
		sakura_del_tab(page);

	/* And destroy sakura if it's the last tab */
	if (npages == 1)
		sakura_destroy();
}


/* Delete the notebook tab passed as a parameter */
static void
sakura_del_tab(gint page)
{
	struct sakura_tab *sk_tab;
	gint npages;

	sk_tab = sakura_get_sktab(sakura, page);
	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Do the first tab checks BEFORE deleting the tab, to ensure correct
	 * sizes are calculated when the tab is deleted */
	if (npages == 2) {
		if (sakura.show_tab_bar == SHOW_TAB_BAR_ALWAYS) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		} else {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}
	}

	gtk_widget_hide(sk_tab->hbox);
	g_signal_handler_disconnect (sk_tab->vte, sk_tab->exit_handler_id);
	gtk_notebook_remove_page(GTK_NOTEBOOK(sakura.notebook), page);

	/* Find the next page, if it exists, and grab focus */
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) > 0) {
		page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
		sk_tab = sakura_get_sktab(sakura, page);
		gtk_widget_grab_focus(sk_tab->vte);
	}
}


/* Save configuration */
static void
sakura_config_done()
{
	GError *gerror = NULL;
	gsize len = 0;

	/* Don't save config file. Option only available thru the config file for users who know the risks */
	if (sakura.dont_save)
		return;

	gchar *cfgdata = g_key_file_to_data(sakura.cfg, &len, &gerror);
	if (!cfgdata) {
		fprintf(stderr, "%s\n", gerror->message);
		exit(EXIT_FAILURE);
	}

	bool overwrite = false;

	/* If there's been changes by another sakura process, ask whether to overwrite it or not */
	/* And if less_questions options is selected don't overwrite */
	if (sakura.externally_modified && !sakura.config_modified && !sakura.less_questions) {
		GtkWidget *dialog;
		gint response;

		dialog = gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
						GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
						_("Configuration has been modified by another process. Overwrite?"));

		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		if (response == GTK_RESPONSE_YES)
			overwrite = true;
	}

	/* Write to file IF there's been changes of IF we want to overwrite another process changes */
	if (sakura.config_modified || overwrite) {
		GIOChannel *cfgfile = g_io_channel_new_file(sakura.configfile, "w", &gerror);
		if (!cfgfile) {
			fprintf(stderr, "%s\n", gerror->message);
			g_error_free(gerror);
			exit(EXIT_FAILURE);
		}

		/* FIXME: if the number of chars written is not "len", something happened.
		 * Check for errors appropriately...*/
		GIOStatus status = g_io_channel_write_chars(cfgfile, cfgdata, len, NULL, &gerror);
		if (status != G_IO_STATUS_NORMAL) {
			// FIXME: we should deal with temporary failures (G_IO_STATUS_AGAIN)
			fprintf(stderr, "%s\n", gerror->message);
			g_error_free(gerror);
			exit(EXIT_FAILURE);
		}
		g_io_channel_shutdown(cfgfile, TRUE, &gerror);
		g_io_channel_unref(cfgfile);
	}
}


/*******************/
/* Misc. functions */
/*******************/

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


static void
sakura_build_command(int *command_argc, char ***command_argv)
{
	GError *gerror = NULL;

	if (option_execute) {
		/* -x option: only one argument */
		if (!g_shell_parse_argv(option_execute, command_argc, command_argv, &gerror)) {
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
			g_error_free(gerror);
		}
	} else {
		/* -e option: last in the command line, takes all extra arguments */
		if (option_xterm_args) {

			guint size=0, i=0; gchar **quoted_args=NULL;

			do { size++; } while (option_xterm_args[size]); /* Get option_xterm_args size */

			/* Quote all arguments to be able to use parameters with spaces like filenames */
			quoted_args = malloc(sizeof(char *) * (size+1));
			while (option_xterm_args[i]) {
				quoted_args[i] = g_shell_quote(option_xterm_args[i]); i++;
			} 
			quoted_args[i]=NULL;

			/* Join all arguments and parse them to create argc&argv */
			gchar *command_joined= command_joined = g_strjoinv(" ", quoted_args);
			if (!g_shell_parse_argv(command_joined, command_argc, command_argv, &gerror)) {
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

			if (gerror != NULL)
				g_error_free(gerror);
			g_free(command_joined);
			g_strfreev(quoted_args);
		}
	}
}


static void
sakura_set_keybind(const gchar *key, guint value)
{
	char *valname;

	valname = gdk_keyval_name(value);
	g_key_file_set_string(sakura.cfg, cfg_group, key, valname);
	sakura.config_modified = TRUE;
}


static guint
sakura_get_keybind(const gchar *key)
{
	gchar *value;
	guint retval = GDK_KEY_VoidSymbol;

	value = g_key_file_get_string(sakura.cfg, cfg_group, key, NULL);
	if (value != NULL) {
		retval = gdk_keyval_from_name(value);
		g_free(value);
	}

	/* For backwards compatibility with integer values */
	/* If gdk_keyval_from_name fail, it seems to be integer value*/
	if ((retval == GDK_KEY_VoidSymbol) || (retval == 0)) {
		retval = g_key_file_get_integer(sakura.cfg, cfg_group, key, NULL);
	}

	/* Always use uppercase value as keyval */
	return gdk_keyval_to_upper(retval);
}


/* Retrieve the cwd of the specified sk_tab page.
 * Original function was from terminal-screen.c of gnome-terminal, copyright (C) 2001 Havoc Pennington
 * Adapted by Hong Jen Yee, non-linux shit removed by David Gómez */
static char *
sakura_get_term_cwd(struct sakura_tab* sk_tab)
{
	char *cwd = NULL;

	if (sk_tab->pid >= 0) {
		char *file, *buf;
		struct stat sb;
		int len;

		file = g_strdup_printf ("/proc/%d/cwd", sk_tab->pid);

		if (g_stat(file, &sb) == -1) {
			g_free(file);
			return cwd;
		}

		buf = malloc(sb.st_size + 1);

		if (buf == NULL) {
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


static guint
sakura_tokeycode (guint key)
{
	GdkKeymap *keymap;
	GdkKeymapKey *keys;
	gint n_keys;
	guint res = 0;

	keymap = gdk_keymap_get_for_display(gdk_display_get_default());

	if (gdk_keymap_get_entries_for_keyval(keymap, key, &keys, &n_keys)) {
		if (n_keys > 0) {
			res = keys[0].keycode;
		}
		g_free(keys);
	}

	return res;
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
		if (chdir(home_directory)) {
			fprintf(stderr, _("Cannot change working directory\n"));
			exit(1);
		}
	}
}


/********/
/* main */
/********/

int
main(int argc, char **argv)
{
	gchar *localedir;
	int i; int n;
	char **nargv; int nargc;
	gboolean have_e;

	/* Localization */
	setlocale(LC_ALL, "");
	localedir = g_strdup_printf("%s/locale", DATADIR);
	textdomain(GETTEXT_PACKAGE);
	bindtextdomain(GETTEXT_PACKAGE, localedir);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	g_free(localedir);

	/* Rewrites argv to include a -- after the -e argument this is required to make
	 * sure GOption doesn't grab any arguments meant for the command being called */

	/* Initialize nargv */
	nargv = (char**)calloc((argc+1), sizeof(char*));
	n = 0; nargc = argc;
	have_e = FALSE;

	for (i=0; i<argc; i++) {
		if (!have_e && g_strcmp0(argv[i],"-e") == 0)
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
	GError *error=NULL;
	GOptionContext *context; GOptionGroup *option_group;

	context = g_option_context_new (_("- vte-based terminal emulator"));
	option_group = gtk_get_option_group(TRUE);
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_group_set_translation_domain(option_group, GETTEXT_PACKAGE);
	g_option_context_add_group (context, option_group);
	if (!g_option_context_parse (context, &nargc, &nargv, &error)) {
		fprintf(stderr, "%s\n", error->message);
		g_error_free(error);
		exit(1);
	}

	g_option_context_free(context);

	if (option_workdir && chdir(option_workdir)) {
		fprintf(stderr, _("Cannot change working directory\n"));
		exit(1);
	}

	if (option_version) {
		fprintf(stderr, _("sakura version is %s\n"), VERSION);
		exit(1);
	}

	if (option_ntabs <= 0) {
		option_ntabs = 1;
	}

	/* Init stuff */
	gtk_init(&nargc, &nargv); g_strfreev(nargv);
	sakura_init();

	/* Add initial tabs (1 by default) */
	for (i=0; i<option_ntabs; i++)
		sakura_add_tab();

	/* Post init stuff */
	sakura.first_run=false;
	g_strfreev(option_xterm_args);

	sakura_sanitize_working_directory();

	gtk_main();

	return 0;
}

