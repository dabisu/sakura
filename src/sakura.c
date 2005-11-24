#include "mobs.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pango.h>
#include <vte/vte.h>


static struct {
	GtkWidget *main_window;
	GtkWidget *notebook;
	GArray *terminals;
	GtkWidget *menu;
	GtkWidget *im_menu;
	PangoFontDescription *font;
	GtkWidget *open_link_item;
	GtkWidget *open_link_separator;
	char *current_match;
} sakura;

struct terminal {
	GtkWidget* vte;		/* Reference to VTE terminal */
	pid_t pid;			/* pid of the forked proccess */
};

#define DEFAULT_FONT "Bitstream Vera Sans Mono 14"
#define SCROLL_LINES 4096
#define HTTP_REGEXP "http://[-a-zA-Z0-9.?$%&/=_]*"

/* Callbacks */
static gboolean sakura_key_press (GtkWidget *, GdkEventKey *, gpointer);
static void sakura_increase_font (GtkWidget *, void *);
static void sakura_decrease_font (GtkWidget *, void *);
static void sakura_child_exited (GtkWidget *, void *);
static void sakura_eof (GtkWidget *, void *);
static gboolean sakura_delete_window (GtkWidget *, void *);
static void sakura_destroy_window (GtkWidget *, void *);
static gboolean sakura_popup (GtkWidget *, GdkEvent *);
static void sakura_font_dialog (GtkWidget *, void *);
static void sakura_new_tab (GtkWidget *, void *);
static void sakura_background_selection (GtkWidget *, void *);
static void sakura_open_url (GtkWidget *, void *);
static void sakura_make_transparent (GtkWidget *, void *);

/* Functions */	
static void sakura_init();
static void sakura_destroy();
static void sakura_add_tab();
static void sakura_del_tab();
static void sakura_set_font();
static void sakura_kill_child();
static void sakura_set_bgimage();


static gboolean sakura_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	unsigned int topage=0;

	if ( (event->state==GDK_CONTROL_MASK) && (event->type==GDK_KEY_PRESS)) {
		if (event->keyval==GDK_t) {
			sakura_add_tab();
			return TRUE;
		} else if (event->keyval==GDK_w) {
			sakura_kill_child();
			sakura_del_tab();
			return TRUE;
		}
	}
	
	if ( (event->state==GDK_MOD1_MASK) && (event->type==GDK_KEY_PRESS)) {
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
		}
	}
	
	return FALSE;
}


static void sakura_increase_font (GtkWidget *widget, void *data)
{
	gint size;
	
	size=pango_font_description_get_size(sakura.font);
	pango_font_description_set_size(sakura.font, ((size/PANGO_SCALE)+1) * PANGO_SCALE);
	
	sakura_set_font();
}


static void sakura_decrease_font (GtkWidget *widget, void *data)
{
	gint size;
	
	size=pango_font_description_get_size(sakura.font);
	pango_font_description_set_size(sakura.font, ((size/PANGO_SCALE)-1) * PANGO_SCALE);
	
	sakura_set_font();
}


static void sakura_child_exited (GtkWidget *widget, void *data)
{
	int status, page;
	struct terminal term;
		
	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term=g_array_index(sakura.terminals, struct terminal,  page);

	SAY("waiting for terminal pid %d", term.pid);
	
	waitpid(term.pid, &status, WNOHANG);
	/* TODO: check wait return */	

	/* Remove the array element before removing the notebook page */
	g_array_remove_index(sakura.terminals, page);
			
	sakura_del_tab();

}


static void sakura_eof (GtkWidget *widget, void *data)
{
	int status, page;
	struct terminal term;
	
	SAY("Got EOF signal");

	/* Workaround for libvte strange behaviour. Check with
	   libvte authors about child-exited/eof signals */
	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook))==0) {
		
		page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
		term=g_array_index(sakura.terminals, struct terminal,  page);

		SAY("waiting for terminal pid (in eof) %d", term.pid);
		
		waitpid(term.pid, &status, WNOHANG);
		/* TODO: check wait return */	

		/* Remove the array element before removing the notebook page */
		g_array_remove_index(sakura.terminals, page);
				
		sakura_del_tab();
	}
}
	

static gboolean sakura_delete_window (GtkWidget *widget, void *data)
{
	GtkWidget *dialog;
	guint response;

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))>1) {
		dialog=gtk_message_dialog_new(GTK_WINDOW(sakura.main_window), GTK_DIALOG_MODAL,
									  GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
									  "There are several tabs opened. Do you really want to close Sakura?");
		
		response=gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		if (response==GTK_RESPONSE_YES) 
			return FALSE;
		else
			return TRUE;
	}

	return FALSE;
}


static void sakura_destroy_window (GtkWidget *widget, void *data)
{
	sakura_destroy();
}


