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

#ifndef NDEBUG
#define SAY(format,...) do {\
    fwide(stderr, -1);\
    fprintf(stderr, "[%d] ", getpid());\
    if (format) fprintf(stderr, format, ##__VA_ARGS__);\
    fputc('\n', stderr);\
    fflush(stderr);\
} while (0)
#else
#define SAY(format,...) do {} while (0)
#endif

static struct {
	GtkWidget *main_window;
	GtkWidget *notebook;
	GtkWidget *menu;
	GtkWidget *im_menu;
	PangoFontDescription *font;
	GdkColor forecolor;
	GdkColor backcolor;
	GdkColor boldcolor;
	char *current_match;
	guint width;
	guint height;
	struct {
		glong columns;
		glong rows;
		gint char_width;
		gint char_height;
		gboolean init;
	} term_info;
	gint label_count;
	bool fake_transparency;
	float opacity_level;
	char *opacity_level_percent;
	bool *opacity;
	bool first_tab;
	bool show_scrollbar;
	GtkWidget *item_clear_background; /* We include here only the items which need to be hided */
    GtkWidget *item_copy_link;
	GtkWidget *item_open_link;
	GtkWidget *open_link_separator;
	GKeyFile *cfg;
	char *configfile;
	char *background;
    char *word_chars;
	char *argv[2];
} sakura;

struct terminal {
	GtkWidget *hbox;
	GtkWidget *vte;     /* Reference to VTE terminal */
	pid_t pid;          /* pid of the forked proccess */
	GtkWidget *scrollbar;
	GtkWidget *label;
};

#define ICON_DIR "/usr/share/pixmaps"
#define SCROLL_LINES 4096
#define HTTP_REGEXP "((f|F)(t|T)(p|P)|((h|H)(t|T)(t|T)(p|P)(s|S)*))://[-a-zA-Z0-9.?$%&/=_~#.,:;+]*"
#define CONFIGFILE "sakura.conf"
#define DEFAULT_COLUMNS 80
#define DEFAULT_ROWS 24
#define DEFAULT_FONT "monospace 11"
#define DEFAULT_WORD_CHARS  "-A-Za-z0-9,./?%&#_~"
const char cfg_group[] = "sakura";

static GQuark term_data_id = 0;
#define  sakura_get_page_term( sakura, page_idx )  \
    (struct terminal*)g_object_get_qdata(  \
            G_OBJECT( gtk_notebook_get_nth_page( (GtkNotebook*)sakura.notebook, page_idx ) ), term_data_id);

#define  sakura_set_page_term( sakura, page_idx, term )  \
    g_object_set_qdata_full( \
            G_OBJECT( gtk_notebook_get_nth_page( (GtkNotebook*)sakura.notebook, page_idx) ), \
            term_data_id, term, (GDestroyNotify)g_free);

/* Callbacks */
static gboolean sakura_key_press (GtkWidget *, GdkEventKey *, gpointer);
static void     sakura_increase_font (GtkWidget *, void *);
static void     sakura_decrease_font (GtkWidget *, void *);
static void     sakura_child_exited (GtkWidget *, void *);
static void     sakura_eof (GtkWidget *, void *);
static void     sakura_title_changed (GtkWidget *, void *);
static gboolean sakura_delete_window (GtkWidget *, void *);
static void     sakura_destroy_window (GtkWidget *, void *);
static gboolean sakura_popup (GtkWidget *, GdkEvent *);
static void     sakura_font_dialog (GtkWidget *, void *);
static void     sakura_set_name_dialog (GtkWidget *, void *);
static void     sakura_color_dialog (GtkWidget *, void *);
static void     sakura_new_tab (GtkWidget *, void *);
static void     sakura_close_tab (GtkWidget *, void *);
static void     sakura_background_selection (GtkWidget *, void *);
static void     sakura_open_url (GtkWidget *, void *);
static void     sakura_clear (GtkWidget *, void *);
static void     sakura_set_opacity (GtkWidget *, void *);
static gboolean sakura_resized_window(GtkWidget *, GdkEventConfigure *, void *);
static void     sakura_setname_entry_changed(GtkWidget *, void *);
static void     sakura_copy(GtkWidget *, void *);
static void     sakura_paste(GtkWidget *, void *);

/* Misc */
static void     sakura_error(const char *, ...);

/* Functions */	
static void     sakura_init();
static void     sakura_init_popup();
static void     sakura_destroy();
static void     sakura_add_tab();
static void     sakura_del_tab();
static void     sakura_set_font();
static void     sakura_kill_child();
static void     sakura_set_bgimage();

static const char *option_font;
static const char *option_execute;
static gboolean option_version=FALSE;
static gboolean option_show_scrollbar=TRUE;
static gint option_ntabs=1;
static gint option_login = FALSE;
static const char *option_title;
static int option_rows, option_columns;

static GOptionEntry entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &option_version, N_("Print version number"), NULL },
	{ "scrollbar", 's', 0, G_OPTION_ARG_NONE, &option_show_scrollbar, N_("Show scrollbar"), NULL },
	{ "font", 'f', 0, G_OPTION_ARG_STRING, &option_font, N_("Select initial terminal font"), NULL },
	{ "ntabs", 'n', 0, G_OPTION_ARG_INT, &option_ntabs, N_("Select initial number of tabs"), NULL },
	{ "execute", 'e', 0, G_OPTION_ARG_STRING, &option_execute, N_("Execute command"), NULL },
	{ "login", 'l', 0, G_OPTION_ARG_NONE, &option_login, N_("Login shell"), NULL },
	{ "title", 't', 0, G_OPTION_ARG_STRING, &option_title, N_("Set window title"), NULL },
	{ "columns", 'c', 0, G_OPTION_ARG_INT, &option_columns, N_("Set columns number"), NULL },
	{ "rows", 'r', 0, G_OPTION_ARG_INT, &option_rows, N_("Set rows number"), NULL },
    { NULL }
};


