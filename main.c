#include <adwaita.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

/* ================== Config ================== */
static const char *OLLAMA_BASE_URL = "http://192.168.0.3:11434";
static const char *DEFAULT_MODEL   = "llama3.2:3b";
static const int   REQUEST_TIMEOUT = 300;
static const char *PREFS_FILE      = "ganesha-prefs.json";
static const char *CONVERSATIONS_FILE = "ganesha-conversations.json";
/* ============================================ */

typedef struct {
  gchar *role;
  gchar *content;
} Message;

typedef struct {
  gchar *id;
  gchar *title;
  GPtrArray *messages;
  gint64 timestamp;
} Conversation;

typedef struct {
  GtkBox        *chat_box;
  GtkEntry      *prompt_entry;
  GtkButton     *send_btn;
  GtkButton     *stop_btn;
  GtkButton     *new_chat_btn;
  GtkSpinner    *spinner;
  GtkDropDown   *model_dropdown;
  GtkListBox    *conversations_list;
  GtkScrolledWindow *chat_scroller;
  GCancellable  *cancellable;
  gboolean       in_progress;
  gboolean       alive;
  
  GListStore    *models_store;
  gchar         *selected_model;
  
  GPtrArray     *conversations;
  Conversation  *current_conversation;
  
  GtkWidget     *current_assistant_box;
  GtkWidget     *theme_btn;
  gboolean       dark_theme;
} AppWidgets;

/* ---------- CSS Styling ---------- */

static const char *DARK_CSS = 
".background { background-color: #0f1115; color: #e6e6e6; }"
".chat-container { background-color: #0f1115; }"
".message-bubble {"
"  padding: 12px 16px;"
"  margin: 8px 16px;"
"  border-radius: 18px;"
"  max-width: 68%;"
"}"
".user-message {"
"  background: linear-gradient(135deg, #5b6cff 0%, #7b61ff 100%);"
"  color: #ffffff;"
"  margin-left: auto;"
"  margin-right: 16px;"
"  box-shadow: 0 6px 18px rgba(91, 108, 255, 0.25);"
"}"
".assistant-message {"
"  background-color: #171a21;"
"  color: #e0e6f1;"
"  margin-right: auto;"
"  margin-left: 16px;"
"  border: 1px solid #242a33;"
"  box-shadow: 0 4px 14px rgba(0,0,0,0.35);"
"}"
".message-content {"
"  font-size: 14px;"
"  line-height: 1.55;"
"}"
".sidebar-header {"
"  background-color: #0b0d11;"
"  color: #e6e6e6;"
"  padding: 12px;"
"  border-bottom: 1px solid #232833;"
"}"
".conversation-item {"
"  padding: 12px 16px;"
"  border-bottom: 1px solid #161a20;"
"  color: #e6e6e6;"
"  transition: background-color 0.2s, transform 0.05s ease-in-out;"
"}"
".conversation-item:hover {"
"  background-color: #12151b;"
"}"
".conversation-item:selected {"
"  background-color: #1a1f27;"
"  color: #e6e6e6;"
"}"
"entry {"
"  background: #0f131a;"
"  color: #e6e6e6;"
"  border: 1px solid #232833;"
"  border-radius: 12px;"
"  padding: 8px 10px;"
"}"
"scrollbar slider {"
"  background: #2a2f38;"
"  border-radius: 6px;"
"}"
"scrollbar slider:hover {"
"  background: #353b46;"
"}"
"button.suggested-action {"
"  background: linear-gradient(135deg, #5b6cff 0%, #7b61ff 100%);"
"  color: #fff;"
"  border-radius: 12px;"
"}"
"button.destructive-action {"
"  background: #2a1a1b;"
"  color: #ffb9b9;"
"  border: 1px solid #4a2a2c;"
"  border-radius: 12px;"
"}"
"label {"
"  color: #e6e6e6;"
"}"
".dim-label {"
"  color: #a0a0a0;"
"}"
".title-4 {"
"  color: #e6e6e6;"
"  font-weight: 600;"
"}"
".navigation-sidebar {"
"  background-color: #0f1115;"
"}";

