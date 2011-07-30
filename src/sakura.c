/*******************************************************************************
 *  Filename: sakura.c
 *  Description: VTE-based terminal emulator
 *
 *           Copyright (C) 2006-2008  David Gómez <david@pleyades.net>
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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <locale.h>
#include <libintl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
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

/* Color palettes. Color lists borrowed from gnome-terminal source (thanks! ;)) */
const GdkColor tango_palette[PALETTE_SIZE] =
{
	{ 0, 0x2e2e, 0x3434, 0x3636 },
	{ 0, 0xcccc, 0x0000, 0x0000 },
	{ 0, 0x4e4e, 0x9a9a, 0x0606 },
	{ 0, 0xc4c4, 0xa0a0, 0x0000 },
	{ 0, 0x3434, 0x6565, 0xa4a4 },
	{ 0, 0x7575, 0x5050, 0x7b7b },
	{ 0, 0x0606, 0x9820, 0x9a9a },
	{ 0, 0xd3d3, 0xd7d7, 0xcfcf },
	{ 0, 0x5555, 0x5757, 0x5353 },
	{ 0, 0xefef, 0x2929, 0x2929 },
	{ 0, 0x8a8a, 0xe2e2, 0x3434 },
	{ 0, 0xfcfc, 0xe9e9, 0x4f4f },
	{ 0, 0x7272, 0x9f9f, 0xcfcf },
	{ 0, 0xadad, 0x7f7f, 0xa8a8 },
	{ 0, 0x3434, 0xe2e2, 0xe2e2 },
	{ 0, 0xeeee, 0xeeee, 0xecec }
};

const GdkColor linux_palette[PALETTE_SIZE] =
{
	{ 0, 0x0000, 0x0000, 0x0000 },
	{ 0, 0xaaaa, 0x0000, 0x0000 },
	{ 0, 0x0000, 0xaaaa, 0x0000 },
	{ 0, 0xaaaa, 0x5555, 0x0000 },
	{ 0, 0x0000, 0x0000, 0xaaaa },
	{ 0, 0xaaaa, 0x0000, 0xaaaa },
	{ 0, 0x0000, 0xaaaa, 0xaaaa },
	{ 0, 0xaaaa, 0xaaaa, 0xaaaa },
	{ 0, 0x5555, 0x5555, 0x5555 },
	{ 0, 0xffff, 0x5555, 0x5555 },
	{ 0, 0x5555, 0xffff, 0x5555 },
	{ 0, 0xffff, 0xffff, 0x5555 },
	{ 0, 0x5555, 0x5555, 0xffff },
	{ 0, 0xffff, 0x5555, 0xffff },
	{ 0, 0x5555, 0xffff, 0xffff },
	{ 0, 0xffff, 0xffff, 0xffff }
};

const GdkColor xterm_palette[PALETTE_SIZE] =
{
	{0, 0x0000, 0x0000, 0x0000 },
	{0, 0xcdcb, 0x0000, 0x0000 },
	{0, 0x0000, 0xcdcb, 0x0000 },
	{0, 0xcdcb, 0xcdcb, 0x0000 },
	{0, 0x1e1a, 0x908f, 0xffff },
	{0, 0xcdcb, 0x0000, 0xcdcb },
	{0, 0x0000, 0xcdcb, 0xcdcb },
	{0, 0xe5e2, 0xe5e2, 0xe5e2 },
	{0, 0x4ccc, 0x4ccc, 0x4ccc },
	{0, 0xffff, 0x0000, 0x0000 },
	{0, 0x0000, 0xffff, 0x0000 },
	{0, 0xffff, 0xffff, 0x0000 },
	{0, 0x4645, 0x8281, 0xb4ae },
	{0, 0xffff, 0x0000, 0xffff },
	{0, 0x0000, 0xffff, 0xffff },
	{0, 0xffff, 0xffff, 0xffff }
};

const GdkColor rxvt_palette[PALETTE_SIZE] =
{
	{ 0, 0x0000, 0x0000, 0x0000 },
	{ 0, 0xcdcd, 0x0000, 0x0000 },
	{ 0, 0x0000, 0xcdcd, 0x0000 },
	{ 0, 0xcdcd, 0xcdcd, 0x0000 },
	{ 0, 0x0000, 0x0000, 0xcdcd },
	{ 0, 0xcdcd, 0x0000, 0xcdcd },
	{ 0, 0x0000, 0xcdcd, 0xcdcd },
	{ 0, 0xfafa, 0xebeb, 0xd7d7 },
	{ 0, 0x4040, 0x4040, 0x4040 },
	{ 0, 0xffff, 0x0000, 0x0000 },
	{ 0, 0x0000, 0xffff, 0x0000 },
	{ 0, 0xffff, 0xffff, 0x0000 },
	{ 0, 0x0000, 0x0000, 0xffff },
	{ 0, 0xffff, 0x0000, 0xffff },
	{ 0, 0x0000, 0xffff, 0xffff },
	{ 0, 0xffff, 0xffff, 0xffff }
};

static struct {
	GtkWidget *main_window;
	GtkWidget *notebook;
	GtkWidget *menu;
	GtkWidget *im_menu;
	//GtkWidget *labels_menu;
	PangoFontDescription *font;
	GdkColor forecolor;
	GdkColor backcolor;
	guint16 backalpha;
	bool has_rgba;
	char *current_match;
	guint width;
	guint height;
	glong columns;
	glong rows;
	gint char_width;
	gint char_height;
	gint label_count;
	bool fake_transparency;
	float opacity_level;
	char *opacity_level_percent;
	bool *opacity;
	bool first_tab;
	bool show_scrollbar;
	bool show_closebutton;
	bool audible_bell;
	bool visible_bell;
	bool blinking_cursor;
	bool borderless;
	bool maximized;
	bool full_screen;
	bool keep_fc;				/* Global flag to indicate that we don't want changes in the files and columns values */
	bool config_modified;		/* Configuration has been modified */
	bool externally_modified;	/* Configuration file has been modified by another proccess */
	GtkWidget *item_clear_background; /* We include here only the items which need to be hided */
	GtkWidget *item_copy_link;
	GtkWidget *item_open_link;
	GtkWidget *open_link_separator;
	GKeyFile *cfg;
	char *configfile;
	char *background;
	char *word_chars;
	const GdkColor *palette;
	gint add_tab_accelerator;
	gint del_tab_accelerator;
	gint switch_tab_accelerator;
	gint copy_accelerator;
	gint scrollbar_accelerator;
	gint open_url_accelerator;
	gint add_tab_key;
	gint del_tab_key;
	gint prev_tab_key;
	gint next_tab_key;
	gint new_window_key;
	gint copy_key;
	gint paste_key;
	gint scrollbar_key;
	gint fullscreen_key;
	GRegex *http_regexp;
	char *argv[2];
} sakura;

struct terminal {
	GtkWidget *hbox;
	GtkWidget *vte;     /* Reference to VTE terminal */
	GPid pid;          /* pid of the forked proccess */
	GtkWidget *scrollbar;
	GtkWidget *label;
	gchar *label_text;
	GtkBorder border;   /* inner-property data */
};


