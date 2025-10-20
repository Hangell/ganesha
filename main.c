#include <adwaita.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

/* ================== Config ================== */
static const char *OLLAMA_BASE_URL = "http://192.168.0.3:11434";
static const char *DEFAULT_MODEL   = "llama3.2:3b";
static const int   REQUEST_TIMEOUT = 300;
static const char *PREFS_FILE      = "ganesha-prefs.json";
/* ============================================ */

typedef struct {
    GtkTextView   *output_view;
    GtkEntry      *prompt_entry;
    GtkButton     *send_btn;
    GtkButton     *stop_btn;
    GtkSpinner    *spinner;
    GtkDropDown   *model_dropdown;

    GCancellable  *cancellable;
    gboolean       in_progress;
    gboolean       alive;
    
    GListStore    *models_store;
    gchar         *selected_model;
} AppWidgets;

/* ---------- Preferences ---------- */

static gchar* get_prefs_path(void) {
    const gchar *config_dir = g_get_user_config_dir();
    gchar *app_dir = g_build_filename(config_dir, "ganesha", NULL);
    g_mkdir_with_parents(app_dir, 0755);
    gchar *path = g_build_filename(app_dir, PREFS_FILE, NULL);
    g_free(app_dir);
    return path;
}

static gchar* load_preferred_model(void) {
    gchar *path = get_prefs_path();
    gchar *contents = NULL;
    gchar *model = NULL;
    
    if (g_file_get_contents(path, &contents, NULL, NULL)) {
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, contents, -1, NULL)) {
            JsonNode *root = json_parser_get_root(parser);
            if (JSON_NODE_HOLDS_OBJECT(root)) {
                JsonObject *obj = json_node_get_object(root);
                if (json_object_has_member(obj, "preferred_model")) {
                    model = g_strdup(json_object_get_string_member(obj, "preferred_model"));
                }
            }
        }
        g_object_unref(parser);
        g_free(contents);
    }
    
    g_free(path);
    return model ? model : g_strdup(DEFAULT_MODEL);
}

static void save_preferred_model(const gchar *model) {
    if (!model) return;
    
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "preferred_model");
    json_builder_add_string_value(builder, model);
    json_builder_end_object(builder);
    
    JsonGenerator *gen = json_generator_new();
    json_generator_set_pretty(gen, TRUE);
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);
    
    gchar *json_data = json_generator_to_data(gen, NULL);
    gchar *path = get_prefs_path();
    
    g_file_set_contents(path, json_data, -1, NULL);
    
    g_free(json_data);
    g_free(path);
    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(builder);
}

/* ---------- helpers UI ---------- */

static void append_text(GtkTextView *view, const char *text) {
    if (!view) return;
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
    if (aw->send_btn) gtk_widget_set_sensitive(GTK_WIDGET(aw->send_btn), !running);
    if (aw->stop_btn) gtk_widget_set_sensitive(GTK_WIDGET(aw->stop_btn),  running);
    if (aw->prompt_entry) gtk_widget_set_sensitive(GTK_WIDGET(aw->prompt_entry), !running);
    if (aw->model_dropdown) gtk_widget_set_sensitive(GTK_WIDGET(aw->model_dropdown), !running);
    
    if (aw->spinner) {
        if (running) {
            gtk_spinner_start(aw->spinner);
            gtk_widget_set_visible(GTK_WIDGET(aw->spinner), TRUE);
        } else {
            gtk_spinner_stop(aw->spinner);
            gtk_widget_set_visible(GTK_WIDGET(aw->spinner), FALSE);
        }
    }
}

/* ---------- callbacks postados no main loop ---------- */

typedef struct {
    AppWidgets *aw;
    char       *chunk;
} AppendChunkData;

static gboolean ui_append_chunk_cb(gpointer data) {
    AppendChunkData *d = (AppendChunkData*)data;
    if (d->aw && d->aw->alive) append_text(d->aw->output_view, d->chunk);
    g_free(d->chunk);
    g_free(d);
    return G_SOURCE_REMOVE;
}

static gboolean ui_append_assistant_prefix_cb(gpointer data) {
    AppWidgets *aw = (AppWidgets*)data;
    if (aw && aw->alive) append_text(aw->output_view, "Assistant: ");
    return G_SOURCE_REMOVE;
}

static gboolean ui_finish_stream_cb(gpointer data) {
    AppWidgets *aw = (AppWidgets*)data;
    if (aw && aw->alive) {
        append_text(aw->output_view, "\n");
        set_streaming_state(aw, FALSE);
    }
    if (aw && aw->cancellable) g_clear_object(&aw->cancellable);
    return G_SOURCE_REMOVE;
}