static const char *LIGHT_CSS = 
".background { background-color: #ffffff; color: #333333; }"
".chat-container { background-color: #ffffff; }"
".message-bubble {"
"  padding: 12px 16px;"
"  margin: 8px 16px;"
"  border-radius: 18px;"
"  max-width: 68%;"
"}"
".user-message {"
"  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);"
"  color: white;"
"  margin-left: auto;"
"  margin-right: 16px;"
"  box-shadow: 0 6px 18px rgba(102, 126, 234, 0.25);"
"}"
".assistant-message {"
"  background-color: #f8f9fa;"
"  color: #333333;"
"  margin-right: auto;"
"  margin-left: 16px;"
"  border: 1px solid #e9ecef;"
"  box-shadow: 0 4px 14px rgba(0,0,0,0.08);"
"}"
".message-content {"
"  font-size: 14px;"
"  line-height: 1.55;"
"}"
".sidebar-header {"
"  background-color: #f8f9fa;"
"  color: #333333;"
"  padding: 12px;"
"  border-bottom: 1px solid #dee2e6;"
"}"
".conversation-item {"
"  padding: 12px 16px;"
"  border-bottom: 1px solid #f1f3f4;"
"  color: #333333;"
"  transition: background-color 0.2s, transform 0.05s ease-in-out;"
"}"
".conversation-item:hover {"
"  background-color: #e9ecef;"
"}"
".conversation-item:selected {"
"  background-color: #dee2e6;"
"  color: #333333;"
"}"
"entry {"
"  background: #ffffff;"
"  color: #333333;"
"  border: 1px solid #ced4da;"
"  border-radius: 12px;"
"  padding: 8px 10px;"
"}"
"scrollbar slider {"
"  background: #adb5bd;"
"  border-radius: 6px;"
"}"
"scrollbar slider:hover {"
"  background: #6c757d;"
"}"
"button.suggested-action {"
"  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);"
"  color: #fff;"
"  border-radius: 12px;"
"}"
"button.destructive-action {"
"  background: #f8d7da;"
"  color: #721c24;"
"  border: 1px solid #f5c6cb;"
"  border-radius: 12px;"
"}"
"label {"
"  color: #333333;"
"}"
".dim-label {"
"  color: #6c757d;"
"}"
".title-4 {"
"  color: #333333;"
"  font-weight: 600;"
"}"
".navigation-sidebar {"
"  background-color: #ffffff;"
"}";

/* ---------- Message/Conversation helpers ---------- */

static Message* message_new(const gchar *role, const gchar *content) {
  Message *msg = g_new0(Message, 1);
  msg->role = g_strdup(role);
  msg->content = g_strdup(content);
  return msg;
}

static void message_free(Message *msg) {
  if (!msg) return;
  g_free(msg->role);
  g_free(msg->content);
  g_free(msg);
}

static Conversation* conversation_new(void) {
  Conversation *conv = g_new0(Conversation, 1);
  conv->id = g_uuid_string_random();
  conv->title = NULL;
  conv->messages = g_ptr_array_new_with_free_func((GDestroyNotify)message_free);
  conv->timestamp = g_get_real_time();
  return conv;
}

static void conversation_free(Conversation *conv) {
  if (!conv) return;
  g_free(conv->id);
  g_free(conv->title);
  g_ptr_array_unref(conv->messages);
  g_free(conv);
}

static void conversation_add_message(Conversation *conv, const gchar *role, const gchar *content) {
  if (!conv) return;
  Message *msg = message_new(role, content);
  g_ptr_array_add(conv->messages, msg);
  
  if (!conv->title && g_strcmp0(role, "user") == 0) {
      gchar *title = g_strdup(content);
      if (strlen(title) > 50) {
          title[47] = '.';
          title[48] = '.';
          title[49] = '.';
          title[50] = '\0';
      }
      conv->title = title;
  }
}

/* ---------- Persistence ---------- */

static gchar* get_conversations_path(void) {
  const gchar *config_dir = g_get_user_config_dir();
  gchar *app_dir = g_build_filename(config_dir, "ganesha", NULL);
  g_mkdir_with_parents(app_dir, 0755);
  gchar *path = g_build_filename(app_dir, CONVERSATIONS_FILE, NULL);
  g_free(app_dir);
  return path;
}