static
gboolean sakura_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	unsigned int topage=0;
	gint npages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	if (event->type!=GDK_KEY_PRESS) return FALSE;
	
	/* Ctrl-Shift-[T/W] pressed */
	if ( (event->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK))==(GDK_CONTROL_MASK|GDK_SHIFT_MASK) ) { 
		if (event->keyval==GDK_t || event->keyval==GDK_T) {
			sakura_add_tab();
			return TRUE;
		} else if (event->keyval==GDK_w || event->keyval==GDK_W) {
			sakura_kill_child();
			sakura_del_tab();
			if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
				sakura_destroy();
			return TRUE;
		}
	}
	
	/* Alt + number pressed / Alt+ Left-Right cursor */
	if ( (event->state & GDK_MOD1_MASK) == GDK_MOD1_MASK ) {
		if ((event->keyval>=GDK_1) && (event->keyval<=GDK_9)) {
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
			if (topage <= gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))) 
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), topage);
			return TRUE;
		} else if (event->keyval==GDK_Left) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==0) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), npages-1);
			} else {
				gtk_notebook_prev_page(GTK_NOTEBOOK(sakura.notebook));
			}
			return TRUE;
		} else if (event->keyval==GDK_Right) {
			if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==(npages-1)) {
				gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), 0);
			} else {
				gtk_notebook_next_page(GTK_NOTEBOOK(sakura.notebook));
			}
			return TRUE;
		}
	}

	/* Ctrl-Shift-[C/V] pressed */
	if ( (event->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK))==(GDK_CONTROL_MASK|GDK_SHIFT_MASK) ) { 
		if (event->keyval==GDK_c || event->keyval==GDK_C) {
			sakura_copy(NULL, NULL);
			return TRUE;
		} else if (event->keyval==GDK_v || event->keyval==GDK_V) {
			sakura_paste(NULL, NULL);
			return TRUE;
		}
	}

	return FALSE;
}


static void
sakura_increase_font (GtkWidget *widget, void *data)
{
	gint size;
	
	size=pango_font_description_get_size(sakura.font);
	pango_font_description_set_size(sakura.font, ((size/PANGO_SCALE)+1) * PANGO_SCALE);
	
	sakura_set_font();
}


static void
sakura_decrease_font (GtkWidget *widget, void *data)
{
	gint size;
	
	size=pango_font_description_get_size(sakura.font);
	pango_font_description_set_size(sakura.font, ((size/PANGO_SCALE)-1) * PANGO_SCALE);
	
	sakura_set_font();
}


static void
sakura_child_exited (GtkWidget *widget, void *data)
{
	int status, page;
    struct terminal *term;
		
	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );

    SAY("waiting for terminal pid %d", term->pid);
	
    waitpid(term->pid, &status, WNOHANG);
	/* TODO: check wait return */	

	sakura_del_tab();
	
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
		sakura_destroy();

}


static void
sakura_eof (GtkWidget *widget, void *data)
{
	int status, page;
    struct terminal *term;
	
	SAY("Got EOF signal");

	/* Workaround for libvte strange behaviour. There is not child-exited signal for
	   the last terminal, so we need to kill it here.  Check with libvte authors about
	   child-exited/eof signals */
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==0) {
		
		page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
        term = sakura_get_page_term( sakura, page );

        SAY("waiting for terminal pid (in eof) %d", term->pid);
		
        waitpid(term->pid, &status, WNOHANG);
		/* TODO: check wait return */	

		sakura_del_tab();
		
		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
			sakura_destroy();
	}
}
	

static void
sakura_title_changed (GtkWidget *widget, void *data)
{
	int page;
    struct terminal *term;
	
	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );
	
	gtk_window_set_title(GTK_WINDOW(sakura.main_window),
                         vte_terminal_get_window_title(VTE_TERMINAL(term->vte)));

}


static gboolean
sakura_delete_window (GtkWidget *widget, void *data)
{
	GtkWidget *dialog;
	guint response;

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))>1) {
		dialog=gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
									  GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
									  _("There are several tabs opened. Do you really want to close Sakura?"));
		
		response=gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		if (response==GTK_RESPONSE_YES) 
			return FALSE;
		else {
			return TRUE;
		}
	}

	return FALSE;
}


static void
sakura_destroy_window (GtkWidget *widget, void *data)
{
	sakura_destroy();
}


static gboolean
sakura_popup (GtkWidget *widget, GdkEvent *event)
{
	GtkMenu *menu;
	GdkEventButton *event_button;
    struct terminal *term;
	glong column, row;
	int page, tag;

	menu = GTK_MENU (widget);

	if (event->type == GDK_BUTTON_PRESS) {
		
		event_button = (GdkEventButton *) event;
		
		page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
		SAY("Current page is %d",page);
        term = sakura_get_page_term( sakura, page );
		
		/* Find out if cursor it's over a matched expression...*/

		/* Get the column and row relative to pointer position */
        column=((glong)(event_button->x) / vte_terminal_get_char_width(VTE_TERMINAL(term->vte)));
        row=((glong)(event_button->y) / vte_terminal_get_char_height(VTE_TERMINAL(term->vte)));
        sakura.current_match=vte_terminal_match_check(VTE_TERMINAL(term->vte), column, row, &tag);

		SAY("current match is %s",sakura.current_match);

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
		
		if (event_button->button == 3) {
			gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 
			 			    event_button->button, event_button->time);
		   return TRUE;
		}
	}

	return FALSE;
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
        g_key_file_set_value(sakura.cfg, cfg_group, "font", pango_font_description_to_string(sakura.font));
	}

	gtk_widget_destroy(font_dialog);
}