/* ---------- Model Loading ---------- */

typedef struct {
    AppWidgets *aw;
    GPtrArray  *model_names;
} ModelsLoadedData;

static gboolean ui_models_loaded_cb(gpointer data) {
    ModelsLoadedData *mld = (ModelsLoadedData*)data;
    AppWidgets *aw = mld->aw;
    
    if (!aw || !aw->alive || !aw->models_store) {
        g_ptr_array_unref(mld->model_names);
        g_free(mld);
        return G_SOURCE_REMOVE;
    }
    
    g_list_store_remove_all(aw->models_store);
    
    guint selected_idx = 0;
    for (guint i = 0; i < mld->model_names->len; i++) {
        const gchar *name = g_ptr_array_index(mld->model_names, i);
        GtkStringObject *str_obj = gtk_string_object_new(name);
        g_list_store_append(aw->models_store, str_obj);
        g_object_unref(str_obj);
        
        if (aw->selected_model && g_strcmp0(name, aw->selected_model) == 0) {
            selected_idx = i;
        }
    }
    
    if (mld->model_names->len > 0) {
        gtk_drop_down_set_selected(aw->model_dropdown, selected_idx);
    }
    
    g_ptr_array_unref(mld->model_names);
    g_free(mld);
    return G_SOURCE_REMOVE;
}

static gpointer load_models_worker(gpointer data) {
    AppWidgets *aw = (AppWidgets*)data;
    
    if (!aw || !aw->alive) return NULL;
    
    gchar *url = g_strdup_printf("%s/api/tags", OLLAMA_BASE_URL);
    
    SoupSession *session = soup_session_new();
    g_object_set(session, "timeout", 10, NULL);
    
    SoupMessage *msg = soup_message_new("GET", url);
    GError *err = NULL;
    GBytes *response_bytes = soup_session_send_and_read(session, msg, NULL, &err);
    
    GPtrArray *model_names = g_ptr_array_new_with_free_func(g_free);
    
    if (response_bytes && !err) {
        gsize size;
        gconstpointer data_ptr = g_bytes_get_data(response_bytes, &size);
        
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, data_ptr, size, NULL)) {
            JsonNode *root = json_parser_get_root(parser);
            if (JSON_NODE_HOLDS_OBJECT(root)) {
                JsonObject *obj = json_node_get_object(root);
                if (json_object_has_member(obj, "models")) {
                    JsonArray *models = json_object_get_array_member(obj, "models");
                    guint len = json_array_get_length(models);
                    
                    for (guint i = 0; i < len; i++) {
                        JsonObject *model = json_array_get_object_element(models, i);
                        if (json_object_has_member(model, "name")) {
                            const gchar *name = json_object_get_string_member(model, "name");
                            g_ptr_array_add(model_names, g_strdup(name));
                        }
                    }
                }
            }
        }
        g_object_unref(parser);
        g_bytes_unref(response_bytes);
    }
    
    if (err) g_error_free(err);
    g_object_unref(session);
    g_object_unref(msg);
    g_free(url);
    
    // Se não encontrou modelos, adiciona o padrão
    if (model_names->len == 0) {
        g_ptr_array_add(model_names, g_strdup(DEFAULT_MODEL));
    }
    
    ModelsLoadedData *mld = g_new0(ModelsLoadedData, 1);
    mld->aw = aw;
    mld->model_names = model_names;
    
    g_idle_add(ui_models_loaded_cb, mld);
    
    return NULL;
}

/* ---------- worker: Ollama streaming ---------- */

typedef struct {
    AppWidgets *aw;
    char       *prompt_copy;
    char       *model_copy;
} WorkerArgs;

static gchar *build_ollama_chat_body(const char *model, const char *user_text) {
    JsonBuilder *b = json_builder_new();
    json_builder_begin_object(b);

    json_builder_set_member_name(b, "model");
    json_builder_add_string_value(b, model);

    json_builder_set_member_name(b, "stream");
    json_builder_add_boolean_value(b, TRUE);

    json_builder_set_member_name(b, "messages");
    json_builder_begin_array(b);
      json_builder_begin_object(b);
        json_builder_set_member_name(b, "role");
        json_builder_add_string_value(b, "user");
        json_builder_set_member_name(b, "content");
        json_builder_add_string_value(b, user_text);
      json_builder_end_object(b);
    json_builder_end_array(b);

    json_builder_end_object(b);

    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(b);
    json_generator_set_root(gen, root);

    gsize len = 0;
    gchar *data = json_generator_to_data(gen, &len);

    g_object_unref(gen);
    json_node_free(root);
    g_object_unref(b);

    return data;
}