#define ICON_FILE "terminal-tango.svg"
#define SCROLL_LINES 4096
#define HTTP_REGEXP "(ftp|http)s?://[-a-zA-Z0-9.?$%&/=_~#.,:;+]*"
#define CONFIGFILE "sakura.conf"
#define DEFAULT_COLUMNS 80
#define DEFAULT_ROWS 24
#define DEFAULT_FONT "monospace 11"
#define DEFAULT_WORD_CHARS  "-A-Za-z0-9,./?%&#_~"
#define DEFAULT_PALETTE "linux"
#define DEFAULT_ADD_TAB_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_DEL_TAB_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_SWITCH_TAB_ACCELERATOR  (GDK_MOD1_MASK)
#define DEFAULT_COPY_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_SCROLLBAR_ACCELERATOR  (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_OPEN_URL_ACCELERATOR (GDK_CONTROL_MASK|GDK_SHIFT_MASK)
#define DEFAULT_ADD_TAB_KEY  GDK_T
#define DEFAULT_DEL_TAB_KEY  GDK_W
#define DEFAULT_PREV_TAB_KEY  GDK_Left
#define DEFAULT_NEXT_TAB_KEY  GDK_Right
#define DEFAULT_NEW_WINDOW_KEY GDK_N
#define DEFAULT_COPY_KEY  GDK_C
#define DEFAULT_PASTE_KEY  GDK_V
#define DEFAULT_SCROLLBAR_KEY  GDK_S
#define DEFAULT_FULLSCREEN_KEY  GDK_F11
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
static void     sakura_opacity_dialog (GtkWidget *, void *);
static void     sakura_set_title_dialog (GtkWidget *, void *);
static void     sakura_select_background_dialog (GtkWidget *, void *);
static void     sakura_new_tab (GtkWidget *, void *);
static void     sakura_close_tab (GtkWidget *, void *);
static void     sakura_new_window (GtkWidget *, void *);
static void     sakura_full_screen (GtkWidget *, void *);
static void     sakura_open_url (GtkWidget *, void *);
static void     sakura_clear (GtkWidget *, void *);
static gboolean sakura_resized_window(GtkWidget *, GdkEventConfigure *, void *);
static void     sakura_setname_entry_changed(GtkWidget *, void *);
static void     sakura_copy(GtkWidget *, void *);
static void     sakura_paste(GtkWidget *, void *);
static void     sakura_show_first_tab (GtkWidget *widget, void *data);
static void     sakura_show_close_button (GtkWidget *widget, void *data);
static void		sakura_show_scrollbar(GtkWidget *, void *);
static void     sakura_closebutton_clicked(GtkWidget *, void *);
static void		sakura_conf_changed(GtkWidget *, void *);

/* Misc */
static void     sakura_error(const char *, ...);

/* Functions */
static void     sakura_init();
static void     sakura_init_popup();
static void     sakura_destroy();
static void     sakura_add_tab();
static void     sakura_del_tab();
static void     sakura_set_font();
static void     sakura_set_geometry_hints();
static void     sakura_set_size(gint, gint);
static void     sakura_kill_child();
static void     sakura_set_bgimage();
static void     sakura_set_config_key(const gchar *, guint);
static guint    sakura_get_config_key(const gchar *);
static void		sakura_config_done();

static const char *option_font;
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

static GOptionEntry entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &option_version, N_("Print version number"), NULL },
	{ "font", 'f', 0, G_OPTION_ARG_STRING, &option_font, N_("Select initial terminal font"), NULL },
	{ "ntabs", 'n', 0, G_OPTION_ARG_INT, &option_ntabs, N_("Select initial number of tabs"), NULL },
	{ "execute", 'x', 0, G_OPTION_ARG_STRING, &option_execute, N_("Execute command"), NULL },
	{ "xterm-execute", 'e', 0, G_OPTION_ARG_NONE, &option_xterm_execute, N_("Execute command (xterm compatible)"), NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &option_xterm_args, NULL, NULL },
	{ "login", 'l', 0, G_OPTION_ARG_NONE, &option_login, N_("Login shell"), NULL },
	{ "title", 't', 0, G_OPTION_ARG_STRING, &option_title, N_("Set window title"), NULL },
	{ "columns", 'c', 0, G_OPTION_ARG_INT, &option_columns, N_("Set columns number"), NULL },
	{ "rows", 'r', 0, G_OPTION_ARG_INT, &option_rows, N_("Set rows number"), NULL },
	{ "hold", 'h', 0, G_OPTION_ARG_NONE, &option_hold, N_("Hold window after execute command"), NULL },
    { "geometry", 0, 0, G_OPTION_ARG_STRING, &option_geometry, N_("X geometry specification"), NULL },
    { NULL }
};


static
gboolean sakura_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	unsigned int topage=0;
	gint npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));

	if (event->type!=GDK_KEY_PRESS) return FALSE;

	/* add_tab_accelerator + T or del_tab_accelerator + W pressed */
	if ( (event->state & sakura.add_tab_accelerator)==sakura.add_tab_accelerator &&
         event->keyval==sakura.add_tab_key ) {
		sakura_add_tab();
        return TRUE;
    } else if ( (event->state & sakura.del_tab_accelerator)==sakura.del_tab_accelerator &&
                event->keyval==sakura.del_tab_key ) {
        sakura_kill_child();
		/* Delete current tab */
        sakura_del_tab(page);
        if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
            sakura_destroy();
        return TRUE;
	}

	/* switch_tab_accelerator + number pressed / switch_tab_accelerator + Left-Right cursor */
	if ( (event->state & sakura.switch_tab_accelerator) == sakura.switch_tab_accelerator ) {
		if ((event->keyval>=GDK_1) && (event->keyval<=GDK_9) && (event->keyval<=GDK_1-1+npages)
				&& (event->keyval!=GDK_1+gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook)))) {
			switch(event->keyval) {
				case GDK_1: topage=0; break;
				case GDK_2: topage=1; break;
				case GDK_3: topage=2; break;
				case GDK_4: topage=3; break;
				case GDK_5: topage=4; break;
				case GDK_6: topage=5; break;
				case GDK_7: topage=6; break;
				case GDK_8: topage=7; break;
				case GDK_9: topage=8; break;
			}
			if (topage <= npages)
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), topage);
			return TRUE;
		} else if (event->keyval==sakura.prev_tab_key) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==0) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), npages-1);
			} else {
				gtk_notebook_prev_page(GTK_NOTEBOOK(sakura.notebook));
			}
			return TRUE;
		} else if (event->keyval==sakura.next_tab_key) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==(npages-1)) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), 0);
			} else {
				gtk_notebook_next_page(GTK_NOTEBOOK(sakura.notebook));
			}
			return TRUE;
		}
	}

	/* copy_accelerator-[C/V] pressed */
	SAY("copy acc: %d", sakura.copy_accelerator);
	SAY("ev+copy: %d", (event->state & sakura.copy_accelerator));
	if ( (event->state & sakura.copy_accelerator)==sakura.copy_accelerator ) {
		SAY("%d %d", event->keyval, sakura.copy_key);
		if (event->keyval==sakura.copy_key) {
			sakura_copy(NULL, NULL);
			return TRUE;
		} else if (event->keyval==sakura.paste_key) {
			sakura_paste(NULL, NULL);
			return TRUE;
		} else if (event->keyval==sakura.new_window_key) {
			sakura_new_window(NULL, NULL);
			return TRUE;
		}
	}

	/* scrollbar_accelerator-[S] pressed */
	if ( (event->state & sakura.scrollbar_accelerator)==sakura.scrollbar_accelerator ) {
		if (event->keyval==sakura.scrollbar_key) {
			sakura_show_scrollbar(NULL, NULL);
			sakura_set_size(sakura.columns, sakura.rows);
			return TRUE;
		}
	}

	/* F11 (fullscreen) pressed */
	if (event->keyval==sakura.fullscreen_key){
		sakura_full_screen(NULL, NULL);
		return TRUE;
	}

	return FALSE;
}


static gboolean
sakura_button_press(GtkWidget *widget, GdkEventButton *button_event, gpointer user_data)
{
	struct terminal *term;
	glong column, row;
	int page, tag;

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
		sakura_set_size(sakura.columns, sakura.rows);
	}
}


static void
sakura_increase_font (GtkWidget *widget, void *data)
{
	gint size;

	size=pango_font_description_get_size(sakura.font);
	pango_font_description_set_size(sakura.font, ((size/PANGO_SCALE)+1) * PANGO_SCALE);

	sakura_set_font();
	sakura_set_size(sakura.columns, sakura.rows);
}


static void
sakura_decrease_font (GtkWidget *widget, void *data)
{
	gint size;

	size=pango_font_description_get_size(sakura.font);
	pango_font_description_set_size(sakura.font, ((size/PANGO_SCALE)-1) * PANGO_SCALE);

	sakura_set_font();
	sakura_set_size(sakura.columns, sakura.rows);
}


static void
sakura_child_exited (GtkWidget *widget, void *data)
{
	int status, page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	sakura_config_done();

	if (option_hold==TRUE) {
		SAY("hold option has been activated");
		return;
	}

	SAY("waiting for terminal pid %d", term->pid);

	waitpid(term->pid, &status, WNOHANG);
	/* TODO: check wait return */

	sakura_del_tab(page);

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
		sakura_destroy();
}


static void
sakura_eof (GtkWidget *widget, void *data)
{
	int status;
	struct terminal *term;

	SAY("Got EOF signal");

	sakura_config_done();

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

		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
			sakura_destroy();
	}
}