static void save_conversations(AppWidgets *aw) {
  if (!aw || !aw->conversations) return;
  
  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "conversations");
  json_builder_begin_array(builder);
  
  for (guint i = 0; i < aw->conversations->len; i++) {
      Conversation *conv = g_ptr_array_index(aw->conversations, i);
      json_builder_begin_object(builder);
      
      json_builder_set_member_name(builder, "id");
      json_builder_add_string_value(builder, conv->id);
      
      json_builder_set_member_name(builder, "title");
      json_builder_add_string_value(builder, conv->title ? conv->title : "New Chat");
      
      json_builder_set_member_name(builder, "timestamp");
      json_builder_add_int_value(builder, conv->timestamp);
      
      json_builder_set_member_name(builder, "messages");
      json_builder_begin_array(builder);
      
      for (guint j = 0; j < conv->messages->len; j++) {
          Message *msg = g_ptr_array_index(conv->messages, j);
          json_builder_begin_object(builder);
          json_builder_set_member_name(builder, "role");
          json_builder_add_string_value(builder, msg->role);
          json_builder_set_member_name(builder, "content");
          json_builder_add_string_value(builder, msg->content);
          json_builder_end_object(builder);
      }
      
      json_builder_end_array(builder);
      json_builder_end_object(builder);
  }
  
  json_builder_end_array(builder);
  json_builder_end_object(builder);
  
  JsonGenerator *gen = json_generator_new();
  json_generator_set_pretty(gen, TRUE);
  JsonNode *root = json_builder_get_root(builder);
  json_generator_set_root(gen, root);
  
  gchar *json_data = json_generator_to_data(gen, NULL);
  gchar *path = get_conversations_path();
  
  g_file_set_contents(path, json_data, -1, NULL);
  
  g_free(json_data);
  g_free(path);
  json_node_free(root);
  g_object_unref(gen);
  g_object_unref(builder);
}

static void load_conversations(AppWidgets *aw) {
  if (!aw) return;
  
  gchar *path = get_conversations_path();
  gchar *contents = NULL;
  
  if (!g_file_get_contents(path, &contents, NULL, NULL)) {
      g_free(path);
      return;
  }
  
  JsonParser *parser = json_parser_new();
  if (!json_parser_load_from_data(parser, contents, -1, NULL)) {
      g_object_unref(parser);
      g_free(contents);
      g_free(path);
      return;
  }
  
  JsonNode *root = json_parser_get_root(parser);
  if (!JSON_NODE_HOLDS_OBJECT(root)) {
      g_object_unref(parser);
      g_free(contents);
      g_free(path);
      return;
  }
  
  JsonObject *obj = json_node_get_object(root);
  if (!json_object_has_member(obj, "conversations")) {
      g_object_unref(parser);
      g_free(contents);
      g_free(path);
      return;
  }
  
  JsonArray *convs_array = json_object_get_array_member(obj, "conversations");
  guint len = json_array_get_length(convs_array);
  
  for (guint i = 0; i < len; i++) {
      JsonObject *conv_obj = json_array_get_object_element(convs_array, i);
      
      Conversation *conv = g_new0(Conversation, 1);
      conv->id = g_strdup(json_object_get_string_member(conv_obj, "id"));
      conv->title = g_strdup(json_object_get_string_member(conv_obj, "title"));
      conv->timestamp = json_object_get_int_member(conv_obj, "timestamp");
      conv->messages = g_ptr_array_new_with_free_func((GDestroyNotify)message_free);
      
      JsonArray *msgs_array = json_object_get_array_member(conv_obj, "messages");
      guint msgs_len = json_array_get_length(msgs_array);
      
      for (guint j = 0; j < msgs_len; j++) {
          JsonObject *msg_obj = json_array_get_object_element(msgs_array, j);
          Message *msg = message_new(
              json_object_get_string_member(msg_obj, "role"),
              json_object_get_string_member(msg_obj, "content")
          );
          g_ptr_array_add(conv->messages, msg);
      }
      
      g_ptr_array_add(aw->conversations, conv);
  }
  
  g_object_unref(parser);
  g_free(contents);
  g_free(path);
}

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