static void
sakura_set_name_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *input_dialog;
	GtkWidget *entry;
	gint response;
	int page;
    struct terminal *term;
	
	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );

	input_dialog=gtk_dialog_new_with_buttons(_("Set name"), GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
											 GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
										     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	
	gtk_dialog_set_default_response(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(input_dialog), TRUE);

	entry=gtk_entry_new();
	/* Set tab label as entry default text */
    gtk_entry_set_text(GTK_ENTRY(entry), gtk_notebook_get_tab_label_text(GTK_NOTEBOOK(sakura.notebook), term->hbox));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(input_dialog)->vbox), entry, FALSE, FALSE, 10);
	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(sakura_setname_entry_changed), input_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT, FALSE);

	gtk_widget_show(entry);

	response=gtk_dialog_run(GTK_DIALOG(input_dialog));
	if (response==GTK_RESPONSE_ACCEPT) {
		gtk_notebook_set_tab_label_text(GTK_NOTEBOOK(sakura.notebook),
                                        term->hbox, gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	gtk_widget_destroy(input_dialog);
}


static void
sakura_color_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *color_dialog;
	GtkWidget *label1, *label2;
	GtkWidget *buttonfore, *buttonback;
	GtkWidget *vbox, *hbox_fore, *hbox_back;
	gint response;
	int page;
    struct terminal *term;

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );

	color_dialog=gtk_dialog_new_with_buttons(_("Select color"), GTK_WINDOW(sakura.main_window),
												GTK_DIALOG_MODAL,
												GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
												GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(color_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(color_dialog), TRUE);

	vbox=gtk_vbox_new(FALSE, 0);
	hbox_fore=gtk_hbox_new(FALSE, 10);
	hbox_back=gtk_hbox_new(FALSE, 10);
	label1=gtk_label_new(_("Select foreground color:"));
	label2=gtk_label_new(_("Select background color:"));
	buttonfore=gtk_color_button_new_with_color(&sakura.forecolor);
	buttonback=gtk_color_button_new_with_color(&sakura.backcolor);

	gtk_container_add(GTK_CONTAINER(hbox_fore), label1);
	gtk_container_add(GTK_CONTAINER(hbox_fore), buttonfore);
	gtk_container_add(GTK_CONTAINER(hbox_back), label2);
	gtk_container_add(GTK_CONTAINER(hbox_back), buttonback);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_fore, FALSE, FALSE, 10);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_back, FALSE, FALSE, 10);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(color_dialog)->vbox), vbox, FALSE, FALSE, 10);

	gtk_widget_show_all(vbox);

	response=gtk_dialog_run(GTK_DIALOG(color_dialog));

	if (response==GTK_RESPONSE_ACCEPT) {
		gtk_color_button_get_color(GTK_COLOR_BUTTON(buttonfore), &sakura.forecolor);
		gtk_color_button_get_color(GTK_COLOR_BUTTON(buttonback), &sakura.backcolor);
        vte_terminal_set_color_foreground(VTE_TERMINAL(term->vte), &sakura.forecolor);
		sakura.boldcolor=sakura.forecolor;
        vte_terminal_set_color_bold(VTE_TERMINAL(term->vte), &sakura.boldcolor);
        vte_terminal_set_color_background(VTE_TERMINAL(term->vte), &sakura.backcolor);

		gchar *cfgtmp;
		cfgtmp = g_strdup_printf("#%02x%02x%02x", sakura.forecolor.red >>8,
		                         sakura.forecolor.green>>8, sakura.forecolor.blue>>8);
        g_key_file_set_value(sakura.cfg, cfg_group, "forecolor", cfgtmp);
		g_free(cfgtmp);

		cfgtmp = g_strdup_printf("#%02x%02x%02x", sakura.backcolor.red >>8,
		                         sakura.backcolor.green>>8, sakura.backcolor.blue>>8);
        g_key_file_set_value(sakura.cfg, cfg_group, "backcolor", cfgtmp);
		g_free(cfgtmp);

		cfgtmp = g_strdup_printf("#%02x%02x%02x", sakura.boldcolor.red >>8,
		                         sakura.boldcolor.green>>8, sakura.boldcolor.blue>>8);
        g_key_file_set_value(sakura.cfg, cfg_group, "boldcolor", cfgtmp);
		g_free(cfgtmp);
	}

	gtk_widget_destroy(color_dialog);
}


static void
sakura_background_selection (GtkWidget *widget, void *data)
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
    clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    gtk_clipboard_set_text( clip, sakura.current_match, -1 );
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
		if( browser = g_find_program_in_path("xdg-open") ) {
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

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term = sakura_get_page_term( sakura, page );

	gtk_widget_hide(sakura.item_clear_background);

	vte_terminal_set_background_image(VTE_TERMINAL(term->vte), NULL);

	// FIXME: is this really needed? IMHO, this should be done just before
	// dumping the config to the config file.
	g_key_file_set_value(sakura.cfg, cfg_group, "background", "none");

	g_free(sakura.background);
	sakura.background=NULL;
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


static void
sakura_set_opacity (GtkWidget *widget, void *data)
{
	GtkWidget *input_dialog, *spin_control, *label, *check;
	GtkObject *spinner_adj;
	gint response;
	int page;
    struct terminal *term;

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );

	input_dialog=gtk_dialog_new_with_buttons(_("Opacity"), GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
			GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(input_dialog), TRUE);

	spinner_adj = gtk_adjustment_new (((1.0 - sakura.opacity_level) * 100), 0.0, 99.0, 1.0, 5.0, 5.0);
	spin_control = gtk_spin_button_new(GTK_ADJUSTMENT(spinner_adj), 1.0, 0);

	label = gtk_label_new(_("Opacity level (%):"));

	check = gtk_check_button_new_with_label(_("Disable opacity"));

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(input_dialog)->vbox), check, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(input_dialog)->vbox), label, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(input_dialog)->vbox), spin_control, FALSE, FALSE, 3);

	g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(sakura_opacity_check), spin_control);	

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), !sakura.fake_transparency);

	gtk_widget_show(check);
	gtk_widget_show(label);
	gtk_widget_show(spin_control);

	response=gtk_dialog_run(GTK_DIALOG(input_dialog));
	if (response==GTK_RESPONSE_ACCEPT) {
		char *value;

		value=g_strdup_printf("%d", gtk_spin_button_get_value_as_int((GtkSpinButton *) spin_control));
		sakura.opacity_level = ( ( 100 - (atof(value)) ) / 100 );
		sakura.opacity_level_percent = value;
		sakura.fake_transparency=!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check));

		if (sakura.fake_transparency) {
            SAY("setting term->pid: %d to transparent...", term->pid);
            vte_terminal_set_background_transparent(VTE_TERMINAL(term->vte), TRUE);
            vte_terminal_set_background_saturation(VTE_TERMINAL(term->vte), sakura.opacity_level);
			sakura.fake_transparency = TRUE;
            g_key_file_set_value(sakura.cfg, cfg_group, "fake_transparency", "Yes");
		} else {
            vte_terminal_set_background_transparent(VTE_TERMINAL(term->vte), FALSE);
			sakura.fake_transparency = FALSE;
            g_key_file_set_value(sakura.cfg, cfg_group, "fake_transparency", "No");
		}
        g_key_file_set_value(sakura.cfg, cfg_group, "opacity_level", sakura.opacity_level_percent);
	}

	gtk_widget_destroy(input_dialog);
}