static gboolean sakura_popup (GtkWidget *widget, GdkEvent *event)
{
	GtkMenu *menu;
	GdkEventButton *event_button;
	struct terminal term;
	glong column, row;
	int page, tag;

	menu = GTK_MENU (widget);

	if (event->type == GDK_BUTTON_PRESS) {
		
		event_button = (GdkEventButton *) event;
		
		page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
		term=g_array_index(sakura.terminals, struct terminal,  page);
		
		/* Find out if cursor it's over a matched expression...*/

		/* Get the column and row relative to pointer position */
		column=((glong)(event_button->x) / vte_terminal_get_char_width(VTE_TERMINAL(term.vte)));
		row=((glong)(event_button->y) / vte_terminal_get_char_height(VTE_TERMINAL(term.vte)));
		sakura.current_match=vte_terminal_match_check(VTE_TERMINAL(term.vte), column, row, &tag);

		if (sakura.current_match) {
			/* Show the extra options in the menu */
			gtk_widget_show(sakura.open_link_item);
			gtk_widget_show(sakura.open_link_separator);
		} else {
			/* Hide all the options */
			gtk_widget_hide(sakura.open_link_item);
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


static void sakura_font_dialog (GtkWidget *widget, void *data)
{
	GtkWidget *font_dialog;
	gint response;

	font_dialog=gtk_font_selection_dialog_new("Select font");

	response=gtk_dialog_run(GTK_DIALOG(font_dialog));
	
	if (response==GTK_RESPONSE_OK) {
		pango_font_description_free(sakura.font);
		sakura.font=pango_font_description_from_string(gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(font_dialog)));
	    sakura_set_font();
	}

	gtk_widget_destroy(font_dialog);
}


static void sakura_background_selection (GtkWidget *widget, void *data)
{
	GtkWidget *dialog;
	gint response;
	gchar *filename;
	
	dialog = gtk_file_chooser_dialog_new ("Select a background file", GTK_WINDOW(sakura.main_window),
								  	      GTK_FILE_CHOOSER_ACTION_OPEN,
										  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
										  NULL);


	response=gtk_dialog_run(GTK_DIALOG(dialog));
	if (response == GTK_RESPONSE_ACCEPT) {

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		sakura_set_bgimage(filename);
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
	
}


static void sakura_open_url (GtkWidget *widget, void *data)
{
	GError *error=NULL;
	gchar *cmd;
	const gchar *browser=NULL;
	
	browser=g_getenv("BROWSER");

	if (browser) {
		cmd=g_strdup_printf("%s %s", browser, sakura.current_match);
	} else {
		cmd=g_strdup_printf("firefox %s", sakura.current_match);
	}
	
	if (!g_spawn_command_line_async(cmd, &error)) {
		SAY("Couldn't exec %s", cmd);
	}

	g_free(cmd);
}

static void sakura_make_transparent (GtkWidget *widget, void *data)
{
	/* tjb Do some transparency magic/hacking */
	/* tjb probably need to do some checking here and  in sakura_set_bgimage for transparency */
	/* tjb among other things already being set. */
	int page;
	struct terminal term;
	
	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term=g_array_index(sakura.terminals, struct terminal,  page);	

	SAY("setting term.pid: %d to transparent...",term.pid);
	vte_terminal_set_background_transparent(VTE_TERMINAL (term.vte),TRUE);
	vte_terminal_set_background_saturation(VTE_TERMINAL (term.vte),TRUE);
}


/* Functions */

static void sakura_new_tab (GtkWidget *widget, void *data)
{
	sakura_add_tab();
}


static void sakura_init()
{
	GtkWidget *item1, *item2, *item3, *item3b, *item4, *separator, *separator2;

	sakura.main_window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	sakura.notebook=gtk_notebook_new();
	sakura.terminals=g_array_sized_new(FALSE, TRUE, sizeof(struct terminal), 5);
	sakura.font=pango_font_description_from_string(DEFAULT_FONT);
	sakura.menu=gtk_menu_new();

	gtk_container_add(GTK_CONTAINER(sakura.main_window), sakura.notebook);
	
	/* Init notebook */
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(sakura.notebook), TRUE);

	/* Init popup menu*/
	sakura.open_link_item=gtk_menu_item_new_with_label("Open link...");
	sakura.open_link_separator=gtk_separator_menu_item_new();
	item1=gtk_menu_item_new_with_label("New tab");
	item2=gtk_menu_item_new_with_label("Select font...");
	item3=gtk_menu_item_new_with_label("Set background...");
	item3b=gtk_menu_item_new_with_label("Make transparent...");
	item4=gtk_menu_item_new_with_label("Input methods");
	separator=gtk_separator_menu_item_new();
	separator2=gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.open_link_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), sakura.open_link_separator);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item1);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), separator);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item2);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item3);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item3b);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), separator2);
	gtk_menu_shell_append(GTK_MENU_SHELL(sakura.menu), item4);		
	sakura.im_menu=gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item4), sakura.im_menu);
	
	g_signal_connect(G_OBJECT(item1), "activate", G_CALLBACK(sakura_new_tab), NULL);
	g_signal_connect(G_OBJECT(item2), "activate", G_CALLBACK(sakura_font_dialog), NULL);
	g_signal_connect(G_OBJECT(item3), "activate", G_CALLBACK(sakura_background_selection), NULL);	
	g_signal_connect(G_OBJECT(item3b), "activate", G_CALLBACK(sakura_make_transparent), NULL);	
	g_signal_connect(G_OBJECT(sakura.open_link_item), "activate", G_CALLBACK(sakura_open_url), NULL);	

	gtk_widget_show_all(sakura.menu);

	g_signal_connect(G_OBJECT(sakura.main_window), "delete_event", G_CALLBACK(sakura_delete_window), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "destroy", G_CALLBACK(sakura_destroy_window), NULL);
	g_signal_connect(G_OBJECT(sakura.main_window), "key-press-event", G_CALLBACK(sakura_key_press), NULL);

}