static gboolean load_theme_preference(void) {
  gchar *path = get_prefs_path();
  gchar *contents = NULL;
  gboolean dark_theme = TRUE; // Default to dark theme
  
  if (g_file_get_contents(path, &contents, NULL, NULL)) {
      JsonParser *parser = json_parser_new();
      if (json_parser_load_from_data(parser, contents, -1, NULL)) {
          JsonNode *root = json_parser_get_root(parser);
          if (JSON_NODE_HOLDS_OBJECT(root)) {
              JsonObject *obj = json_node_get_object(root);
              if (json_object_has_member(obj, "dark_theme")) {
                  dark_theme = json_object_get_boolean_member(obj, "dark_theme");
              }
          }
      }
      g_object_unref(parser);
      g_free(contents);
  }
  
  g_free(path);
  return dark_theme;
}

static void save_theme_preference(gboolean dark_theme) {
  gchar *path = get_prefs_path();
  gchar *contents = NULL;
  JsonParser *parser = json_parser_new();
  JsonNode *root = NULL;
  
  // Load existing preferences
  if (g_file_get_contents(path, &contents, NULL, NULL)) {
      if (json_parser_load_from_data(parser, contents, -1, NULL)) {
          root = json_parser_get_root(parser);
          if (root) json_node_ref(root);
      }
      g_free(contents);
  }
  
  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);
  
  // Copy existing preferences
  if (root && JSON_NODE_HOLDS_OBJECT(root)) {
      JsonObject *obj = json_node_get_object(root);
      GList *members = json_object_get_members(obj);
      for (GList *l = members; l; l = l->next) {
          const gchar *key = l->data;
          if (g_strcmp0(key, "dark_theme") != 0) {
              json_builder_set_member_name(builder, key);
              JsonNode *value = json_object_get_member(obj, key);
              json_builder_add_value(builder, value);
          }
      }
      g_list_free(members);
  }
  
  // Add theme preference
  json_builder_set_member_name(builder, "dark_theme");
  json_builder_add_boolean_value(builder, dark_theme);
  
  json_builder_end_object(builder);
  
  JsonGenerator *gen = json_generator_new();
  json_generator_set_pretty(gen, TRUE);
  JsonNode *new_root = json_builder_get_root(builder);
  json_generator_set_root(gen, new_root);
  
  gchar *json_data = json_generator_to_data(gen, NULL);
  g_file_set_contents(path, json_data, -1, NULL);
  
  g_free(json_data);
  g_free(path);
  if (root) json_node_unref(root);
  json_node_free(new_root);
  g_object_unref(gen);
  g_object_unref(builder);
  g_object_unref(parser);
}

/* ---------- UI Message Bubbles ---------- */

static GtkWidget* create_message_bubble(const gchar *role, const gchar *content) {
  GtkWidget *bubble = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(bubble, "message-bubble");
  
  if (g_strcmp0(role, "user") == 0) {
      gtk_widget_add_css_class(bubble, "user-message");
      gtk_widget_set_halign(bubble, GTK_ALIGN_END);
  } else {
      gtk_widget_add_css_class(bubble, "assistant-message");
      gtk_widget_set_halign(bubble, GTK_ALIGN_START);
  }
  
  GtkWidget *label = gtk_label_new(content);
  gtk_label_set_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);
  gtk_widget_add_css_class(label, "message-content");
  
  gtk_box_append(GTK_BOX(bubble), label);
  
  return bubble;
}

static void append_message_bubble(AppWidgets *aw, const gchar *role, const gchar *content) {
  if (!aw || !aw->chat_box) return;
  
  GtkWidget *bubble = create_message_bubble(role, content);
  gtk_box_append(aw->chat_box, bubble);
  
  // Auto-scroll to bottom
  GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(aw->chat_scroller);
  gtk_adjustment_set_value(vadj, gtk_adjustment_get_upper(vadj));
}

static void clear_chat_display(AppWidgets *aw) {
  if (!aw || !aw->chat_box) return;
  
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child(GTK_WIDGET(aw->chat_box)))) {
      gtk_box_remove(aw->chat_box, child);
  }
}

static void display_conversation(AppWidgets *aw, Conversation *conv) {
  if (!aw || !conv) return;
  
  clear_chat_display(aw);
  
  for (guint i = 0; i < conv->messages->len; i++) {
      Message *msg = g_ptr_array_index(conv->messages, i);
      append_message_bubble(aw, msg->role, msg->content);
  }
}