static void
sakura_show_first_tab (GtkWidget *widget, void *data)
{
	int page;
    struct terminal *term;

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
        g_key_file_set_value(sakura.cfg, cfg_group, "show_always_first_tab", "Yes");
	} else {
		/* Only hide tabs if the notebook has one page */
		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 1) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}
        g_key_file_set_value(sakura.cfg, cfg_group, "show_always_first_tab", "No");
	}
}


static void
sakura_set_title_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *input_dialog;
	GtkWidget *entry;
	gint response;
	int page;
	struct terminal *term;

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term=sakura_get_page_term(sakura, page);

	input_dialog=gtk_dialog_new_with_buttons(_("Set title"), GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
			GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_modal(GTK_WINDOW(input_dialog), TRUE);

	entry=gtk_entry_new();
	/* Set window label as entry default text */
	gtk_entry_set_text(GTK_ENTRY(entry), gtk_window_get_title(GTK_WINDOW(sakura.main_window)));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(input_dialog)->vbox), entry, FALSE, FALSE, 10);
	/* Disable accept button until some text is entered */
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(sakura_setname_entry_changed), input_dialog);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT, FALSE);

	gtk_widget_show(entry);

	response=gtk_dialog_run(GTK_DIALOG(input_dialog));
	if (response==GTK_RESPONSE_ACCEPT) {
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	gtk_widget_destroy(input_dialog);

}

static void
sakura_get_term_row_col (gint width, gint height)
{
    struct terminal *term;
	gint x_adjust, y_adjust;

    term = sakura_get_page_term( sakura, 0 );

	/* This is to prevent a race with ConfigureEvents when the window is being destroyed */
    if (!VTE_IS_TERMINAL(term->vte)) return;

    vte_terminal_get_padding( VTE_TERMINAL(term->vte), &x_adjust, &y_adjust );
    sakura.term_info.char_width = vte_terminal_get_char_width(VTE_TERMINAL(term->vte));
    sakura.term_info.char_height = vte_terminal_get_char_height(VTE_TERMINAL(term->vte));
    x_adjust += sakura.main_window->allocation.width - term->vte->allocation.width;
	sakura.term_info.columns = (width - x_adjust) / sakura.term_info.char_width;
    y_adjust += sakura.main_window->allocation.height - term->vte->allocation.height;
	sakura.term_info.rows = (height - y_adjust) / sakura.term_info.char_height;
}


static gboolean
sakura_resized_window (GtkWidget *widget, GdkEventConfigure *event, void *data)
{
	if (event->width!=sakura.width || event->height!=sakura.height) {
		/* User has resized the application */
		sakura_get_term_row_col (event->width, event->height);
	}

	return FALSE;
}


static void
sakura_setname_entry_changed (GtkWidget *widget, void *data)
{
	GtkDialog *input_dialog=(GtkDialog *)data;

	if (strcmp(gtk_entry_get_text(GTK_ENTRY(widget)), "")==0) {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT, FALSE);
	} else {
		gtk_dialog_set_response_sensitive(GTK_DIALOG(input_dialog), GTK_RESPONSE_ACCEPT, TRUE);
	}
}


/* Parameters are never used */
static void
sakura_copy (GtkWidget *widget, void *data)
{
	int page;
    struct terminal *term;

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );

    vte_terminal_copy_primary(VTE_TERMINAL(term->vte));
    vte_terminal_copy_clipboard(VTE_TERMINAL(term->vte));
}


/* Parameters are never used */
static void
sakura_paste (GtkWidget *widget, void *data)
{
	int page;
    struct terminal *term;

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );

    vte_terminal_paste_primary(VTE_TERMINAL(term->vte));
}


static void
sakura_new_tab (GtkWidget *widget, void *data)
{
	sakura_add_tab();
}


#if (GTK_MAJOR_VERSION >= 2 && GTK_MINOR_VERSION >=10)
static void
sakura_reordered_tab (GtkWidget *widget, void *data)
{
	SAY("reorder-tab signal");
}

static void
sakura_page_reordered (GtkWidget *widget, void *data)
{
	SAY("page-reordered signal");
}

static void
sakura_move_focus_out (GtkWidget *widget, void *data)
{
	SAY("move-focus-out");
}

static void
sakura_focus_tab (GtkWidget *widget, void *data)
{
	SAY("focus-tab");
}
#endif

static void
sakura_close_tab (GtkWidget *widget, void *data)
{
	sakura_del_tab();

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==0)
		sakura_destroy();
}


/* Functions */