static void
sakura_title_changed (GtkWidget *widget, void *data)
{
	int page;
	struct terminal *term;
	const char *title;
	gchar *window_title, *chopped_title;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);
	title = vte_terminal_get_window_title(VTE_TERMINAL(term->vte));
	window_title = g_strconcat(title, " - sakura", NULL);

	if ( (title!=NULL) && (g_strcmp0(title, "") !=0) ) {
		chopped_title = g_strndup(title, 40); /* Should it be configurable? */
		gtk_label_set_text(GTK_LABEL(term->label), chopped_title);
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), window_title);
		free(chopped_title);
	} else { /* Use the default values */
		gtk_label_set_text(GTK_LABEL(term->label), term->label_text);
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), "sakura");
	}

	g_free(window_title);

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
			guint response;

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
			g_io_channel_close(cfgfile);
		}
	}
}


static gboolean
sakura_delete_event (GtkWidget *widget, void *data)
{
	struct terminal *term;
	GtkWidget *dialog;
	guint response;
	gint npages;
	gint i;
	pid_t pgid;

	npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Check for each tab if there are running processes. Use tcgetpgrp to compare to the shell PGID */
	for (i=0; i < npages; i++) {

		term = sakura_get_page_term(sakura, i);
		pgid = tcgetpgrp(vte_terminal_get_pty(VTE_TERMINAL(term->vte)));

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

	sakura_config_done();
	return FALSE;
}


static void
sakura_destroy_window (GtkWidget *widget, void *data)
{
	sakura_destroy();
}


static void
sakura_font_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *font_dialog;
	gint response;

	font_dialog=gtk_font_selection_dialog_new(_("Select font"));
	gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(font_dialog),
	                                        pango_font_description_to_string(sakura.font));

	response=gtk_dialog_run(GTK_DIALOG(font_dialog));

	if (response==GTK_RESPONSE_OK) {
		pango_font_description_free(sakura.font);
		sakura.font=pango_font_description_from_string(gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(font_dialog)));
		sakura_set_font();
		sakura_set_size(sakura.columns, sakura.rows);
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
	int page;
	struct terminal *term;
	const gchar *text;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	input_dialog=gtk_dialog_new_with_buttons(_("Set name"), GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
	                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	                                         GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(input_dialog), TRUE);

	/* Set style */
	gtk_widget_set_name (input_dialog, "set-name-dialog");
	gtk_rc_parse_string ("widget \"set-name-dialog\" style \"hig-dialog\"\n");

	name_hbox=gtk_hbox_new(FALSE, 0);
	entry=gtk_entry_new();
	label=gtk_label_new(_("Tab new text"));
	/* Set tab label as entry default text (when first tab is not displayed, get_tab_label_text
	   returns a null value, so check accordingly */
	text = gtk_notebook_get_tab_label_text(GTK_NOTEBOOK(sakura.notebook), term->hbox);
	if (text) {
		gtk_entry_set_text(GTK_ENTRY(entry), text);
	}
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(name_hbox), label, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(name_hbox), entry, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(input_dialog)->vbox), name_hbox, FALSE, FALSE, 12);
	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(sakura_setname_entry_changed), input_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT, FALSE);

	gtk_widget_show_all(name_hbox);

	response=gtk_dialog_run(GTK_DIALOG(input_dialog));
	if (response==GTK_RESPONSE_ACCEPT) {
		gtk_label_set_text(GTK_LABEL(term->label), gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	gtk_widget_destroy(input_dialog);
}


static void
sakura_color_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *color_dialog;
	GtkWidget *label1, *label2;
	GtkWidget *buttonfore, *buttonback;
	GtkWidget *hbox_fore, *hbox_back;
	gint response;
	int page;
	int i, n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	color_dialog=gtk_dialog_new_with_buttons(_("Select color"), GTK_WINDOW(sakura.main_window),
	                                                            GTK_DIALOG_MODAL,
	                                                            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	                                                            GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(color_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(color_dialog), TRUE);
	/* Set style */
	gtk_widget_set_name (color_dialog, "set-color-dialog");
	gtk_rc_parse_string ("widget \"set-color-dialog\" style \"hig-dialog\"\n");

	hbox_fore=gtk_hbox_new(FALSE, 12);
	hbox_back=gtk_hbox_new(FALSE, 12);
	label1=gtk_label_new(_("Select foreground color:"));
	label2=gtk_label_new(_("Select background color:"));
	buttonfore=gtk_color_button_new_with_color(&sakura.forecolor);
	buttonback=gtk_color_button_new_with_color(&sakura.backcolor);

	if (sakura.has_rgba) {
		gtk_color_button_set_use_alpha(GTK_COLOR_BUTTON(buttonback), TRUE);
		gtk_color_button_set_alpha(GTK_COLOR_BUTTON(buttonback), sakura.backalpha);
	}

	gtk_box_pack_start(GTK_BOX(hbox_fore), label1, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(hbox_fore), buttonfore, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox_back), label2, FALSE, FALSE, 12);
	gtk_box_pack_end(GTK_BOX(hbox_back), buttonback, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(color_dialog)->vbox), hbox_fore, FALSE, FALSE, 6);
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(color_dialog)->vbox), hbox_back, FALSE, FALSE, 6);

	gtk_widget_show_all(GTK_DIALOG(color_dialog)->vbox);

	response=gtk_dialog_run(GTK_DIALOG(color_dialog));

	if (response==GTK_RESPONSE_ACCEPT) {
		gtk_color_button_get_color(GTK_COLOR_BUTTON(buttonfore), &sakura.forecolor);
		gtk_color_button_get_color(GTK_COLOR_BUTTON(buttonback), &sakura.backcolor);

		if (sakura.has_rgba) {
			sakura.backalpha = gtk_color_button_get_alpha(GTK_COLOR_BUTTON(buttonback));
		}

		for (i = (n_pages - 1); i >= 0; i--) {
			term = sakura_get_page_term(sakura, i);
			if (sakura.has_rgba) {
				/* HACK: need to force change the background to make this work.
				   User's with slow workstations may see a flicker when this happens. */
				vte_terminal_set_color_background(VTE_TERMINAL (term->vte), &sakura.forecolor);
				vte_terminal_set_opacity(VTE_TERMINAL (term->vte), sakura.backalpha);
			}
			vte_terminal_set_colors(VTE_TERMINAL(term->vte), &sakura.forecolor, &sakura.backcolor,
			                        sakura.palette, PALETTE_SIZE);
		}

		gchar *cfgtmp;
		cfgtmp = g_strdup_printf("#%02x%02x%02x", sakura.forecolor.red >>8,
		                         sakura.forecolor.green>>8, sakura.forecolor.blue>>8);
		sakura_set_config_string("forecolor", cfgtmp);
		g_free(cfgtmp);

		cfgtmp = g_strdup_printf("#%02x%02x%02x", sakura.backcolor.red >>8,
		                         sakura.backcolor.green>>8, sakura.backcolor.blue>>8);
		sakura_set_config_string("backcolor", cfgtmp);
		g_free(cfgtmp);

		sakura_set_config_integer("backalpha", sakura.backalpha);

	}

	gtk_widget_destroy(color_dialog);
}


static void
sakura_opacity_check (GtkWidget *widget, void *data)
{
	bool state;

	state=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	if (state) {
		/* Enable spinbutton */
		gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
	} else {
		/* Disable spinbutton */
		gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
	}
}