static const char* extract_chunk_text(JsonNode *root) {
    if (!JSON_NODE_HOLDS_OBJECT(root)) return NULL;
    JsonObject *obj = json_node_get_object(root);

    if (json_object_has_member(obj, "message")) {
        JsonObject *msg = json_object_get_object_member(obj, "message");
        if (msg && json_object_has_member(msg, "content")) {
            return json_object_get_string_member(msg, "content");
        }
    }
    if (json_object_has_member(obj, "response")) {
        return json_object_get_string_member(obj, "response");
    }
    return NULL;
}

static gboolean chunk_is_done(JsonNode *root) {
    if (!JSON_NODE_HOLDS_OBJECT(root)) return FALSE;
    JsonObject *obj = json_node_get_object(root);
    if (json_object_has_member(obj, "done")) {
        return json_object_get_boolean_member(obj, "done");
    }
    return FALSE;
}

static gpointer ollama_stream_worker(gpointer data) {
    WorkerArgs *wa = (WorkerArgs*)data;
    AppWidgets *aw = wa->aw;

    if (!aw || !aw->alive) {
        g_free(wa->prompt_copy);
        g_free(wa->model_copy);
        g_free(wa);
        return NULL;
    }

    GCancellable *c = aw->cancellable;
    g_idle_add(ui_append_assistant_prefix_cb, aw);

    gchar *url = g_strdup_printf("%s/api/chat", OLLAMA_BASE_URL);
    gchar *body_json = build_ollama_chat_body(wa->model_copy, wa->prompt_copy);

    SoupSession *session = soup_session_new();
    g_object_set(session, "timeout", REQUEST_TIMEOUT, NULL);
    
    SoupMessage *msg = soup_message_new("POST", url);
    soup_message_headers_append(soup_message_get_request_headers(msg), "Content-Type", "application/json");
    GBytes *body = g_bytes_new_take(body_json, strlen(body_json));
    soup_message_set_request_body_from_bytes(msg, "application/json", body);
    g_bytes_unref(body);

    GError *err = NULL;
    GInputStream *stream = soup_session_send(session, msg, c, &err);
    if (!stream) {
        AppendChunkData *chunk = g_new0(AppendChunkData, 1);
        chunk->aw = aw;
        chunk->chunk = g_strdup_printf("[network error] %s\n", err ? err->message : "unknown");
        g_idle_add(ui_append_chunk_cb, chunk);
        if (err) g_error_free(err);
        g_object_unref(session);
        g_object_unref(msg);
        g_free(url);
        g_free(wa->prompt_copy);
        g_free(wa->model_copy);
        g_free(wa);
        g_idle_add(ui_finish_stream_cb, aw);
        return NULL;
    }

    GDataInputStream *din = g_data_input_stream_new(stream);
    g_data_input_stream_set_newline_type(din, G_DATA_STREAM_NEWLINE_TYPE_ANY);

    while (aw->alive && !g_cancellable_is_cancelled(c)) {
        gsize len = 0;
        gchar *line = g_data_input_stream_read_line_utf8(din, &len, c, &err);
        if (!line) break;
        if (len == 0) {
            g_free(line);
            continue;
        }

        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, line, -1, NULL)) {
            JsonNode *root = json_parser_get_root(parser);

            const char *delta = extract_chunk_text(root);
            if (delta && *delta) {
                AppendChunkData *chunk = g_new0(AppendChunkData, 1);
                chunk->aw = aw;
                chunk->chunk = g_strdup(delta);
                g_idle_add(ui_append_chunk_cb, chunk);
            }

            if (chunk_is_done(root)) {
                g_object_unref(parser);
                g_free(line);
                break;
            }
        }
        g_object_unref(parser);
        g_free(line);
    }

    g_idle_add(ui_finish_stream_cb, aw);

    g_object_unref(din);
    g_object_unref(stream);
    g_object_unref(session);
    g_object_unref(msg);
    g_free(url);
    g_free(wa->prompt_copy);
    g_free(wa->model_copy);
    g_free(wa);
    return NULL;
}

/* ---------- callbacks UI ---------- */

static void start_ollama_stream(AppWidgets *aw, const char *user_text) {
    if (!aw || !aw->alive || aw->in_progress) return;

    gchar *user_line = g_strdup_printf("You: %s\n", user_text);
    append_text(aw->output_view, user_line);
    g_free(user_line);

    if (aw->cancellable) g_clear_object(&aw->cancellable);
    aw->cancellable = g_cancellable_new();
    set_streaming_state(aw, TRUE);

    WorkerArgs *args = g_new0(WorkerArgs, 1);
    args->aw = aw;
    args->prompt_copy = g_strdup(user_text);
    args->model_copy = g_strdup(aw->selected_model ? aw->selected_model : DEFAULT_MODEL);

    g_thread_new("ganesha-ollama", ollama_stream_worker, args);
}