static void
sakura_init()
{
	GError *gerror=NULL;
	char* configdir = NULL;

	term_data_id = g_quark_from_static_string( "sakura_term" );

	/* Config file initialization*/
	sakura.cfg = g_key_file_new();

	configdir = g_build_filename( g_get_user_config_dir(), "sakura", NULL );
	if( ! g_file_test( g_get_user_config_dir(), G_FILE_TEST_EXISTS) )
		g_mkdir( g_get_user_config_dir(), 0755 );
	if( ! g_file_test( configdir, G_FILE_TEST_EXISTS) )
		g_mkdir( configdir, 0755 );
	/* Use more standard-conforming path for config files, if available. */
	sakura.configfile=g_build_filename( configdir, CONFIGFILE, NULL );
	g_free(configdir);

	if (!g_key_file_load_from_file(sakura.cfg, sakura.configfile, 0, &gerror)) {
		char *file_contents;
		char *new_file_contents;

		/* Workaround for cfgpool to g_key_file update. We update the config
		 * file here if needed. This support should be removed in future
		 * versions as everyone is supposed to be using a recent (no cfgpool)
		 * sakura release in the future */
		rename(sakura.configfile, "/tmp/sakura.cfg.old");
		g_file_get_contents("/tmp/sakura.cfg.old", &file_contents, NULL, NULL);
		new_file_contents=g_strconcat("[sakura]\n", file_contents, NULL);
		g_file_set_contents(sakura.configfile, new_file_contents, strlen(new_file_contents), NULL);
		g_free(file_contents); g_free(new_file_contents);
		unlink("/tmp/sakura.cfg.old");
		g_key_file_load_from_file(sakura.cfg, sakura.configfile, 0, &gerror);
	}


	/* Add default values if needed */
	gchar *cfgtmp = NULL;

	/* We can safely ignore errors from g_key_file_get_value(), since if the
	 * call to g_key_file_has_key() was successful, the key IS there. From the
	 * glib docs I don't know if we can ignore errors from g_key_file_has_key,
	 * too. I think we can: the only possible error is that the config file
	 * doesn't exist, but we have just read it!
	 */

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "forecolor", NULL)) {
		g_key_file_set_value(sakura.cfg, cfg_group, "forecolor", "#c0c0c0");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "forecolor", NULL);
	gdk_color_parse(cfgtmp, &sakura.forecolor);
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "backcolor", NULL)) {
		g_key_file_set_value(sakura.cfg, cfg_group, "backcolor", "#000000");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "backcolor", NULL);
	gdk_color_parse(cfgtmp, &sakura.backcolor);
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "boldcolor", NULL)) {
		g_key_file_set_value(sakura.cfg, cfg_group, "boldcolor", "#ffffff");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "boldcolor", NULL);
	gdk_color_parse(cfgtmp, &sakura.boldcolor);
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "opacity_level", NULL)) {
		g_key_file_set_value(sakura.cfg, cfg_group, "opacity_level", "80");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "opacity_level", NULL);
	sakura.opacity_level_percent=cfgtmp;
	sakura.opacity_level=( ( 100 - (atof(cfgtmp)) ) / 100 );
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "fake_transparency", NULL)) {
		g_key_file_set_value(sakura.cfg, cfg_group, "fake_transparency", "No");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "fake_transparency", NULL);
	if (strcmp(cfgtmp, "Yes")==0) {
		sakura.fake_transparency=1;
	} else {
		sakura.fake_transparency=0;
	}
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "background", NULL)) {
		g_key_file_set_value(sakura.cfg, cfg_group, "background", "none");
	}
	cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "background", NULL);
	if (strcmp(cfgtmp, "none")==0) {
		sakura.background=NULL;
	} else {
		sakura.background=g_strdup(cfgtmp);
	}
	g_free(cfgtmp);


	if (!g_key_file_has_key(sakura.cfg, cfg_group, "font", NULL)) {
		g_key_file_set_value(sakura.cfg, cfg_group, "font", DEFAULT_FONT);
	}

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "show_always_first_tab", NULL)) {
		g_key_file_set_value(sakura.cfg, cfg_group, "show_always_first_tab", "No");
	}

	if (!g_key_file_has_key(sakura.cfg, cfg_group, "word_chars", NULL)) {
		g_key_file_set_value(sakura.cfg, cfg_group, "word_chars", DEFAULT_WORD_CHARS);
	}
	sakura.word_chars = g_key_file_get_string(sakura.cfg, cfg_group, "word_chars", NULL);

	sakura.main_window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(sakura.main_window), "sakura");
	gtk_window_set_icon_from_file(GTK_WINDOW(sakura.main_window), ICON_DIR "/terminal-tango.png", &gerror);
	/* Minimum size*/
	sakura.term_info.columns = DEFAULT_COLUMNS;
	sakura.term_info.rows = DEFAULT_ROWS;
	sakura.term_info.init = FALSE;

	sakura.notebook=gtk_notebook_new();

	/* Set argv for forked childs */
	if (option_login) {
		sakura.argv[0]=g_strdup_printf("-%s", g_getenv("SHELL"));
	} else {
		sakura.argv[0]=g_strdup(g_getenv("SHELL"));
	}
	sakura.argv[1]=NULL;
	
	if (option_show_scrollbar) {
		sakura.show_scrollbar = option_show_scrollbar;
	}

	if (option_title) {
		gtk_window_set_title(GTK_WINDOW(sakura.main_window), option_title);
	}

	if (option_columns) {
		sakura.term_info.columns = option_columns;
	}

	if (option_rows) {
		sakura.term_info.rows = option_rows;
	}

	if (option_font) {
		sakura.font=pango_font_description_from_string(option_font);
	} else {
        cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "font", NULL);
		sakura.font = pango_font_description_from_string(cfgtmp);
		free(cfgtmp);
	}

	sakura.menu=gtk_menu_new();
	sakura.label_count=1;

	gtk_container_add(GTK_CONTAINER(sakura.main_window), sakura.notebook);

	/* Init notebook */
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(sakura.notebook), TRUE);

	sakura_init_popup();

	g_signal_connect(G_OBJECT(sakura.main_window), "delete_event", G_CALLBACK(sakura_delete_window), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "destroy", G_CALLBACK(sakura_destroy_window), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "key-press-event", G_CALLBACK(sakura_key_press), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "configure-event", G_CALLBACK(sakura_resized_window), NULL);
}


