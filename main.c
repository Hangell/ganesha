#include <adwaita.h>

typedef struct {
    GtkTextView   *output_view;
    GtkEntry      *prompt_entry;
    GtkButton     *send_btn;
    GtkButton     *stop_btn;

    GCancellable  *cancellable;
    gboolean       in_progress;
    gboolean       alive;        // <- flag: janela viva
} AppWidgets;

/* ---------- helpers UI ---------- */

static void append_text(GtkTextView *view, const char *text) {
    if (!view) return; // guarda extra
    GtkTextBuffer *buf = gtk_text_view_get_buffer(view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, text, -1);
    GtkTextMark *mark = gtk_text_buffer_create_mark(buf, NULL, &end, FALSE);
    gtk_text_view_scroll_mark_onscreen(view, mark);
    gtk_text_buffer_delete_mark(buf, mark);
}

static void set_streaming_state(AppWidgets *aw, gboolean running) {
    if (!aw || !aw->alive) return;
    aw->in_progress = running;
    if (aw->send_btn)
        gtk_widget_set_sensitive(GTK_WIDGET(aw->send_btn), !running);
    if (aw->stop_btn)
        gtk_widget_set_sensitive(GTK_WIDGET(aw->stop_btn), running);
}

/* ---------- callbacks postados no main loop ---------- */

typedef struct {
    AppWidgets *aw;
    char       *chunk;
} AppendChunkData;

static gboolean ui_append_chunk_cb(gpointer data) {
    AppendChunkData *d = (AppendChunkData*)data;
    if (d->aw && d->aw->alive)
        append_text(d->aw->output_view, d->chunk);
    g_free(d->chunk);
    g_free(d);
    return G_SOURCE_REMOVE;
}

static gboolean ui_append_assistant_prefix_cb(gpointer data) {
    AppWidgets *aw = (AppWidgets*)data;
    if (aw && aw->alive)
        append_text(aw->output_view, "Assistant: ");
    return G_SOURCE_REMOVE;
}

static gboolean ui_finish_stream_cb(gpointer data) {
    AppWidgets *aw = (AppWidgets*)data;
    if (aw && aw->alive) {
        append_text(aw->output_view, "\n");
        set_streaming_state(aw, FALSE);
    }
    if (aw && aw->cancellable) {
        g_clear_object(&aw->cancellable); // liberar com segurança ao fim
    }
    return G_SOURCE_REMOVE;
}

/* ---------- worker de streaming (simulado) ---------- */

typedef struct {
    AppWidgets *aw;
    char       *prompt_copy;
} WorkerArgs;

static gpointer stream_worker(gpointer data) {
    WorkerArgs *wa = (WorkerArgs*)data;
    AppWidgets *aw = wa->aw;

    // se já não está vivo, encerra cedo
    if (!aw || !aw->alive) {
        g_free(wa->prompt_copy);
        g_free(wa);
        return NULL;
    }

    GCancellable *c = aw->cancellable;
    g_idle_add(ui_append_assistant_prefix_cb, aw);

    gchar *reply = g_strdup_printf(
        "You said: \"%s\". Here's a simulated streaming reply with chunks coming in… ",
        wa->prompt_copy);

    gchar **words = g_strsplit(reply, " ", -1);
    for (gint i = 0; words[i] != NULL; i++) {
        if (g_cancellable_is_cancelled(c)) break;

        AppendChunkData *chunk = g_new0(AppendChunkData, 1);
        chunk->aw = aw;
        chunk->chunk = g_strdup_printf("%s ", words[i]);
        g_idle_add(ui_append_chunk_cb, chunk);

        g_usleep(80 * 1000); // ~80ms entre pedaços

        // se a UI morreu durante o stream, para
        if (!aw->alive) break;
    }

    g_idle_add(ui_finish_stream_cb, aw);

    g_strfreev(words);
    g_free(reply);
    g_free(wa->prompt_copy);
    g_free(wa);
    return NULL;
}

/* ---------- callbacks UI ---------- */