/* FIXME: Merge this dialog and the colors dialog into one. This one is a mess */
static void
sakura_opacity_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *opacity_dialog, *spin_control, *spin_label, *check;
	GtkObject *spinner_adj;
	GtkWidget *dialog_hbox, *dialog_vbox, *dialog_spin_hbox;
	gint response;
	int page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	opacity_dialog=gtk_dialog_new_with_buttons(_("Opacity"), GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	                                         GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(opacity_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(opacity_dialog), TRUE);
	/* Set style */
	gtk_widget_set_name (opacity_dialog, "set-opacity-dialog");
	gtk_rc_parse_string ("widget \"set-opacity-dialog\" style \"hig-dialog\"\n");

	spinner_adj = gtk_adjustment_new (((1.0 - sakura.opacity_level) * 100), 0.0, 99.0, 1.0, 5.0, 0);
	spin_control = gtk_spin_button_new(GTK_ADJUSTMENT(spinner_adj), 1.0, 0);

	spin_label = gtk_label_new(_("Opacity level (%):"));
	check = gtk_check_button_new_with_label(_("Disable opacity"));
	dialog_hbox=gtk_hbox_new(FALSE, 0);
	dialog_vbox=gtk_vbox_new(FALSE, 0);
	dialog_spin_hbox=gtk_hbox_new(FALSE, 0);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(opacity_dialog)->vbox), dialog_hbox, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(dialog_hbox), dialog_vbox, FALSE, FALSE, 12);
	if (!sakura.has_rgba) { /* Ignore it if there is a composite manager */
		gtk_box_pack_start(GTK_BOX(dialog_vbox), check, FALSE, FALSE, 6);
	} else
		sakura.fake_transparency = TRUE;

	gtk_box_pack_start(GTK_BOX(dialog_spin_hbox), spin_label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(dialog_spin_hbox), spin_control, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), dialog_spin_hbox, TRUE, TRUE, 6);

	g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(sakura_opacity_check), spin_control);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), !sakura.fake_transparency);

	gtk_widget_show_all(dialog_hbox);

	response=gtk_dialog_run(GTK_DIALOG(opacity_dialog));
	if (response==GTK_RESPONSE_ACCEPT) {
		char *value;
		int i, n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

		value=g_strdup_printf("%d", gtk_spin_button_get_value_as_int((GtkSpinButton *) spin_control));
		sakura.opacity_level = ( ( 100 - (atof(value)) ) / 100 );
		sakura.opacity_level_percent = value;
		sakura.fake_transparency=!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check));
		if (sakura.has_rgba) sakura.fake_transparency= TRUE; /* Set it to true just to ignore it. I know, this is an ugly hack :-/ */

		if (sakura.fake_transparency) {

			/* Set transparency for all tabs */
			for (i = (n_pages - 1); i >= 0; i--) {
				term = sakura_get_page_term(sakura, i);
				if (sakura.has_rgba) {
					/* Another hack. Forecolor is not ok, we need a white one */
					GdkColor color; color.red=255; color.blue=255; color.green=255;
					vte_terminal_set_color_background(VTE_TERMINAL (term->vte), &color);
					/* Map opacity level to alpha */
					sakura.backalpha = (atol(value)*65535)/100;
					vte_terminal_set_opacity(VTE_TERMINAL (term->vte), sakura.backalpha);
					vte_terminal_set_colors(VTE_TERMINAL(term->vte), &sakura.forecolor, &sakura.backcolor,
					                        sakura.palette, PALETTE_SIZE);
					sakura_set_config_integer("backalpha", sakura.backalpha);
					sakura.fake_transparency = TRUE;
				} else {
					/* Fake transparency, there is no composite manager */
					vte_terminal_set_background_transparent(VTE_TERMINAL(term->vte), TRUE);
					vte_terminal_set_background_saturation(VTE_TERMINAL(term->vte), sakura.opacity_level);
					vte_terminal_set_background_tint_color(VTE_TERMINAL(term->vte), &sakura.backcolor);
					sakura.fake_transparency = TRUE;
					sakura_set_config_string("fake_transparency", "Yes");
				}
			}

		} else {

			/* Unset fake transparency for all tabs */
			for (i = (n_pages - 1); i >= 0; i--) {
				term = sakura_get_page_term(sakura, i);
				vte_terminal_set_background_transparent(VTE_TERMINAL(term->vte), FALSE);
				sakura.fake_transparency = FALSE;
				sakura_set_config_string("fake_transparency", "No");
			}

		}

		sakura_set_config_string("opacity_level", sakura.opacity_level_percent);
	}

	gtk_widget_destroy(opacity_dialog);
}


static void
sakura_set_title_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *title_dialog;
	GtkWidget *entry, *label;
	GtkWidget *title_hbox;
	gint response;
	int page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	title_dialog=gtk_dialog_new_with_buttons(_("Set window title"), GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
	                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	                                         GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(title_dialog), TRUE);
	/* Set style */
	gtk_widget_set_name (title_dialog, "set-title-dialog");
	gtk_rc_parse_string ("widget \"set-title-dialog\" style \"hig-dialog\"\n");

	entry=gtk_entry_new();
	label=gtk_label_new(_("New window title"));
	title_hbox=gtk_hbox_new(FALSE, 0);
	/* Set window label as entry default text */
	gtk_entry_set_text(GTK_ENTRY(entry), gtk_window_get_title(GTK_WINDOW(sakura.main_window)));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(title_hbox), label, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(title_hbox), entry, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(title_dialog)->vbox), title_hbox, FALSE, FALSE, 12);
	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(sakura_setname_entry_changed), title_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(title_dialog), GTK_RESPONSE_ACCEPT, FALSE);

	gtk_widget_show_all(title_hbox);

	response=gtk_dialog_run(GTK_DIALOG(title_dialog));
	if (response==GTK_RESPONSE_ACCEPT) {
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
	                                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                                                     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
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
			g_free( browser );
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
	int page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	gtk_widget_hide(sakura.item_clear_background);

	vte_terminal_set_background_image(VTE_TERMINAL(term->vte), NULL);

	// FIXME: is this really needed? IMHO, this should be done just before
	// dumping the config to the config file.
	sakura_set_config_string("background", "none");

	g_free(sakura.background);
	sakura.background=NULL;
}


static void
sakura_show_first_tab (GtkWidget *widget, void *data)
{
	int page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		sakura_set_config_string("show_always_first_tab", "Yes");
	} else {
		/* Only hide tabs if the notebook has one page */
		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 1) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}
		sakura_set_config_string("show_always_first_tab", "No");
	}
}


static void
sakura_show_close_button (GtkWidget *widget, void *data)
{
	int page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		sakura_set_config_boolean("closebutton", TRUE);
	} else {
		sakura_set_config_boolean("closebutton", FALSE);
	}
}


static void
sakura_show_scrollbar (GtkWidget *widget, void *data)
{
	int page;
	struct terminal *term;
	int n_pages;
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
}


static void
sakura_audible_bell (GtkWidget *widget, void *data)
{
	int page;
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
	int page;
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
	int page;
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
sakura_borderless (GtkWidget *widget, void *data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_window_set_decorated (GTK_WINDOW(sakura.main_window), FALSE);
		sakura_set_config_string("borderless", "Yes");
	} else {
		gtk_window_set_decorated (GTK_WINDOW(sakura.main_window), TRUE);
		sakura_set_config_string("borderless", "No");
	}
}


static void
sakura_maximized (GtkWidget *widget, void *data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_window_maximize (GTK_WINDOW(sakura.main_window));
		sakura_set_config_string("maximized", "Yes");
	} else {
		gtk_window_unmaximize (GTK_WINDOW(sakura.main_window));
		sakura_set_config_string("maximized", "No");
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
		} else if (strcmp(palette, "tango")==0) {
			sakura.palette=tango_palette;
		} else if (strcmp(palette, "xterm")==0) {
			sakura.palette=xterm_palette;
		} else {
			sakura.palette=rxvt_palette;
		}

		for (i = (n_pages - 1); i >= 0; i--) {
			term = sakura_get_page_term(sakura, i);
			vte_terminal_set_colors(VTE_TERMINAL(term->vte), &sakura.forecolor, &sakura.backcolor,
			                        sakura.palette, PALETTE_SIZE);
		}

		sakura_set_config_string("palette", palette);
	}
}


/* Every the window changes its size by an user action (resize, fullscreen), calculate
 * the new values for the number of columns and rows */
static void
sakura_calculate_row_col (gint width, gint height)
{
	struct terminal *term;
	gint x_padding, y_padding;
	gint n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	if (n_pages==-1) return;

	SAY("Calculating row_col");
	term = sakura_get_page_term(sakura, 0);

	/* This is to prevent a race with ConfigureEvents when the window is being destroyed */
	if (!VTE_IS_TERMINAL(term->vte)) return;

	vte_terminal_get_padding( VTE_TERMINAL(term->vte), &x_padding, &y_padding );
	sakura.char_width = vte_terminal_get_char_width(VTE_TERMINAL(term->vte));
	sakura.char_height = vte_terminal_get_char_height(VTE_TERMINAL(term->vte));
	/* Ignore resize events in sakura window is in fullscreen */
	if (!sakura.keep_fc) {
		/* We cannot trust in vte allocation values, they're unreliable */
		/* FIXME: Round values properly */
		sakura.columns = (width/sakura.char_width);
		sakura.rows = (height/sakura.char_height);
		sakura.keep_fc=false;
		SAY("new columns %ld and rows %ld", sakura.columns, sakura.rows);
	}
	sakura.width = sakura.main_window->allocation.width + x_padding;
	sakura.height = sakura.main_window->allocation.height + y_padding;
	//}
}


