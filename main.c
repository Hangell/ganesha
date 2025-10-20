#include <adwaita.h>

static void on_activate(GApplication *app, gpointer user_data) {
    (void)user_data;

    // Janela principal (AdwApplicationWindow exige GtkApplication*)
    AdwApplicationWindow *win = ADW_APPLICATION_WINDOW(
        adw_application_window_new(GTK_APPLICATION(app))
    );
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 320);
    gtk_window_set_title(GTK_WINDOW(win), "Ganesha");

    // Cria a HeaderBar
    AdwHeaderBar *header = ADW_HEADER_BAR(adw_header_bar_new());
    GtkWidget *title = gtk_label_new("Ganesha");
    adw_header_bar_set_title_widget(header, title);

    // Cria uma ToolbarView para hospedar barras (top/bottom) + conteúdo
    AdwToolbarView *view = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    adw_toolbar_view_add_top_bar(view, GTK_WIDGET(header));

    // Conteúdo placeholder
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);

    GtkWidget *hello = gtk_label_new("Base pronta com Libadwaita ✅");
    gtk_box_append(GTK_BOX(box), hello);

    // Define o conteúdo principal dentro da ToolbarView
    adw_toolbar_view_set_content(view, box);

    // Coloca a ToolbarView dentro da janela
    adw_application_window_set_content(win, GTK_WIDGET(view));

    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    adw_init();

    AdwApplication *app = ADW_APPLICATION(
        adw_application_new("org.hangell.ganesha", G_APPLICATION_DEFAULT_FLAGS)
    );
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