static void set_streaming_state(AppWidgets *aw, gboolean running) {
  if (!aw || !aw->alive) return;
  aw->in_progress = running;
  if (aw->send_btn) gtk_widget_set_sensitive(GTK_WIDGET(aw->send_btn), !running);
  if (aw->stop_btn) gtk_widget_set_sensitive(GTK_WIDGET(aw->stop_btn),  running);
  if (aw->prompt_entry) gtk_widget_set_sensitive(GTK_WIDGET(aw->prompt_entry), !running);
  if (aw->model_dropdown) gtk_widget_set_sensitive(GTK_WIDGET(aw->model_dropdown), !running);
  if (aw->new_chat_btn) gtk_widget_set_sensitive(GTK_WIDGET(aw->new_chat_btn), !running);
  
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

/* ---------- Conversations List UI ---------- */

static void update_conversations_list(AppWidgets *aw) {
  if (!aw || !aw->conversations_list) return;
  
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child(GTK_WIDGET(aw->conversations_list)))) {
      gtk_list_box_remove(aw->conversations_list, child);
  }
  
  for (gint i = aw->conversations->len - 1; i >= 0; i--) {
      Conversation *conv = g_ptr_array_index(aw->conversations, i);
      
      GtkWidget *row = gtk_list_box_row_new();
      gtk_widget_add_css_class(row, "conversation-item");
      g_object_set_data(G_OBJECT(row), "conversation", conv);
      
      GtkWidget *label = gtk_label_new(conv->title ? conv->title : "New Chat");
      gtk_widget_set_halign(label, GTK_ALIGN_START);
      gtk_widget_set_margin_start(label, 16);
      gtk_widget_set_margin_end(label, 16);
      gtk_widget_set_margin_top(label, 8);
      gtk_widget_set_margin_bottom(label, 8);
      gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
      
      gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
      gtk_list_box_append(aw->conversations_list, row);
      
      if (conv == aw->current_conversation) {
          gtk_list_box_select_row(aw->conversations_list, GTK_LIST_BOX_ROW(row));
      }
  }
}

/* ---------- Theme Management ---------- */

/* ---------- Theme Management ---------- */