/* Retrieve the cwd of the specified term page.
 * Original function was from terminal-screen.c of gnome-terminal, copyright (C) 2001 Havoc Pennington
 * Adapted by Hong Jen Yee, non-linux shit removed by David Gómez */
static char*
sakura_get_term_cwd(struct terminal* term)
{
	char *cwd = NULL;

	if (term->pid >= 0) {
		char *file;
		char buf[PATH_MAX+1];
		int len;

		file = g_strdup_printf ("/proc/%d/cwd", term->pid);
		len = readlink (file, buf, sizeof (buf) - 1);

		if (len > 0 && buf[0] == '/') {
			buf[len] = '\0';
			cwd = g_strdup(buf);
		}

		g_free(file);
	}

	return cwd;
}


static gboolean
sakura_resized_window (GtkWidget *widget, GdkEventConfigure *event, void *data)
{
	if (event->width!=sakura.width || event->height!=sakura.height) {
		SAY("sakura w & h %d %d event w & h %d %d",
		sakura.width, sakura.height, event->width, event->height);
		/* Window has been resized by the user. Recalculate sizes */
		sakura_calculate_row_col (event->width, event->height);
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
	int page;
	struct terminal *term;

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	vte_terminal_copy_clipboard(VTE_TERMINAL(term->vte));
}


/* Parameters are never used */
static void
sakura_paste (GtkWidget *widget, void *data)
{
	int page;
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
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));

	sakura_del_tab(page);

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
		sakura_destroy();
}

static void
sakura_new_window (GtkWidget *widget, void *data)
{
	SAY("Forking a new process");
	pid_t pid = vfork();
	if (pid == 0) {
		execlp("sakura", "sakura", NULL);
	} else if (pid < 0) {
		fprintf(stderr, "Failed to fork\n");
	}
}


static void
sakura_full_screen (GtkWidget *widget, void *data)
{
	if (sakura.full_screen!=TRUE) {
		sakura.full_screen=TRUE;
		gtk_window_fullscreen(GTK_WINDOW(sakura.main_window));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(sakura.main_window));
		sakura.full_screen=FALSE;
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
	gint response;

	page = gtk_notebook_page_num(GTK_NOTEBOOK(sakura.notebook), hbox);
	term = sakura_get_page_term(sakura, page);

	/* Check if there are running processes for this tab. Use tcgetpgrp to compare to the shell PGID */
	pgid = tcgetpgrp(vte_terminal_get_pty(VTE_TERMINAL(term->vte)));

	if ( (pgid != -1) && (pgid != term->pid)) {
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

	term_data_id = g_quark_from_static_string("sakura_term");

	/* Harcode TERM enviroment variable. With versions of vte>=0.26.0 behaviour seems to be different
	   and if TERM is not defined we get errors from several applications */
	g_setenv("TERM", "xterm", FALSE);

	/* Config file initialization*/
	sakura.cfg = g_key_file_new();
	sakura.config_modified=false;

	configdir = g_build_filename( g_get_user_config_dir(), "sakura", NULL );
	if( ! g_file_test( g_get_user_config_dir(), G_FILE_TEST_EXISTS) )
		g_mkdir( g_get_user_config_dir(), 0755 );
	if( ! g_file_test( configdir, G_FILE_TEST_EXISTS) )
		g_mkdir( configdir, 0755 );
	/* Use more standard-conforming path for config files, if available. */
	sakura.configfile=g_build_filename(configdir, CONFIGFILE, NULL);
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

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "forecolor", NULL)) {
		sakura_set_config_string("forecolor", "#c0c0c0");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "forecolor", NULL);
	gdk_color_parse(cfgtmp, &sakura.forecolor);
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "backcolor", NULL)) {
		sakura_set_config_string("backcolor", "#000000");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "backcolor", NULL);
	gdk_color_parse(cfgtmp, &sakura.backcolor);
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "backalpha", NULL)) {
		sakura_set_config_integer("backalpha", 65535);
	}
	sakura.backalpha = g_key_file_get_integer(sakura.cfg, cfg_group, "backalpha", NULL);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "opacity_level", NULL)) {
		sakura_set_config_string("opacity_level", "80");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "opacity_level", NULL);
	sakura.opacity_level_percent=cfgtmp;
	sakura.opacity_level=( ( 100 - (atof(cfgtmp)) ) / 100 );
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "fake_transparency", NULL)) {
		sakura_set_config_string("fake_transparency", "No");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "fake_transparency", NULL);
	if (strcmp(cfgtmp, "Yes")==0) {
		sakura.fake_transparency=1;
	} else {
		sakura.fake_transparency=0;
	}
	g_free(cfgtmp);


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

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "show_always_first_tab", NULL)) {
		sakura_set_config_string("show_always_first_tab", "No");
	}

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "scrollbar", NULL)) {
		sakura_set_config_boolean("scrollbar", FALSE);
	}
	sakura.show_scrollbar = g_key_file_get_boolean(sakura.cfg, cfg_group, "scrollbar", NULL);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "closebutton", NULL)) {
		sakura_set_config_boolean("closebutton", FALSE);
	}
	sakura.show_closebutton = g_key_file_get_boolean(sakura.cfg, cfg_group, "closebutton", NULL);

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

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "borderless", NULL)) {
		sakura_set_config_string("borderless", "No");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "borderless", NULL);
	sakura.borderless= (strcmp(cfgtmp, "Yes")==0) ? 1 : 0;
	g_free(cfgtmp);

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "maximized", NULL)) {
		sakura_set_config_string("maximized", "No");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "maximized", NULL);
	sakura.maximized= (strcmp(cfgtmp, "Yes")==0) ? 1 : 0;
	g_free(cfgtmp);

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
	} else if (strcmp(cfgtmp, "tango")==0) {
		sakura.palette=tango_palette;
	} else if (strcmp(cfgtmp, "xterm")==0) {
		sakura.palette=xterm_palette;
	} else {
		sakura.palette=rxvt_palette;
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

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "new_window_key", NULL)) {
		sakura_set_config_key("new_window_key", DEFAULT_NEW_WINDOW_KEY);
	}
	sakura.new_window_key = sakura_get_config_key("new_window_key");

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

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "fullscreen_key", NULL)) {
		sakura_set_config_key("fullscreen_key", DEFAULT_FULLSCREEN_KEY);
	}
	sakura.fullscreen_key = sakura_get_config_key("fullscreen_key");

	/* Set dialog style */
	gtk_rc_parse_string ("style \"hig-dialog\" {\n"
	                     "GtkDialog::action-area-border = 12\n"
                         "GtkDialog::button-spacing = 12\n"
                         "}\n");

	sakura.main_window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(sakura.main_window), "sakura");
	gtk_window_set_icon_from_file(GTK_WINDOW(sakura.main_window), DATADIR "/pixmaps/" ICON_FILE, &gerror);
	/* Default terminal size*/
	sakura.columns = DEFAULT_COLUMNS;
	sakura.rows = DEFAULT_ROWS;

	sakura.notebook=gtk_notebook_new();
	//gtk_notebook_popup_enable(GTK_NOTEBOOK(sakura.notebook));

	/* Figure out if we have rgba capabilities. */
	GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (sakura.main_window));
	GdkColormap *colormap = gdk_screen_get_rgba_colormap(screen);
	if (colormap != NULL && gdk_screen_is_composited (screen)) {
		gtk_widget_set_colormap (GTK_WIDGET (sakura.main_window), colormap);
		/* Should probably set as false if WM looses capabilities */
		sakura.has_rgba = true;
	}
	else {
		/* Probably not needed, as is likely the default initializer */
		sakura.has_rgba = false;
	}

	/* Set argv for forked childs */
	if (option_login) {
		sakura.argv[0]=g_strdup_printf("-%s", g_getenv("SHELL"));
	} else {
		sakura.argv[0]=g_strdup(g_getenv("SHELL"));
	}
	sakura.argv[1]=NULL;

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
	} else {
        cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "font", NULL);
		sakura.font = pango_font_description_from_string(cfgtmp);
		free(cfgtmp);
	}

	sakura.label_count=1;
	sakura.full_screen=FALSE;
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
}


