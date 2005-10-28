#include "mobs.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

static struct {
	GtkWidget *main_window;
	GtkWidget *notebook;
	GArray *terminals;
	gchar *font;
	gint size;
} sakura;

struct terminal {
	GtkWidget* vte;		/* Reference to VTE terminal */
	pid_t pid;			/* pid of the forked proccess */
};

#define DEFAULT_FONT "Bitstream Vera Sans Mono"
#define DEFAULT_FONT_SIZE 14

/* Callbacks */
static void sakura_increase_font (GtkWidget *, void *);
static void sakura_decrease_font (GtkWidget *, void *);
static void sakura_child_exited (GtkWidget *, void *);

/* Functions */	
static void sakura_init();
static void sakura_destroy();
static void sakura_add_tab();
static void sakura_del_tab();
static void sakura_set_font();


static void sakura_increase_font (GtkWidget *widget, void *data)
{
	sakura.size += 1;
	sakura_set_font();
}

static void sakura_decrease_font (GtkWidget *widget, void *data)
{
	sakura.size -= 1;
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
			
    if ( gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 1) {
		/* Last terminal was closes so...*/
		sakura_destroy();
	} else {
		sakura_del_tab();
	}

}

static void sakura_init()
{
	sakura.main_window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	sakura.notebook=gtk_notebook_new();
	sakura.terminals=g_array_sized_new(FALSE, TRUE, sizeof(struct terminal), 5);
	sakura.font=g_strdup(DEFAULT_FONT);
	sakura.size=DEFAULT_FONT_SIZE;

	gtk_container_add(GTK_CONTAINER(sakura.main_window), sakura.notebook);
}

static void sakura_destroy()
{
	/* Destroy the last notebook page */
	sakura_del_tab();

	g_array_destroy(sakura.terminals);
	gtk_widget_destroy(sakura.main_window);
	g_free(sakura.font);

	gtk_main_quit();
}

static void sakura_set_font()
{
	int page=gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook));
	gchar *fontstring;
	
	fontstring=g_strdup_printf("%s %d", sakura.font, sakura.size);
	vte_terminal_set_font_from_string(VTE_TERMINAL(gtk_notebook_get_nth_page(GTK_NOTEBOOK(sakura.notebook),
			   						  page)), fontstring);
	g_free(fontstring);
}


static void sakura_add_tab()
{
	struct terminal term;

	term.vte=vte_terminal_new();
	/*TODO: Get real user shell. Check parameters */
	term.pid=vte_terminal_fork_command(VTE_TERMINAL(term.vte), "/bin/zsh", NULL, NULL, NULL, TRUE, TRUE,TRUE);
	gtk_notebook_append_page(GTK_NOTEBOOK(sakura.notebook), term.vte, NULL);

    if ( gtk_notebook_get_n_pages(GTK_NOTEBOOK(sakura.notebook)) == 1) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), FALSE);
		gtk_notebook_set_show_border(GTK_NOTEBOOK(sakura.notebook), FALSE);
	} else {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(sakura.notebook), TRUE);
	}

	g_array_append_val(sakura.terminals, term);
	g_signal_connect(G_OBJECT(term.vte), "increase-font-size", G_CALLBACK(sakura_increase_font), NULL);
	g_signal_connect(G_OBJECT(term.vte), "decrease-font-size", G_CALLBACK(sakura_decrease_font), NULL);
	g_signal_connect(G_OBJECT(term.vte), "child-exited", G_CALLBACK(sakura_child_exited), NULL);
}

static void sakura_del_tab()
{
	gtk_notebook_remove_page(GTK_NOTEBOOK(sakura.notebook),
							 gtk_notebook_get_current_page(GTK_NOTEBOOK(sakura.notebook)));
}

int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);

	/* Init stuff */
	sakura_init();

	/* Add first tab */
	sakura_add_tab();
	sakura_set_font();

	gtk_widget_show_all(sakura.main_window);
	gtk_main();

	return 0;
}