static void apply_theme(AppWidgets *aw, gboolean dark_theme) {
  // Create new provider
  GtkCssProvider *css_provider = gtk_css_provider_new();
  AdwStyleManager *style = adw_style_manager_get_default();
  adw_style_manager_set_color_scheme(style, dark_theme ? ADW_COLOR_SCHEME_FORCE_DARK : ADW_COLOR_SCHEME_FORCE_LIGHT);
  
  if (dark_theme) {
    gtk_css_provider_load_from_string(css_provider, DARK_CSS);
  } else {
    gtk_css_provider_load_from_string(css_provider, LIGHT_CSS);
  }
  
  GdkDisplay *display = gdk_display_get_default();
  
  // Simple approach: just add the new provider
  // GTK will handle the rest automatically
  gtk_style_context_add_provider_for_display(
      display,
      GTK_STYLE_PROVIDER(css_provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
  );
  
  // Force refresh of all widgets by toggling classes
  if (aw->chat_scroller) {
    gtk_widget_remove_css_class(GTK_WIDGET(aw->chat_scroller), "chat-container");
    gtk_widget_add_css_class(GTK_WIDGET(aw->chat_scroller), "chat-container");
  }
  
  // Update theme button icon
  if (aw->theme_btn) {
    if (dark_theme) {
      gtk_button_set_icon_name(GTK_BUTTON(aw->theme_btn), "weather-clear-night-symbolic");
    } else {
      gtk_button_set_icon_name(GTK_BUTTON(aw->theme_btn), "weather-clear-symbolic");
    }
  }
  
  // Force redraw of the entire window
  if (aw->chat_box) {
    gtk_widget_queue_draw(GTK_WIDGET(aw->chat_box));
  }
  if (aw->conversations_list) {
    gtk_widget_queue_draw(GTK_WIDGET(aw->conversations_list));
  }
}

static void on_theme_toggled(GtkButton *btn, gpointer user_data) {
  (void)btn;
  AppWidgets *aw = (AppWidgets*)user_data;
  if (!aw) return;
  
  aw->dark_theme = !aw->dark_theme;
  apply_theme(aw, aw->dark_theme);
  save_theme_preference(aw->dark_theme);
}

/* ---------- callbacks postados no main loop ---------- */

typedef struct {
  AppWidgets *aw;
  char       *chunk;
} AppendChunkData;

static gboolean ui_append_chunk_cb(gpointer data) {
  AppendChunkData *d = (AppendChunkData*)data;
  if (d->aw && d->aw->alive && d->aw->current_assistant_box) {
      GtkWidget *child = gtk_widget_get_first_child(d->aw->current_assistant_box);
      if (GTK_IS_LABEL(child)) {
          const gchar *current = gtk_label_get_text(GTK_LABEL(child));
          gchar *new_text = g_strconcat(current, d->chunk, NULL);
          gtk_label_set_text(GTK_LABEL(child), new_text);
          g_free(new_text);
      }
      
      if (d->aw->current_conversation && d->aw->current_conversation->messages->len > 0) {
          Message *last_msg = g_ptr_array_index(d->aw->current_conversation->messages, 
                                                d->aw->current_conversation->messages->len - 1);
          if (g_strcmp0(last_msg->role, "assistant") == 0) {
              gchar *new_content = g_strconcat(last_msg->content, d->chunk, NULL);
              g_free(last_msg->content);
              last_msg->content = new_content;
          }
      }
      
      GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(d->aw->chat_scroller);
      gtk_adjustment_set_value(vadj, gtk_adjustment_get_upper(vadj));
  }
  g_free(d->chunk);
  g_free(d);
  return G_SOURCE_REMOVE;
}

static gboolean ui_append_assistant_prefix_cb(gpointer data) {
  AppWidgets *aw = (AppWidgets*)data;
  if (aw && aw->alive) {
      GtkWidget *bubble = create_message_bubble("assistant", "");
      gtk_box_append(aw->chat_box, bubble);
      aw->current_assistant_box = bubble;
      
      if (aw->current_conversation) {
          conversation_add_message(aw->current_conversation, "assistant", "");
      }
  }
  return G_SOURCE_REMOVE;
}

static gboolean ui_finish_stream_cb(gpointer data) {
  AppWidgets *aw = (AppWidgets*)data;
  if (aw && aw->alive) {
      aw->current_assistant_box = NULL;
      set_streaming_state(aw, FALSE);
      save_conversations(aw);
      update_conversations_list(aw);
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

static gchar *build_ollama_chat_body(const char *model, Conversation *conv) {
  JsonBuilder *b = json_builder_new();
  json_builder_begin_object(b);
  json_builder_set_member_name(b, "model");
  json_builder_add_string_value(b, model);
  json_builder_set_member_name(b, "stream");
  json_builder_add_boolean_value(b, TRUE);
  json_builder_set_member_name(b, "messages");
  json_builder_begin_array(b);
  
  for (guint i = 0; i < conv->messages->len; i++) {
      Message *msg = g_ptr_array_index(conv->messages, i);
      json_builder_begin_object(b);
      json_builder_set_member_name(b, "role");
      json_builder_add_string_value(b, msg->role);
      json_builder_set_member_name(b, "content");
      json_builder_add_string_value(b, msg->content);
      json_builder_end_object(b);
  }
  
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
  gchar *body_json = build_ollama_chat_body(wa->model_copy, aw->current_conversation);
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
      chunk->chunk = g_strdup_printf("[network error] %s", err ? err->message : "unknown");
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
  
  if (!aw->current_conversation) {
      aw->current_conversation = conversation_new();
      g_ptr_array_add(aw->conversations, aw->current_conversation);
  }
  
  conversation_add_message(aw->current_conversation, "user", user_text);
  append_message_bubble(aw, "user", user_text);
  
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

static void on_new_chat_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  AppWidgets *aw = (AppWidgets*)user_data;
  if (!aw || aw->in_progress) return;
  
  aw->current_conversation = conversation_new();
  g_ptr_array_add(aw->conversations, aw->current_conversation);
  
  clear_chat_display(aw);
  
  update_conversations_list(aw);
  
  save_conversations(aw);
}

static void on_conversation_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
  (void)box;
  AppWidgets *aw = (AppWidgets*)user_data;
  if (!aw || !row || aw->in_progress) return;
  
  Conversation *conv = g_object_get_data(G_OBJECT(row), "conversation");
  if (conv) {
      aw->current_conversation = conv;
      display_conversation(aw, conv);
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
  
  save_conversations(aw);
  
  if (aw->conversations) {
      g_ptr_array_unref(aw->conversations);
  }
  
  g_free(aw->selected_model);
}

/* ---------- bootstrap ---------- */

static void on_activate(GApplication *app, gpointer user_data) {
  (void)user_data;
  
  AdwApplicationWindow *win = ADW_APPLICATION_WINDOW(
      adw_application_window_new(GTK_APPLICATION(app))
  );
  gtk_window_set_default_size(GTK_WINDOW(win), 1000, 700);
  gtk_window_set_title(GTK_WINDOW(win), "Ganesha");
  
  // Header with theme toggle
  AdwHeaderBar *header = ADW_HEADER_BAR(adw_header_bar_new());
  GtkWidget *title = gtk_label_new("Ganesha");
  adw_header_bar_set_title_widget(header, title);
  
  // Theme toggle button
  GtkWidget *theme_btn = gtk_button_new_from_icon_name("weather-clear-night-symbolic");
  gtk_widget_set_tooltip_text(theme_btn, "Toggle dark/light theme");
  adw_header_bar_pack_end(header, theme_btn);
  
  AdwToolbarView *view = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
  adw_toolbar_view_add_top_bar(view, GTK_WIDGET(header));
  
  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  
  // Sidebar with centered header
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request(sidebar, 250, -1);
  
  // Sidebar header with centered title and button
  GtkWidget *sidebar_header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class(sidebar_header, "sidebar-header");
  
  // Conversations title - centered
  GtkWidget *conversations_title = gtk_label_new("Conversations");
  gtk_widget_add_css_class(conversations_title, "title-4");
  gtk_widget_set_halign(conversations_title, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(conversations_title, 12);
  gtk_widget_set_margin_bottom(conversations_title, 8);
  
  // New Chat button - centered
  GtkWidget *new_chat_btn = gtk_button_new_with_label("+ New Chat");
  gtk_widget_add_css_class(new_chat_btn, "suggested-action");
  gtk_widget_set_halign(new_chat_btn, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start(new_chat_btn, 12);
  gtk_widget_set_margin_end(new_chat_btn, 12);
  gtk_widget_set_margin_bottom(new_chat_btn, 12);
  
  gtk_box_append(GTK_BOX(sidebar_header), conversations_title);
  gtk_box_append(GTK_BOX(sidebar_header), new_chat_btn);
  
  GtkWidget *conversations_scroller = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(conversations_scroller, TRUE);
  
  GtkWidget *conversations_list = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(conversations_list), GTK_SELECTION_SINGLE);
  gtk_widget_add_css_class(conversations_list, "navigation-sidebar");
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(conversations_scroller), conversations_list);
  
  gtk_box_append(GTK_BOX(sidebar), sidebar_header);
  gtk_box_append(GTK_BOX(sidebar), conversations_scroller);
  
  // Main content area
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  
  GtkWidget *model_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start(model_hbox, 16);
  gtk_widget_set_margin_end(model_hbox, 16);
  gtk_widget_set_margin_top(model_hbox, 12);
  gtk_widget_set_margin_bottom(model_hbox, 12);
  
  GtkWidget *model_label = gtk_label_new("Model:");
  gtk_widget_add_css_class(model_label, "dim-label");
  
  GListStore *models_store = g_list_store_new(GTK_TYPE_STRING_OBJECT);
  GtkWidget *model_dropdown = gtk_drop_down_new(G_LIST_MODEL(models_store), NULL);
  gtk_widget_set_hexpand(model_dropdown, TRUE);
  
  gtk_box_append(GTK_BOX(model_hbox), model_label);
  gtk_box_append(GTK_BOX(model_hbox), model_dropdown);
  
  GtkWidget *chat_scroller = gtk_scrolled_window_new();
  gtk_widget_set_vexpand(chat_scroller, TRUE);
  gtk_widget_set_hexpand(chat_scroller, TRUE);
  gtk_widget_add_css_class(chat_scroller, "chat-container");
  
  GtkWidget *chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_start(chat_box, 16);
  gtk_widget_set_margin_end(chat_box, 16);
  gtk_widget_set_margin_top(chat_box, 16);
  gtk_widget_set_margin_bottom(chat_box, 16);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(chat_scroller), chat_box);
  
  GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start(input_box, 16);
  gtk_widget_set_margin_end(input_box, 16);
  gtk_widget_set_margin_top(input_box, 12);
  gtk_widget_set_margin_bottom(input_box, 16);
  
  GtkWidget *entry = gtk_entry_new();
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type your message...");
  
  GtkWidget *spinner = gtk_spinner_new();
  gtk_widget_set_visible(spinner, FALSE);
  
  GtkWidget *send_btn = gtk_button_new_with_label("Send");
  gtk_widget_add_css_class(send_btn, "suggested-action");
  
  GtkWidget *stop_btn = gtk_button_new_with_label("Stop");
  gtk_widget_add_css_class(stop_btn, "destructive-action");
  gtk_widget_set_sensitive(stop_btn, FALSE);
  
  gtk_box_append(GTK_BOX(input_box), entry);
  gtk_box_append(GTK_BOX(input_box), spinner);
  gtk_box_append(GTK_BOX(input_box), send_btn);
  gtk_box_append(GTK_BOX(input_box), stop_btn);
  
  gtk_box_append(GTK_BOX(vbox), model_hbox);
  gtk_box_append(GTK_BOX(vbox), chat_scroller);
  gtk_box_append(GTK_BOX(vbox), input_box);
  
  gtk_paned_set_start_child(GTK_PANED(paned), sidebar);
  gtk_paned_set_end_child(GTK_PANED(paned), vbox);
  gtk_paned_set_position(GTK_PANED(paned), 250);
  gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
  gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);
  
  // Initialize app widgets
  AppWidgets *aw = g_new0(AppWidgets, 1);
  aw->chat_box = GTK_BOX(chat_box);
  aw->chat_scroller = GTK_SCROLLED_WINDOW(chat_scroller);
  aw->prompt_entry = GTK_ENTRY(entry);
  aw->send_btn = GTK_BUTTON(send_btn);
  aw->stop_btn = GTK_BUTTON(stop_btn);
  aw->new_chat_btn = GTK_BUTTON(new_chat_btn);
  aw->spinner = GTK_SPINNER(spinner);
  aw->model_dropdown = GTK_DROP_DOWN(model_dropdown);
  aw->conversations_list = GTK_LIST_BOX(conversations_list);
  aw->models_store = models_store;
  aw->cancellable = NULL;
  aw->in_progress = FALSE;
  aw->alive = TRUE;
  aw->selected_model = load_preferred_model();
  aw->conversations = g_ptr_array_new_with_free_func((GDestroyNotify)conversation_free);
  aw->current_conversation = NULL;
  aw->current_assistant_box = NULL;
  aw->theme_btn = theme_btn;
  aw->dark_theme = load_theme_preference();
  
  // Load conversations and apply theme
  load_conversations(aw);
  
  if (aw->conversations->len == 0) {
      aw->current_conversation = conversation_new();
      g_ptr_array_add(aw->conversations, aw->current_conversation);
  } else {
      aw->current_conversation = g_ptr_array_index(aw->conversations, aw->conversations->len - 1);
      display_conversation(aw, aw->current_conversation);
  }
  
  // Apply initial theme
  apply_theme(aw, aw->dark_theme);
  
  // Connect signals
  g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send_clicked), aw);
  g_signal_connect(stop_btn, "clicked", G_CALLBACK(on_stop_clicked), aw);
  g_signal_connect(new_chat_btn, "clicked", G_CALLBACK(on_new_chat_clicked), aw);
  g_signal_connect(entry, "activate", G_CALLBACK(on_entry_activate), aw);
  g_signal_connect(model_dropdown, "notify::selected", G_CALLBACK(on_model_selected), aw);
  g_signal_connect(conversations_list, "row-activated", G_CALLBACK(on_conversation_selected), aw);
  g_signal_connect(theme_btn, "clicked", G_CALLBACK(on_theme_toggled), aw);
  g_signal_connect(win, "destroy", G_CALLBACK(on_window_destroy), aw);
  
  update_conversations_list(aw);
  
  adw_toolbar_view_set_content(view, paned);
  adw_application_window_set_content(win, GTK_WIDGET(view));
  gtk_window_present(GTK_WINDOW(win));
  
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