static void
sakura_init_popup()
{
	GtkWidget *item_new_tab, *item_set_name, *item_close_tab, *item_copy, *item_new_window,
	          *item_paste, *item_select_font, *item_select_colors,
	          *item_select_background, *item_set_title, *item_full_screen,
	          *item_toggle_scrollbar, *item_options, *item_input_methods,
	          *item_opacity_menu, *item_show_first_tab, *item_audible_bell, *item_visible_bell,
	          *item_blinking_cursor, *item_borderless_maximized,
	          *item_palette, *item_palette_tango, *item_palette_linux, *item_palette_xterm, *item_palette_rxvt,
	          *item_show_close_button;
	GtkAction *action_open_link, *action_copy_link, *action_new_tab, *action_set_name, *action_close_tab,
                  *action_new_window,
	          *action_copy, *action_paste, *action_select_font, *action_select_colors,
	          *action_select_background, *action_clear_background, *action_opacity, *action_set_title,
	          *action_full_screen;
	GtkWidget *options_menu, *palette_menu;

	/* Define actions */
	action_open_link=gtk_action_new("open_link", _("Open link..."), NULL, NULL);
	action_copy_link=gtk_action_new("copy_link", _("Copy link..."), NULL, NULL);
	action_new_tab=gtk_action_new("new_tab", _("New tab"), NULL, GTK_STOCK_NEW);
	action_new_window=gtk_action_new("new_window", _("New window"), NULL, NULL);
	action_set_name=gtk_action_new("set_name", _("Set name..."), NULL, NULL);
	action_close_tab=gtk_action_new("close_tab", _("Close tab"), NULL, GTK_STOCK_CLOSE);
	action_full_screen=gtk_action_new("full_screen", _("Full screen"), NULL, GTK_STOCK_FULLSCREEN);
	action_copy=gtk_action_new("copy", _("Copy"), NULL, GTK_STOCK_COPY);
	action_paste=gtk_action_new("paste", _("Paste"), NULL, GTK_STOCK_PASTE);
	action_select_font=gtk_action_new("select_font", _("Select font..."), NULL, GTK_STOCK_SELECT_FONT);
	action_select_colors=gtk_action_new("select_colors", _("Select colors..."), NULL, GTK_STOCK_SELECT_COLOR);
	action_select_background=gtk_action_new("select_background", _("Select background..."), NULL, NULL);
	action_clear_background=gtk_action_new("clear_background", _("Clear background"), NULL, NULL);
	action_opacity=gtk_action_new("set_opacity", _("Set opacity level..."), NULL, NULL);
	action_set_title=gtk_action_new("set_title", _("Set window title..."), NULL, NULL);

	/* Create menuitems */
	sakura.item_open_link=gtk_action_create_menu_item(action_open_link);
	sakura.item_copy_link=gtk_action_create_menu_item(action_copy_link);
	item_new_tab=gtk_action_create_menu_item(action_new_tab);
	item_set_name=gtk_action_create_menu_item(action_set_name);
	item_close_tab=gtk_action_create_menu_item(action_close_tab);
	item_new_window=gtk_action_create_menu_item(action_new_window);
	item_full_screen=gtk_action_create_menu_item(action_full_screen);
	item_copy=gtk_action_create_menu_item(action_copy);
	item_paste=gtk_action_create_menu_item(action_paste);
	item_select_font=gtk_action_create_menu_item(action_select_font);
	item_select_colors=gtk_action_create_menu_item(action_select_colors);
	item_select_background=gtk_action_create_menu_item(action_select_background);
	sakura.item_clear_background=gtk_action_create_menu_item(action_clear_background);
	item_opacity_menu=gtk_action_create_menu_item(action_opacity);
	item_set_title=gtk_action_create_menu_item(action_set_title);

	item_show_first_tab=gtk_check_menu_item_new_with_label(_("Always show tab bar"));
	item_show_close_button=gtk_check_menu_item_new_with_label(_("Show tab close button"));
	item_toggle_scrollbar=gtk_check_menu_item_new_with_label(_("Show scrollbar"));
	item_audible_bell=gtk_check_menu_item_new_with_label(_("Set audible bell"));
	item_visible_bell=gtk_check_menu_item_new_with_label(_("Set visible bell"));
	item_blinking_cursor=gtk_check_menu_item_new_with_label(_("Set blinking cursor"));
	item_borderless_maximized=gtk_check_menu_item_new_with_label(_("Borderless and maximized"));
	item_input_methods=gtk_menu_item_new_with_label(_("Input methods"));
	item_palette_tango=gtk_radio_menu_item_new_with_label(NULL, "Tango");
	item_palette_linux=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_palette_tango), "Linux");
	item_palette_xterm=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_palette_tango), "xterm");
	item_palette_rxvt=gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(item_palette_tango), "rxvt");
	item_options=gtk_menu_item_new_with_label(_("Options"));
	item_palette=gtk_menu_item_new_with_label(_("Set palette"));

	/* Show defaults in menu items */
	gchar *cfgtmp = NULL;

	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "show_always_first_tab", NULL);
	if (strcmp(cfgtmp, "Yes")==0) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_first_tab), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_first_tab), FALSE);
	}
	g_free(cfgtmp);

	if (sakura.show_closebutton) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_close_button), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_close_button), FALSE);
	}

	if (sakura.show_scrollbar) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_scrollbar), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_toggle_scrollbar), FALSE);
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

	if (sakura.borderless && sakura.maximized) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_borderless_maximized), TRUE);
	}

	cfgtmp = g_key_file_get_string(sakura.cfg, cfg_group, "palette", NULL);
	if (strcmp(cfgtmp, "linux")==0) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_palette_linux), TRUE);
	} else if (strcmp(cfgtmp, "tango")==0) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_palette_tango), TRUE);
	} else if (strcmp(cfgtmp, "xterm")==0) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_palette_xterm), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_palette_rxvt), TRUE);
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
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_new_window);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_full_screen);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_copy);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_paste);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_select_colors);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_select_font);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.item_clear_background);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_options);

	sakura.im_menu=gtk_menu_new();
	options_menu=gtk_menu_new();
	palette_menu=gtk_menu_new();

	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_show_first_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_show_close_button);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_toggle_scrollbar);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_audible_bell);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_visible_bell);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_blinking_cursor);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_borderless_maximized);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_set_title);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_opacity_menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_select_background);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_palette);
	gtk_menu_shell_append(GTK_MENU_SHELL(palette_menu), item_palette_tango);
	gtk_menu_shell_append(GTK_MENU_SHELL(palette_menu), item_palette_linux);
	gtk_menu_shell_append(GTK_MENU_SHELL(palette_menu), item_palette_xterm);
	gtk_menu_shell_append(GTK_MENU_SHELL(palette_menu), item_palette_rxvt);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_input_methods);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_input_methods), sakura.im_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_options), options_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_palette), palette_menu);

	//gtk_menu_shell_append(GTK_MENU_SHELL(sakura.labels_menu), item_label_new_tab);

	/* ... and finally assign callbacks to menuitems */
	g_signal_connect(G_OBJECT(action_new_tab), "activate", G_CALLBACK(sakura_new_tab), NULL);
	g_signal_connect(G_OBJECT(action_new_window), "activate", G_CALLBACK(sakura_new_window), NULL);
	g_signal_connect(G_OBJECT(action_set_name), "activate", G_CALLBACK(sakura_set_name_dialog), NULL);
	g_signal_connect(G_OBJECT(action_close_tab), "activate", G_CALLBACK(sakura_close_tab), NULL);
	g_signal_connect(G_OBJECT(action_select_font), "activate", G_CALLBACK(sakura_font_dialog), NULL);
	g_signal_connect(G_OBJECT(action_select_background), "activate",
                              G_CALLBACK(sakura_select_background_dialog), NULL);
	g_signal_connect(G_OBJECT(action_copy), "activate", G_CALLBACK(sakura_copy), NULL);
	g_signal_connect(G_OBJECT(action_paste), "activate", G_CALLBACK(sakura_paste), NULL);
	g_signal_connect(G_OBJECT(action_select_colors), "activate", G_CALLBACK(sakura_color_dialog), NULL);

	g_signal_connect(G_OBJECT(item_show_first_tab), "activate", G_CALLBACK(sakura_show_first_tab), NULL);
	g_signal_connect(G_OBJECT(item_show_close_button), "activate", G_CALLBACK(sakura_show_close_button), NULL);
	g_signal_connect(G_OBJECT(item_toggle_scrollbar), "activate", G_CALLBACK(sakura_show_scrollbar), NULL);
	g_signal_connect(G_OBJECT(item_audible_bell), "activate", G_CALLBACK(sakura_audible_bell), NULL);
	g_signal_connect(G_OBJECT(item_visible_bell), "activate", G_CALLBACK(sakura_visible_bell), NULL);
	g_signal_connect(G_OBJECT(item_blinking_cursor), "activate", G_CALLBACK(sakura_blinking_cursor), NULL);
	g_signal_connect(G_OBJECT(item_borderless_maximized), "activate", G_CALLBACK(sakura_borderless), NULL);
	g_signal_connect(G_OBJECT(item_borderless_maximized), "activate", G_CALLBACK(sakura_maximized), NULL);
	g_signal_connect(G_OBJECT(action_opacity), "activate", G_CALLBACK(sakura_opacity_dialog), NULL);
	g_signal_connect(G_OBJECT(action_set_title), "activate", G_CALLBACK(sakura_set_title_dialog), NULL);
	g_signal_connect(G_OBJECT(item_palette_tango), "activate", G_CALLBACK(sakura_set_palette), "tango");
	g_signal_connect(G_OBJECT(item_palette_linux), "activate", G_CALLBACK(sakura_set_palette), "linux");
	g_signal_connect(G_OBJECT(item_palette_xterm), "activate", G_CALLBACK(sakura_set_palette), "xterm");
	g_signal_connect(G_OBJECT(item_palette_rxvt), "activate", G_CALLBACK(sakura_set_palette), "rxvt");

	g_signal_connect(G_OBJECT(action_open_link), "activate", G_CALLBACK(sakura_open_url), NULL);
	g_signal_connect(G_OBJECT(action_copy_link), "activate", G_CALLBACK(sakura_copy_url), NULL);
	g_signal_connect(G_OBJECT(action_clear_background), "activate", G_CALLBACK(sakura_clear), NULL);
	g_signal_connect(G_OBJECT(action_full_screen), "activate", G_CALLBACK(sakura_full_screen), NULL);


	gtk_widget_show_all(sakura.menu);

	/* We don't want to see this if there's no background image */
	if (!sakura.background) {
		gtk_widget_hide(sakura.item_clear_background);
	}
}