static void sakura_destroy()
{
	SAY("Destroying sakura");

	while (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) > 1) {
		sakura_del_tab();
	}
	g_array_free(sakura.terminals, TRUE);
	g_free(sakura.font);

	gtk_main_quit();
}


static void sakura_set_font()
{
	int page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	
	vte_terminal_set_font(VTE_TERMINAL(gtk_notebook_get_nth_page(GTK_NOTEBOOK(sakura.notebook), page)),
		                  sakura.font);
}


static void sakura_add_tab()
{
	struct terminal term;
	int index;
	
	term.vte=vte_terminal_new();

	/* Init vte */
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(term.vte), SCROLL_LINES);
	vte_terminal_match_add(VTE_TERMINAL(term.vte), HTTP_REGEXP);
	
	/*TODO: Check parameters */
	term.pid=vte_terminal_fork_command(VTE_TERMINAL(term.vte), g_getenv("SHELL"), NULL, NULL, g_getenv("HOME"), TRUE, TRUE,TRUE);
	if ((index=gtk_notebook_append_page(GTK_NOTEBOOK(sakura.notebook), term.vte, NULL))==-1) {
		SAY("Cannot create a new tab"); BUG();
		return;
	}
		
	g_array_append_val(sakura.terminals, term);
	g_signal_connect(G_OBJECT(term.vte), "increase-font-size", G_CALLBACK(sakura_increase_font), NULL);
	g_signal_connect(G_OBJECT(term.vte), "decrease-font-size", G_CALLBACK(sakura_decrease_font), NULL);
	g_signal_connect(G_OBJECT(term.vte), "child-exited", G_CALLBACK(sakura_child_exited), NULL);
	g_signal_connect(G_OBJECT(term.vte), "eof", G_CALLBACK(sakura_eof), NULL);
	g_signal_connect_swapped(G_OBJECT(term.vte), "button-press-event", G_CALLBACK(sakura_popup), sakura.menu);
	
	/* Show everything the first time after creating a terminal. Unrationale:
	   im_append and set_current_page fails if the window isn't visible */
	if  ( gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 1) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		gtk_notebook_set_show_border(GTK_NOTEBOOK(sakura.notebook), FALSE);
		sakura_set_font();
		gtk_widget_show_all(sakura.main_window);
	} else {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
		gtk_widget_show_all(sakura.main_window);
	    gtk_notebook_set_current_page(GTK_NOTEBOOK(sakura.notebook), index);
		sakura_set_font();
	}
	
}


static void sakura_del_tab()
{
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook))==1) {
		/* Last terminal was closed so... close the application */
		gtk_notebook_remove_page(GTK_NOTEBOOK(sakura.notebook),
								 gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook)));
		sakura_destroy();
	} else {
		gtk_notebook_remove_page(GTK_NOTEBOOK(sakura.notebook),
		                                      gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook)));
		if ( gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 1) {
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		}
	}
}


static void sakura_kill_child()
{
	/* TODO: Kill the forked child nicely */
}


static void sakura_set_bgimage(char *infile)	
{
	GError *gerror=NULL;
	GdkPixbuf *pixbuf=NULL;
	int page;
	struct terminal term;
	
	ASSERT(infile);

	page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	term=g_array_index(sakura.terminals, struct terminal,  page);	

	/* Check file existence and type */
	if (g_file_test(infile, G_FILE_TEST_IS_REGULAR))
 		pixbuf = gdk_pixbuf_new_from_file (infile, &gerror);

	if (!pixbuf) {
	 	SAY("Frick: %s\n", gerror->message);
	 	SAY("Frick: not using image file...\n");
	} else {
		vte_terminal_set_background_image(VTE_TERMINAL(term.vte), pixbuf);
	 	vte_terminal_set_background_saturation(VTE_TERMINAL(term.vte), TRUE);
		vte_terminal_set_background_transparent(VTE_TERMINAL(term.vte),FALSE);
	 }
}


int main(int argc, char **argv)
{
	struct terminal term;
	
	gtk_init(&argc, &argv);

	/* Init stuff */
	sakura_init();
	/* Add first tab */
	sakura_add_tab();
    /* Fill Input Methods menu */
	term=g_array_index(sakura.terminals, struct terminal, 0);
	vte_terminal_im_append_menuitems(VTE_TERMINAL(term.vte), GTK_MENU_SHELL(sakura.im_menu));

	gtk_main();

	return 0;
}

