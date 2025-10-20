#include <adwaita.h>

typedef struct {
    GtkTextView   *output_view;
    GtkEntry      *prompt_entry;
} AppWidgets;

static void append_text(GtkTextView *view, const char *text) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, text, -1);
    gtk_text_buffer_insert(buf, &end, "\n", -1);

    // rolar para o final
    GtkTextMark *mark = gtk_text_buffer_create_mark(buf, NULL, &end, FALSE);
    gtk_text_view_scroll_mark_onscreen(view, mark);
    gtk_text_buffer_delete_mark(buf, mark);
}

static void on_send_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppWidgets *aw = (AppWidgets*)user_data;

    const char *txt = gtk_editable_get_text(GTK_EDITABLE(aw->prompt_entry));
    if (txt && *txt) {
        // por enquanto só joga o texto para a área de saída
        append_text(aw->output_view, txt);
        // limpar entrada
        gtk_editable_set_text(GTK_EDITABLE(aw->prompt_entry), "");
    }
}

static void on_entry_activate(GtkEntry *entry, gpointer user_data) {
    (void)entry;
    // Reaproveita a mesma lógica do botão
    on_send_clicked(NULL, user_data);
}

static void on_activate(GApplication *app, gpointer user_data) {
    (void)user_data;

    // janela base (Libadwaita)
    AdwApplicationWindow *win = ADW_APPLICATION_WINDOW(
        adw_application_window_new(GTK_APPLICATION(app))
    );
    gtk_window_set_default_size(GTK_WINDOW(win), 640, 420);
    gtk_window_set_title(GTK_WINDOW(win), "Ganesha");

    // header bar (topo)
    AdwHeaderBar *header = ADW_HEADER_BAR(adw_header_bar_new());
    GtkWidget *title = gtk_label_new("Ganesha");
    adw_header_bar_set_title_widget(header, title);

    // ToolbarView para acomodar barras e conteúdo
    AdwToolbarView *view = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    adw_toolbar_view_add_top_bar(view, GTK_WIDGET(header));

    // conteúdo principal: caixa vertical
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 12);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);

    // área de saída (TextView dentro de ScrolledWindow)
    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_widget_set_hexpand(scroller, TRUE);

    GtkWidget *output = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(output), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(output), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), output);

    // linha de prompt (Entry + Button)
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type your prompt…");

    GtkWidget *btn = gtk_button_new_with_label("Send");

    gtk_box_append(GTK_BOX(hbox), entry);
    gtk_box_append(GTK_BOX(hbox), btn);

    // montar vbox
    gtk_box_append(GTK_BOX(vbox), scroller);
    gtk_box_append(GTK_BOX(vbox), hbox);

    // conectar tudo
    AppWidgets *aw = g_new0(AppWidgets, 1);
    aw->output_view  = GTK_TEXT_VIEW(output);
    aw->prompt_entry = GTK_ENTRY(entry);

    g_signal_connect(btn,   "clicked",  G_CALLBACK(on_send_clicked), aw);
    g_signal_connect(entry, "activate", G_CALLBACK(on_entry_activate), aw); // ENTER no entry

    // finalizar janela
    adw_toolbar_view_set_content(view, vbox);
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