static void
sakura_set_geometry_hints()
{
	struct terminal *term;
	GdkGeometry hints;
	gint pad_x, pad_y;
	gint char_width, char_height;

	term = sakura_get_page_term(sakura, 0);
	vte_terminal_get_padding(VTE_TERMINAL(term->vte), (int *)&pad_x, (int *)&pad_y);
	char_width = vte_terminal_get_char_width(VTE_TERMINAL(term->vte));
	char_height = vte_terminal_get_char_height(VTE_TERMINAL(term->vte));

	hints.min_width = char_width + pad_x;
	hints.min_height = char_height + pad_y;
	hints.base_width = pad_x;
	hints.base_height = pad_y;
	hints.width_inc = char_width;
	hints.height_inc = char_height;
	gtk_window_set_geometry_hints (GTK_WINDOW (sakura.main_window),
	                               GTK_WIDGET (term->vte),
	                               &hints,
	                               GDK_HINT_RESIZE_INC | GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);
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
sakura_set_size(gint columns, gint rows)
{
	struct terminal *term;
	GtkRequisition main_request;
	GtkRequisition term_request;
	gint pad_x, pad_y;
	gint char_width, char_height;
	GdkGeometry hints;

	term = sakura_get_page_term(sakura, 0);

	/* New values used to resize the window */
	//sakura.columns = columns;
	//sakura.rows = rows;

	vte_terminal_get_padding(VTE_TERMINAL(term->vte), (int *)&pad_x, (int *)&pad_y);
	gtk_widget_style_get(term->vte, "inner-border", &term->border, NULL);
	SAY("l%d r%d t%d b%d", term->border.left, term->border.right, term->border.top, term->border.bottom);
	char_width = vte_terminal_get_char_width(VTE_TERMINAL(term->vte));
	char_height = vte_terminal_get_char_height(VTE_TERMINAL(term->vte));

	hints.min_width = char_width + pad_x;
	hints.min_height = char_height + pad_y;
	hints.base_width = pad_x;
	hints.base_height = pad_y;
	hints.width_inc = char_width;
	hints.height_inc = char_height;
	gtk_window_set_geometry_hints (GTK_WINDOW (sakura.main_window),
	                               GTK_WIDGET (term->vte),
	                               &hints,
	                               GDK_HINT_RESIZE_INC | GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);

	gtk_widget_size_request (sakura.main_window, &main_request);
	gtk_widget_size_request (term->vte, &term_request);
	sakura.width = main_request.width - term_request.width;
	sakura.height = main_request.height - term_request.height;
	sakura.width += pad_x + char_width * sakura.columns;
	sakura.height += pad_y + char_height * sakura.rows;
	/* FIXME: Deprecated GTK_WIDGET_MAPPED. Replace it when gtk+-2.20 is widely used */
	if (GTK_WIDGET_MAPPED (sakura.main_window)) {
		gtk_window_resize (GTK_WINDOW (sakura.main_window), sakura.width, sakura.height);
		SAY("Resizing to %ld columns %ld rows", sakura.columns, sakura.rows);
	} else {
		gtk_window_set_default_size (GTK_WINDOW (sakura.main_window), sakura.width, sakura.height);
	}
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
sakura_add_tab()
{
	struct terminal *term;
	GtkWidget *tab_hbox;
	GtkWidget *close_btn;
	int index;
	gchar *cwd = NULL;


	term = g_new0( struct terminal, 1 );
	term->hbox=gtk_hbox_new(FALSE, 0);
	term->vte=vte_terminal_new();

	/* Create label (and optional close button) for tabs */
	term->label_text=g_strdup_printf(_("Terminal %d"), sakura.label_count++);
	term->label=gtk_label_new(term->label_text);
	tab_hbox=gtk_hbox_new(FALSE,2);
	gtk_box_pack_start(GTK_BOX(tab_hbox), term->label, FALSE, FALSE, 0);

	if (sakura.show_closebutton) {
		close_btn=gtk_button_new();
		gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
		GtkWidget *image=gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
		gtk_container_add (GTK_CONTAINER (close_btn), image);
		/* FIXME: Use GtkWidget set-style signal to properly reflect the changes */
		gtk_box_pack_start(GTK_BOX(tab_hbox), close_btn, FALSE, FALSE, 0);
	}

	gtk_widget_show_all(tab_hbox);

	/* Init vte */
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(term->vte), SCROLL_LINES);
	vte_terminal_match_add_gregex(VTE_TERMINAL(term->vte), sakura.http_regexp, 0);
	vte_terminal_set_mouse_autohide(VTE_TERMINAL(term->vte), TRUE);

	term->scrollbar=gtk_vscrollbar_new(vte_terminal_get_adjustment(VTE_TERMINAL(term->vte)));

	gtk_box_pack_start(GTK_BOX(term->hbox), term->vte, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(term->hbox), term->scrollbar, FALSE, FALSE, 0);

	/* Select the directory to use for the new tab */
	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	if(index >= 0) {
		struct terminal *prev_term;
		prev_term = sakura_get_page_term( sakura, index );
		cwd = sakura_get_term_cwd( prev_term );
	}
	if (!cwd)
		cwd = g_get_current_dir();

	/* Keep values when adding tabs */
	sakura.keep_fc=true;

	if ((index=gtk_notebook_append_page(GTK_NOTEBOOK(sakura.notebook), term->hbox, tab_hbox))==-1) {
		sakura_error("Cannot create a new tab");
		return;
	}

#if GTK_CHECK_VERSION( 2, 10, 0 )
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(sakura.notebook), term->hbox, TRUE);
	// TODO: Set group id to support detached tabs
#if 0
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(sakura.notebook), term->hbox, TRUE);
#endif
#endif

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
		g_signal_connect(G_OBJECT(close_btn), "clicked", G_CALLBACK(sakura_closebutton_clicked), term->hbox);
	}

	/* First tab */
	if ( gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 1) {
		gchar *cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "show_always_first_tab", NULL);
		if (strcmp(cfgtmp, "Yes")==0) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		} else {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}
		g_free(cfgtmp);

		gtk_notebook_set_show_border(GTK_NOTEBOOK(sakura.notebook), FALSE);
		sakura_set_font();

        gtk_widget_show_all(sakura.notebook);
        if (!sakura.show_scrollbar) {
            gtk_widget_hide(term->scrollbar);
        }
        sakura_set_geometry_hints();
		if (option_geometry) {
			if (!gtk_window_parse_geometry(GTK_WINDOW(sakura.main_window), option_geometry)) {
				fprintf(stderr, "Invalid geometry.\n");
				gtk_widget_show(sakura.main_window);
			} else {
				gtk_widget_show(sakura.main_window);
				sakura.columns = VTE_TERMINAL(term->vte)->column_count;
				sakura.rows = VTE_TERMINAL(term->vte)->row_count;
			}
		} else {
            gtk_widget_show(sakura.main_window);
		}
		sakura_set_size(sakura.columns, sakura.rows);

		if (option_execute||option_xterm_execute) {
			int command_argc; char **command_argv;
			GError *gerror;
			gchar *path;

			if(option_execute) {
				if (!g_shell_parse_argv(option_execute, &command_argc, &command_argv, &gerror)) {
					sakura_error("Cannot parse command line arguments");
					exit(1);
				}
			} else {
				gchar *command_joined;
				/* the xterm -e command takes all extra arguments */
				command_joined = g_strjoinv(" ", option_xterm_args);
				if (!g_shell_parse_argv(command_joined, &command_argc, &command_argv, &gerror)) {
					sakura_error("Cannot parse command line arguments");
					exit(1);
				}
				g_free(command_joined);
			}

			/* Check if the command is valid */
			path=g_find_program_in_path(command_argv[0]);
			if (path) {
				free(path);
			} else {
				option_execute=NULL;
				g_strfreev(option_xterm_args);
				option_xterm_args=NULL;
			}

			vte_terminal_fork_command_full(VTE_TERMINAL(term->vte), VTE_PTY_DEFAULT, NULL, command_argv, NULL, 
										   G_SPAWN_SEARCH_PATH, NULL, NULL, &term->pid, NULL);
			g_strfreev(command_argv);
			option_execute=NULL;
			g_strfreev(option_xterm_args);
			option_xterm_args=NULL;
		} else { /* No execute option */
			if (option_hold==TRUE) {
				sakura_error("Hold option given without any command");
				option_hold=FALSE;
			}
			/* TODO: Check the new command_full works ok with login shells */
			vte_terminal_fork_command_full(VTE_TERMINAL(term->vte), VTE_PTY_DEFAULT, cwd, sakura.argv, NULL,
										   G_SPAWN_SEARCH_PATH, NULL, NULL, &term->pid, NULL);
		}
	/* Not the first tab */
	} else {

		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		sakura_set_font();
		gtk_widget_show_all(term->hbox);
		if (!sakura.show_scrollbar) {
			gtk_widget_hide(term->scrollbar);
		}
		sakura_set_size(sakura.columns, sakura.rows);
		/* Call set_current page after showing the widget: gtk ignores this
		 * function in the window is not visible *sigh*. Gtk documentation
		 * says this is for "historical" reasons. Me arse */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), index);
		vte_terminal_fork_command_full(VTE_TERMINAL(term->vte), VTE_PTY_DEFAULT, cwd, sakura.argv, NULL,
									   G_SPAWN_SEARCH_PATH, NULL, NULL, &term->pid, NULL);
	}

	free(cwd);

	/* Configuration per-terminal */
	if (sakura.has_rgba) {
		vte_terminal_set_color_background(VTE_TERMINAL (term->vte), &sakura.forecolor);
		vte_terminal_set_opacity(VTE_TERMINAL (term->vte), sakura.backalpha);
	}
	vte_terminal_set_backspace_binding(VTE_TERMINAL(term->vte), VTE_ERASE_ASCII_DELETE);
	vte_terminal_set_colors(VTE_TERMINAL(term->vte), &sakura.forecolor, &sakura.backcolor,
	                        sakura.palette, PALETTE_SIZE);

	if (sakura.fake_transparency) {
		vte_terminal_set_background_saturation(VTE_TERMINAL (term->vte), sakura.opacity_level);
		vte_terminal_set_background_transparent(VTE_TERMINAL (term->vte),TRUE);
		vte_terminal_set_background_tint_color(VTE_TERMINAL(term->vte), &sakura.backcolor);
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

	/* Apply user defined window configuration */
	gtk_window_set_decorated (GTK_WINDOW(sakura.main_window), sakura.borderless ? FALSE : TRUE);
	if (sakura.maximized) {
		gtk_window_maximize (GTK_WINDOW(sakura.main_window));
	}

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
	term = sakura_get_page_term(sakura, page);

	/* Do the first tab checks BEFORE deleting the tab, to ensure correct
	 * sizes are calculated when the tab is deleted */
	if ( gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 2) {
        char *cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "show_always_first_tab", NULL);
		if (strcmp(cfgtmp, "Yes")==0) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		} else {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}
		g_free(cfgtmp);
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
sakura_kill_child()
{
	/* TODO: Kill the forked child nicely */
}