static void on_send_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppWidgets *aw = (AppWidgets*)user_data;
    if (!aw || !aw->alive) return;

    const char *txt = gtk_editable_get_text(GTK_EDITABLE(aw->prompt_entry));
    if (txt && *txt) {
        start_ollama_stream(aw, txt);
        gtk_editable_set_text(GTK_EDITABLE(aw->prompt_entry), "");
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
    }
}

static void on_model_selected(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    AppWidgets *aw = (AppWidgets*)user_data;
    if (!aw) return;
    
    GtkStringObject *str_obj = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(dropdown));
    if (str_obj) {
        const gchar *model_name = gtk_string_object_get_string(str_obj);
        g_free(aw->selected_model);
        aw->selected_model = g_strdup(model_name);
        save_preferred_model(model_name);
    }
}

static void on_window_destroy(GtkWidget *w, gpointer user_data) {
    (void)w;
    AppWidgets *aw = (AppWidgets*)user_data;
    if (!aw) return;
    aw->alive = FALSE;
    if (aw->cancellable) g_cancellable_cancel(aw->cancellable);
    g_free(aw->selected_model);
}

/* ---------- bootstrap ---------- */

static void on_activate(GApplication *app, gpointer user_data) {
    (void)user_data;

    AdwApplicationWindow *win = ADW_APPLICATION_WINDOW(
        adw_application_window_new(GTK_APPLICATION(app))
    );
    gtk_window_set_default_size(GTK_WINDOW(win), 720, 500);
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

    // Model selector row
    GtkWidget *model_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *model_label = gtk_label_new("Model:");
    gtk_widget_add_css_class(model_label, "dim-label");
    
    GListStore *models_store = g_list_store_new(GTK_TYPE_STRING_OBJECT);
    GtkWidget *model_dropdown = gtk_drop_down_new(G_LIST_MODEL(models_store), NULL);
    gtk_widget_set_hexpand(model_dropdown, TRUE);
    
    gtk_box_append(GTK_BOX(model_hbox), model_label);
    gtk_box_append(GTK_BOX(model_hbox), model_dropdown);

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

    GtkWidget *spinner = gtk_spinner_new();
    gtk_widget_set_visible(spinner, FALSE);

    GtkWidget *send_btn = gtk_button_new_with_label("Send");
    gtk_widget_add_css_class(send_btn, "suggested-action");
    
    GtkWidget *stop_btn = gtk_button_new_with_label("Stop");
    gtk_widget_add_css_class(stop_btn, "destructive-action");
    gtk_widget_set_sensitive(stop_btn, FALSE);

    gtk_box_append(GTK_BOX(hbox), entry);
    gtk_box_append(GTK_BOX(hbox), spinner);
    gtk_box_append(GTK_BOX(hbox), send_btn);
    gtk_box_append(GTK_BOX(hbox), stop_btn);

    gtk_box_append(GTK_BOX(vbox), model_hbox);
    gtk_box_append(GTK_BOX(vbox), scroller);
    gtk_box_append(GTK_BOX(vbox), hbox);

    AppWidgets *aw = g_new0(AppWidgets, 1);
    aw->output_view  = GTK_TEXT_VIEW(output);
    aw->prompt_entry = GTK_ENTRY(entry);
    aw->send_btn     = GTK_BUTTON(send_btn);
    aw->stop_btn     = GTK_BUTTON(stop_btn);
    aw->spinner      = GTK_SPINNER(spinner);
    aw->model_dropdown = GTK_DROP_DOWN(model_dropdown);
    aw->models_store = models_store;
    aw->cancellable  = NULL;
    aw->in_progress  = FALSE;
    aw->alive        = TRUE;
    aw->selected_model = load_preferred_model();

    g_signal_connect(send_btn, "clicked",  G_CALLBACK(on_send_clicked), aw);
    g_signal_connect(stop_btn, "clicked",  G_CALLBACK(on_stop_clicked), aw);
    g_signal_connect(entry,    "activate", G_CALLBACK(on_entry_activate), aw);
    g_signal_connect(model_dropdown, "notify::selected", G_CALLBACK(on_model_selected), aw);
    g_signal_connect(win,      "destroy",  G_CALLBACK(on_window_destroy), aw);

    adw_toolbar_view_set_content(view, vbox);
    adw_application_window_set_content(win, GTK_WIDGET(view));
    gtk_window_present(GTK_WINDOW(win));
    
    // Load models in background
    g_thread_new("ganesha-models", load_models_worker, aw);
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