static void
sakura_init_popup()
{
	GtkWidget *item_new_tab, *item_set_name, *item_close_tab, *item_copy,
			  *item_paste, *item_select_font, *item_select_colors,
			  *item_select_background, *item_set_title;
	GtkWidget *item_options, *item_input_methods, *item_opacity_menu, *item_show_first_tab;
    GtkAction *action_open_link, *action_copy_link, *action_new_tab, *action_set_name, *action_close_tab,
			  *action_copy, *action_paste, *action_select_font, *action_select_colors,
			  *action_select_background, *action_clear_background, *action_opacity, *action_set_title;
	GtkWidget *separator, *separator2, *separator3, *separator4, *separator5;
	GtkWidget *options_menu;


	/* Define actions */
	action_open_link=gtk_action_new("open_link", _("Open link..."), NULL, NULL);
    action_copy_link=gtk_action_new("copy_link", _("Copy link..."), NULL, NULL);
	action_new_tab=gtk_action_new("new_tab", _("New tab"), NULL, GTK_STOCK_NEW);
	action_set_name=gtk_action_new("set_name", _("Set name..."), NULL, NULL);
	action_close_tab=gtk_action_new("close_tab", _("Close tab"), NULL, GTK_STOCK_CLOSE);
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
	item_copy=gtk_action_create_menu_item(action_copy);
	item_paste=gtk_action_create_menu_item(action_paste);
	item_select_font=gtk_action_create_menu_item(action_select_font);
	item_select_colors=gtk_action_create_menu_item(action_select_colors);
	item_select_background=gtk_action_create_menu_item(action_select_background);
	sakura.item_clear_background=gtk_action_create_menu_item(action_clear_background);
	item_opacity_menu=gtk_action_create_menu_item(action_opacity);
	item_set_title=gtk_action_create_menu_item(action_set_title);

	item_show_first_tab=gtk_check_menu_item_new_with_label(_("Show always first tab"));
	item_input_methods=gtk_menu_item_new_with_label(_("Input methods"));
	item_options=gtk_menu_item_new_with_label(_("Options"));

	/* Show defaults in menu items */
    gchar *cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "show_always_first_tab", NULL);
	if (strcmp(cfgtmp, "Yes")==0) {	
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_first_tab), TRUE);
	} else {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_show_first_tab), FALSE);
	}
	g_free(cfgtmp);

	sakura.open_link_separator=gtk_separator_menu_item_new();
	separator=gtk_separator_menu_item_new();
	separator2=gtk_separator_menu_item_new();
	separator3=gtk_separator_menu_item_new();
	separator4=gtk_separator_menu_item_new();
	separator5=gtk_separator_menu_item_new();

	/* Add items to popup menu */
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.item_open_link);
    gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.item_copy_link);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.open_link_separator);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_new_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_set_name);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), separator);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_close_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_copy);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_paste);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), separator3);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_select_colors);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_select_font);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_select_background);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.item_clear_background);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), separator4);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_options);		
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), separator5);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item_input_methods);		

	sakura.im_menu=gtk_menu_new();
	options_menu=gtk_menu_new();

	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_show_first_tab);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_opacity_menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), item_set_title);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_input_methods), sakura.im_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_options), options_menu);

	/* ... and finally assign callbacks to menuitems */
	g_signal_connect(G_OBJECT(action_new_tab), "activate", G_CALLBACK(sakura_new_tab), NULL);
	g_signal_connect(G_OBJECT(action_set_name), "activate", G_CALLBACK(sakura_set_name_dialog), NULL);
	g_signal_connect(G_OBJECT(action_close_tab), "activate", G_CALLBACK(sakura_close_tab), NULL);
	g_signal_connect(G_OBJECT(action_select_font), "activate", G_CALLBACK(sakura_font_dialog), NULL);
	g_signal_connect(G_OBJECT(action_select_background), "activate",
							  G_CALLBACK(sakura_background_selection), NULL);	
	g_signal_connect(G_OBJECT(action_copy), "activate", G_CALLBACK(sakura_copy), NULL);	
	g_signal_connect(G_OBJECT(action_paste), "activate", G_CALLBACK(sakura_paste), NULL);	
	g_signal_connect(G_OBJECT(action_select_colors), "activate", G_CALLBACK(sakura_color_dialog), NULL);	
	g_signal_connect(G_OBJECT(item_show_first_tab), "activate", G_CALLBACK(sakura_show_first_tab), NULL);	
	g_signal_connect(G_OBJECT(action_open_link), "activate", G_CALLBACK(sakura_open_url), NULL);	
    g_signal_connect(G_OBJECT(action_copy_link), "activate", G_CALLBACK(sakura_copy_url), NULL);
	g_signal_connect(G_OBJECT(action_clear_background), "activate", G_CALLBACK(sakura_clear), NULL);	
	g_signal_connect(G_OBJECT(action_opacity), "activate", G_CALLBACK(sakura_set_opacity), NULL);
	g_signal_connect(G_OBJECT(action_set_title), "activate", G_CALLBACK(sakura_set_title_dialog), NULL);

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

	while (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) >= 1) {
		sakura_del_tab();
	}

	pango_font_description_free(sakura.font);

	if (sakura.background)
		free(sakura.background);

	GError *gerror = NULL;
	gsize len = 0;
	gchar *data = g_key_file_to_data(sakura.cfg, &len, &gerror);
	if (!data) {
		fprintf(stderr, "%s\n", gerror->message);
		exit(EXIT_FAILURE);
	}

	/* Write to file */
	GIOChannel *cfgfile = g_io_channel_new_file(sakura.configfile, "w", &gerror);
	if (!cfgfile) {
		fprintf(stderr, "%s\n", gerror->message);
		exit(EXIT_FAILURE);
	}

	/* FIXME: if the number of chars written is not "len", something happened.
	 * Check for errors appropriately...*/
	GIOStatus status = g_io_channel_write_chars(cfgfile, data, len, NULL, &gerror);
	if (status != G_IO_STATUS_NORMAL) {
		// FIXME: we should deal with temporary failures (G_IO_STATUS_AGAIN)
		fprintf(stderr, "%s\n", gerror->message);
		exit(EXIT_FAILURE);
	}

	g_io_channel_close(cfgfile);

	g_key_file_free(sakura.cfg);

	free(sakura.configfile);

	gtk_main_quit();

}