static void
sakura_set_bgimage(char *infile)
{
	GError *gerror=NULL;
	GdkPixbuf *pixbuf=NULL;
	int page;
	struct terminal *term;

	if (!infile) SAY("File parameter is NULL");

	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term(sakura, page);

	/* Check file existence and type */
	if (g_file_test(infile, G_FILE_TEST_IS_REGULAR)) {

		pixbuf = gdk_pixbuf_new_from_file (infile, &gerror);
		if (!pixbuf) {
			sakura_error("Not using image file, %s\n", gerror->message);
		} else {
            vte_terminal_set_background_image(VTE_TERMINAL(term->vte), pixbuf);
            vte_terminal_set_background_saturation(VTE_TERMINAL(term->vte), TRUE);
            vte_terminal_set_background_transparent(VTE_TERMINAL(term->vte),FALSE);

			sakura_set_config_string("background", infile);
		}
	}
}


static void
sakura_set_config_key(const gchar *key, guint value) {
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
	guint retval=GDK_VoidSymbol;

	value=g_key_file_get_string(sakura.cfg, cfg_group, key, NULL);
	if (value!=NULL){
		retval=gdk_keyval_from_name(value);
		g_free(value);
	}

	/* For backwards compatibility with integer values */
	/* If gdk_keyval_from_name fail, it seems to be integer value*/
	if ((retval==GDK_VoidSymbol)||(retval==0)) {
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
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	free(buff);
}


int
main(int argc, char **argv)
{
	struct terminal *term;
	gchar *localedir;
	GError *error=NULL;
	GOptionContext *context;
	int i;
	int n;
	char **nargv;
	int nargc;

	/* Localization */
	setlocale(LC_ALL, "");
	localedir=g_strdup_printf("%s/locale", DATADIR);
	textdomain(GETTEXT_PACKAGE);
	bindtextdomain(GETTEXT_PACKAGE, localedir);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* Rewrites argv to include a -- after the -e argument this is required to make
	 * sure GOption doesn't grab any arguments meant for the command being called */

	/* Initialize nargv */
	nargv = (char**)calloc((argc+1), sizeof(char*));
   	n=0; nargc=argc;

	for(i=0; i<argc; i++) {
		if(g_strcmp0(argv[i],"-e") == 0)
		{
			nargv[n]="-e";
			n++;
			nargv[n]="--";
			nargc = argc+1;
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
	g_option_context_parse (context, &nargc, &nargv, &error);

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

	/* Fill Input Methods menu */
	term = sakura_get_page_term(sakura, 0);
	vte_terminal_im_append_menuitems(VTE_TERMINAL(term->vte), GTK_MENU_SHELL(sakura.im_menu));

	gtk_main();

	return 0;
}