static void start_stream_simulation(AppWidgets *aw, const char *user_text) {
    if (!aw || !aw->alive || aw->in_progress) return;

    gchar *user_line = g_strdup_printf("You: %s\n", user_text);
    append_text(aw->output_view, user_line);
    g_free(user_line);

    // reseta cancellable anterior se sobrou
    if (aw->cancellable) g_clear_object(&aw->cancellable);
    aw->cancellable = g_cancellable_new();
    set_streaming_state(aw, TRUE);

    WorkerArgs *args = g_new0(WorkerArgs, 1);
    args->aw = aw;
    args->prompt_copy = g_strdup(user_text);

    g_thread_new("ganesha-stream", stream_worker, args);
}

static void on_send_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppWidgets *aw = (AppWidgets*)user_data;
    if (!aw || !aw->alive) return;

    const char *txt = gtk_editable_get_text(GTK_EDITABLE(aw->prompt_entry));
    if (txt && *txt) {
        gtk_editable_set_text(GTK_EDITABLE(aw->prompt_entry), "");
        start_stream_simulation(aw, txt);
    }
}

static void on_entry_activate(GtkEntry *entry, gpointer user_data) {
    (void)entry;
    on_send_clicked(NULL, user_data);
}

static void on_stop_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppWidgets *aw = (AppWidgets*)user_data;
    if (aw && aw->in_progress && aw->cancellable) {
        g_cancellable_cancel(aw->cancellable);
        // não limpar aqui; o finish_cb limpa com segurança
    }
}

/* cancelar streams e marcar UI como morta ao fechar a janela */
static void on_window_destroy(GtkWidget *w, gpointer user_data) {
    (void)w;
    AppWidgets *aw = (AppWidgets*)user_data;
    if (!aw) return;
    aw->alive = FALSE;
    if (aw->cancellable)
        g_cancellable_cancel(aw->cancellable);
}

/* ---------- bootstrap ---------- */

static void on_activate(GApplication *app, gpointer user_data) {
    (void)user_data;

    AdwApplicationWindow *win = ADW_APPLICATION_WINDOW(
        adw_application_window_new(GTK_APPLICATION(app))
    );
    gtk_window_set_default_size(GTK_WINDOW(win), 680, 460);
    gtk_window_set_title(GTK_WINDOW(win), "Ganesha");

    AdwHeaderBar *header = ADW_HEADER_BAR(adw_header_bar_new());
    GtkWidget *title = gtk_label_new("Ganesha");
    adw_header_bar_set_title_widget(header, title);

    AdwToolbarView *view = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    adw_toolbar_view_add_top_bar(view, GTK_WIDGET(header));

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 12);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_widget_set_hexpand(scroller, TRUE);

    GtkWidget *output = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(output), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(output), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), output);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type your prompt…");

    GtkWidget *send_btn = gtk_button_new_with_label("Send");
    GtkWidget *stop_btn = gtk_button_new_with_label("Stop");
    gtk_widget_set_sensitive(stop_btn, FALSE);

    gtk_box_append(GTK_BOX(hbox), entry);
    gtk_box_append(GTK_BOX(hbox), send_btn);
    gtk_box_append(GTK_BOX(hbox), stop_btn);

    gtk_box_append(GTK_BOX(vbox), scroller);
    gtk_box_append(GTK_BOX(vbox), hbox);

    AppWidgets *aw = g_new0(AppWidgets, 1);
    aw->output_view  = GTK_TEXT_VIEW(output);
    aw->prompt_entry = GTK_ENTRY(entry);
    aw->send_btn     = GTK_BUTTON(send_btn);
    aw->stop_btn     = GTK_BUTTON(stop_btn);
    aw->cancellable  = NULL;
    aw->in_progress  = FALSE;
    aw->alive        = TRUE;

    g_signal_connect(send_btn, "clicked",  G_CALLBACK(on_send_clicked), aw);
    g_signal_connect(stop_btn, "clicked",  G_CALLBACK(on_stop_clicked), aw);
    g_signal_connect(entry,    "activate", G_CALLBACK(on_entry_activate), aw);
    g_signal_connect(win,      "destroy",  G_CALLBACK(on_window_destroy), aw);

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