static void
sakura_set_size()
{
    struct terminal *term;
	GtkRequisition main_request;
	GtkRequisition term_request;
	GdkGeometry hints;
	gint pad_x, pad_y;
	gint char_width, char_height;

    term = sakura_get_page_term( sakura, 0 );
    vte_terminal_get_padding(VTE_TERMINAL(term->vte), (int *)&pad_x, (int *)&pad_y);
    char_width = vte_terminal_get_char_width(VTE_TERMINAL(term->vte));
    char_height = vte_terminal_get_char_height(VTE_TERMINAL(term->vte));

	hints.min_width = char_width + pad_x;
	hints.min_height = char_height + pad_y;
	hints.base_width = pad_x;
	hints.base_height = pad_y;
	hints.width_inc = char_width;
	hints.height_inc = char_height;
	gtk_window_set_geometry_hints ( GTK_WINDOW (sakura.main_window),
            GTK_WIDGET (term->vte),
			&hints,
			GDK_HINT_RESIZE_INC | GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);

	gtk_widget_size_request (sakura.main_window, &main_request);
    gtk_widget_size_request (term->vte, &term_request);
	sakura.width = main_request.width - term_request.width;
	sakura.height = main_request.height - term_request.height;
	sakura.width += pad_x + char_width * sakura.term_info.columns;
	sakura.height += pad_y + char_height * sakura.term_info.rows;
	if (GTK_WIDGET_MAPPED (sakura.main_window)) {
		gtk_window_resize (GTK_WINDOW (sakura.main_window), sakura.width, sakura.height);
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
	static gboolean init = FALSE;

	n_pages=gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook));

	/* Set the font for all tabs */
	for (i = (n_pages - 1); i >= 0; i--) {
        term = sakura_get_page_term( sakura, i );
        vte_terminal_set_font(VTE_TERMINAL(term->vte), sakura.font);
	}

	gtk_widget_show_all(sakura.main_window);
	if (!sakura.term_info.init) {
		if (init)
			return;
	}

	sakura_set_size();
	init = TRUE;
}


static void
sakura_add_tab()
{
    struct terminal *term;
	int index;
	gchar *label_text;
    gchar *cwd = NULL;

    term = g_new0( struct terminal, 1 );
    term->hbox=gtk_hbox_new(FALSE, 0);
    term->vte=vte_terminal_new();

    vte_terminal_set_size(VTE_TERMINAL(term->vte), DEFAULT_COLUMNS, DEFAULT_ROWS);

	label_text=g_strdup_printf(_("Terminal %d"), sakura.label_count++);
    term->label=gtk_label_new(label_text);
	g_free(label_text);

	/* Init vte */
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(term->vte), SCROLL_LINES);
    vte_terminal_match_add(VTE_TERMINAL(term->vte), HTTP_REGEXP);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(term->vte), TRUE);

	if (sakura.show_scrollbar) {
		term->scrollbar=gtk_vscrollbar_new(vte_terminal_get_adjustment(VTE_TERMINAL(term->vte)));
	}

    gtk_box_pack_start(GTK_BOX(term->hbox), term->vte, TRUE, TRUE, 0);
	if (sakura.show_scrollbar) {
		gtk_box_pack_start(GTK_BOX(term->hbox), term->scrollbar, FALSE, FALSE, 0);
	}

    index = gtk_notebook_get_current_page( GTK_NOTEBOOK(sakura.notebook) );

    if ((index=gtk_notebook_append_page(GTK_NOTEBOOK(sakura.notebook), term->hbox, term->label))==-1) {
		sakura_error("Cannot create a new tab"); 
		return;
	}

#if GTK_CHECK_VERSION( 2, 10, 0 )
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(sakura.notebook), term->hbox, TRUE);
	// TODO: Set group id to support detached tabs
	//Disable until it gets stable	
#if 0
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(sakura.notebook), term->hbox, TRUE);
#endif
#endif

	sakura_set_page_term(sakura, index, term );

	/* GtkNotebook signals */
#if GTK_CHECK_VERSION( 2, 10, 0 )
	/* g_signal_connect(G_OBJECT(sakura.notebook), "reorder-tab", G_CALLBACK(sakura_reordered_tab), NULL); */
	/* g_signal_connect(G_OBJECT(sakura.notebook), "page-reordered", G_CALLBACK(sakura_page_reordered), NULL); */
	g_signal_connect(G_OBJECT(sakura.notebook), "move-focus-out", G_CALLBACK(sakura_move_focus_out), NULL);
	g_signal_connect(G_OBJECT(sakura.notebook), "focus-tab", G_CALLBACK(sakura_focus_tab), NULL);
#endif

	/* vte signals */
    g_signal_connect(G_OBJECT(term->vte), "increase-font-size", G_CALLBACK(sakura_increase_font), NULL);
    g_signal_connect(G_OBJECT(term->vte), "decrease-font-size", G_CALLBACK(sakura_decrease_font), NULL);
    g_signal_connect(G_OBJECT(term->vte), "child-exited", G_CALLBACK(sakura_child_exited), NULL);
    g_signal_connect(G_OBJECT(term->vte), "eof", G_CALLBACK(sakura_eof), NULL);
    g_signal_connect(G_OBJECT(term->vte), "window-title-changed", G_CALLBACK(sakura_title_changed), NULL);
    g_signal_connect_swapped(G_OBJECT(term->vte), "button-press-event", G_CALLBACK(sakura_popup), sakura.menu);

	cwd = g_get_current_dir();

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

		if (option_execute) {
			int command_argc; char **command_argv;
			GError *gerror;
			gchar *path;

			if (!g_shell_parse_argv(option_execute, &command_argc, &command_argv, &gerror)) {
				sakura_error("Cannot parse command line arguments");
				exit(1);
			}

			/* Check if the command is valid */
			path=g_find_program_in_path(command_argv[0]);
			if (path) 
				free(path);
			else
				option_execute=NULL;

            term->pid=vte_terminal_fork_command(VTE_TERMINAL(term->vte), command_argv[0],
					command_argv, NULL, cwd, TRUE, TRUE, TRUE);
			g_strfreev(command_argv);
			option_execute=NULL;
		} else {
			term->pid=vte_terminal_fork_command(VTE_TERMINAL(term->vte), sakura.argv[0],
			                                    sakura.argv, NULL, cwd, TRUE, TRUE, TRUE);
		}
	/* Not the first tab */	
	} else {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		sakura_set_font();
		/* Call set_current page after showing the widget: gtk ignores this
		 * function in the window is not visible *sigh*. Gtk documentation
		 * says this is for "historical" reasons. Me arse */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), index);
		term->pid=vte_terminal_fork_command(VTE_TERMINAL(term->vte), sakura.argv[0],
		                                    sakura.argv, NULL, cwd, TRUE, TRUE,TRUE);
	}

	free(cwd);

	/* Configuration per-terminal */
    vte_terminal_set_backspace_binding(VTE_TERMINAL(term->vte), VTE_ERASE_ASCII_DELETE);
	/* TODO: Use tango color pallete with vte_terminal_set_colors */
    vte_terminal_set_color_foreground(VTE_TERMINAL(term->vte), &sakura.forecolor);
    vte_terminal_set_color_bold(VTE_TERMINAL(term->vte), &sakura.boldcolor);
    vte_terminal_set_color_background(VTE_TERMINAL(term->vte), &sakura.backcolor);

	if (sakura.fake_transparency) {
        vte_terminal_set_background_saturation(VTE_TERMINAL (term->vte), sakura.opacity_level);
        vte_terminal_set_background_transparent(VTE_TERMINAL (term->vte),TRUE);
	}

	if (sakura.background) {
		sakura_set_bgimage(sakura.background);
	}

    if (sakura.word_chars) {
        vte_terminal_set_word_chars( VTE_TERMINAL (term->vte), sakura.word_chars );
    }

	/* Grrrr. Why the fucking label widget in the notebook STEAL the fucking focus? */
    gtk_widget_grab_focus(term->vte);
}


static void
sakura_del_tab()
{
	gint page;
    struct terminal *term;

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );

    gtk_widget_hide(term->hbox);
    //gtk_widget_hide(term->label);

	gtk_notebook_remove_page(GTK_NOTEBOOK(sakura.notebook), page);

	if ( gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 1) {
        char *cfgtmp = g_key_file_get_value(sakura.cfg, cfg_group, "show_always_first_tab", NULL);
		if (strcmp(cfgtmp, "Yes")==0) {	
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		} else {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}
		g_free(cfgtmp);
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

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
    term = sakura_get_page_term( sakura, page );

	/* Check file existence and type */
	if (g_file_test(infile, G_FILE_TEST_IS_REGULAR)) {

		pixbuf = gdk_pixbuf_new_from_file (infile, &gerror);
		if (!pixbuf) {
			sakura_error("Not using image file, %s\n", gerror->message);
		} else {
            vte_terminal_set_background_image(VTE_TERMINAL(term->vte), pixbuf);
            vte_terminal_set_background_saturation(VTE_TERMINAL(term->vte), TRUE);
            vte_terminal_set_background_transparent(VTE_TERMINAL(term->vte),FALSE);

            g_key_file_set_value(sakura.cfg, cfg_group, "background", infile);
		}
	}
}


static void
sakura_error(const char *format, ...) 
{
	GtkWidget *dialog;
	va_list args;

	va_start(args, format);

	dialog = gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_DESTROY_WITH_PARENT,
			 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, format, args);
	gtk_dialog_run (GTK_DIALOG (dialog));

	va_end(args);
	gtk_widget_destroy (dialog);
}


int
main(int argc, char **argv)
{
    struct terminal *term;
	gchar *localedir;
	GError *error=NULL;
	GOptionContext *context;
	int i;
	
	/* Localization */
	setlocale(LC_ALL, "");
	localedir=g_strdup_printf("%s/locale", DATADIR);
	textdomain(GETTEXT_PACKAGE);
	bindtextdomain(GETTEXT_PACKAGE, localedir);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	
	/* Options parsing */
	context = g_option_context_new (_("- vte-based terminal emulator"));
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_group_set_translation_domain(gtk_get_option_group(TRUE), GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group(TRUE));
	g_option_context_parse (context, &argc, &argv, &error);

	if (option_version) {
		fprintf(stderr, _("sakura version is %s\n"), VERSION);
		exit(1);
	} 

	if (option_ntabs <= 0) {
		option_ntabs=1;
	}

	g_option_context_free(context);

	gtk_init(&argc, &argv);

	/* Init stuff */
	sakura_init();

	/* Add first tab */
	for (i=0; i<option_ntabs; i++)
		sakura_add_tab();
	/* Fill Input Methods menu */
    term = sakura_get_page_term( sakura, 0 );
    vte_terminal_im_append_menuitems(VTE_TERMINAL(term->vte), GTK_MENU_SHELL(sakura.im_menu));
	sakura.term_info.init = TRUE;
	
	gtk_main();

	return 0;
}

