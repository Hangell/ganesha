#include <adwaita.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <gtksourceview/gtksource.h>

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
  GPtrArray *images; // Array de imagens em base64
} Message;

typedef struct {
  gchar *id;
  gchar *title;
  GPtrArray *messages;
  gint64 timestamp;
} Conversation;

typedef struct {
  GtkBox        *chat_box;
  GtkTextView   *prompt_text_view;
  GtkScrolledWindow *prompt_scroller;
  GtkButton     *action_btn;
  GtkButton     *attach_btn;
  GtkButton     *audio_btn;
  GtkButton     *new_chat_btn;
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
  GPtrArray     *pending_images; // Imagens pendentes para anexar
  
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
"textview {"
"  background: #0f131a;"
"  color: #e6e6e6;"
"  border: 1px solid #232833;"
"  border-radius: 12px;"
"  padding: 8px 10px;"
"  font-size: 14px;"
"}"
"textview text {"
"  background: #0f131a;"
"  color: #e6e6e6;"
"}"
".input-container {"
"  background: #0f131a;"
"  border: 1px solid #232833;"
"  border-radius: 12px;"
"  padding: 8px;"
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
"button.flat {"
"  background: transparent;"
"  border: none;"
"  color: #a0a0a0;"
"}"
"button.flat:hover {"
"  background: #1a1f27;"
"  color: #e6e6e6;"
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
"}"
".loading-dot {"
"  background-color: #a0a0a0;"
"  border-radius: 50%;"
"  width: 8px;"
"  height: 8px;"
"  animation: blink 1.4s infinite ease-in-out;"
"}"
".loading-dot:nth-child(1) { animation-delay: 0s; }"
".loading-dot:nth-child(2) { animation-delay: 0.2s; }"
".loading-dot:nth-child(3) { animation-delay: 0.4s; }"
"@keyframes blink {"
"  0%, 100% { opacity: 0.3; transform: scale(0.8); }"
"  50% { opacity: 1; transform: scale(1.2); }"
"}"
".code-block {"
"  background-color: #1a1d23;"
"  border: 1px solid #2a2f38;"
"  border-radius: 8px;"
"  font-family: 'Monospace', monospace;"
"  font-size: 12px;"
"  color: #e0e6f1;"
"  padding: 12px;"
"  margin: 8px 0;"
"}"
".inline-code {"
"  background-color: #1a1d23;"
"  color: #e0e6f1;"
"  padding: 2px 6px;"
"  border-radius: 4px;"
"  font-family: 'Monospace', monospace;"
"  font-size: 12px;"
"}"
".header-1 {"
"  font-size: 18px;"
"  font-weight: bold;"
"  color: #e6e6e6;"
"  margin-top: 16px;"
"  margin-bottom: 8px;"
"}"
".header-2 {"
"  font-size: 16px;"
"  font-weight: bold;"
"  color: #e6e6e6;"
"  margin-top: 12px;"
"  margin-bottom: 6px;"
"}"
".header-3 {"
"  font-size: 14px;"
"  font-weight: bold;"
"  color: #e6e6e6;"
"  margin-top: 10px;"
"  margin-bottom: 4px;"
"}"
".list-bullet {"
"  color: #5b6cff;"
"  font-weight: bold;"
"}"
".list-item {"
"  color: #e0e6f1;"
"}"
".blockquote {"
"  border-left: 3px solid #5b6cff;"
"  padding-left: 12px;"
"  margin: 8px 0;"
"  color: #a0a0a0;"
"  font-style: italic;"
"}"
".image-preview {"
"  border-radius: 8px;"
"  border: 1px solid #2a2f38;"
"  margin: 4px;"
"  max-width: 200px;"
"  max-height: 150px;"
"}"
".image-container {"
"  background: #171a21;"
"  border-radius: 8px;"
"  padding: 8px;"
"  margin: 8px 0;"
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
"textview {"
"  background: #ffffff;"
"  color: #333333;"
"  border: 1px solid #ced4da;"
"  border-radius: 12px;"
"  padding: 8px 10px;"
"  font-size: 14px;"
"}"
"textview text {"
"  background: #ffffff;"
"  color: #333333;"
"}"
".input-container {"
"  background: #ffffff;"
"  border: 1px solid #ced4da;"
"  border-radius: 12px;"
"  padding: 8px;"
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
"button.flat {"
"  background: transparent;"
"  border: none;"
"  color: #6c757d;"
"}"
"button.flat:hover {"
"  background: #e9ecef;"
"  color: #333333;"
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
"}"
".loading-dot {"
"  background-color: #6c757d;"
"  border-radius: 50%;"
"  width: 8px;"
"  height: 8px;"
"  animation: blink 1.4s infinite ease-in-out;"
"}"
".loading-dot:nth-child(1) { animation-delay: 0s; }"
".loading-dot:nth-child(2) { animation-delay: 0.2s; }"
".loading-dot:nth-child(3) { animation-delay: 0.4s; }"
"@keyframes blink {"
"  0%, 100% { opacity: 0.3; transform: scale(0.8); }"
"  50% { opacity: 1; transform: scale(1.2); }"
"}"
".code-block {"
"  background-color: #f8f9fa;"
"  border: 1px solid #e9ecef;"
"  border-radius: 8px;"
"  font-family: 'Monospace', monospace;"
"  font-size: 12px;"
"  color: #333333;"
"  padding: 12px;"
"  margin: 8px 0;"
"}"
".inline-code {"
"  background-color: #f8f9fa;"
"  color: #333333;"
"  padding: 2px 6px;"
"  border-radius: 4px;"
"  font-family: 'Monospace', monospace;"
"  font-size: 12px;"
"}"
".header-1 {"
"  font-size: 18px;"
"  font-weight: bold;"
"  color: #333333;"
"  margin-top: 16px;"
"  margin-bottom: 8px;"
"}"
".header-2 {"
"  font-size: 16px;"
"  font-weight: bold;"
"  color: #333333;"
"  margin-top: 12px;"
"  margin-bottom: 6px;"
"}"
".header-3 {"
"  font-size: 14px;"
"  font-weight: bold;"
"  color: #333333;"
"  margin-top: 10px;"
"  margin-bottom: 4px;"
"}"
".list-bullet {"
"  color: #667eea;"
"  font-weight: bold;"
"}"
".list-item {"
"  color: #333333;"
"}"
".blockquote {"
"  border-left: 3px solid #667eea;"
"  padding-left: 12px;"
"  margin: 8px 0;"
"  color: #6c757d;"
"  font-style: italic;"
"}"
".image-preview {"
"  border-radius: 8px;"
"  border: 1px solid #e9ecef;"
"  margin: 4px;"
"  max-width: 200px;"
"  max-height: 150px;"
"}"
".image-container {"
"  background: #f8f9fa;"
"  border-radius: 8px;"
"  padding: 8px;"
"  margin: 8px 0;"
"}";

/* ---------- Message/Conversation helpers ---------- */

static Message* message_new(const gchar *role, const gchar *content) {
  Message *msg = g_new0(Message, 1);
  msg->role = g_strdup(role);
  msg->content = g_strdup(content);
  msg->images = g_ptr_array_new_with_free_func(g_free);
  return msg;
}

static void message_free(Message *msg) {
  if (!msg) return;
  g_free(msg->role);
  g_free(msg->content);
  if (msg->images) g_ptr_array_unref(msg->images);
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
          
          // Save images if present
          if (msg->images && msg->images->len > 0) {
              json_builder_set_member_name(builder, "images");
              json_builder_begin_array(builder);
              for (guint k = 0; k < msg->images->len; k++) {
                  const gchar *img = g_ptr_array_index(msg->images, k);
                  json_builder_add_string_value(builder, img);
              }
              json_builder_end_array(builder);
          }
          
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
          
          // Load images if present
          if (json_object_has_member(msg_obj, "images")) {
              JsonArray *imgs_array = json_object_get_array_member(msg_obj, "images");
              guint imgs_len = json_array_get_length(imgs_array);
              for (guint k = 0; k < imgs_len; k++) {
                  const gchar *img = json_array_get_string_element(imgs_array, k);
                  g_ptr_array_add(msg->images, g_strdup(img));
              }
          }
          
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
          if (g_strcmp0(key, "preferred_model") != 0) {
              json_builder_set_member_name(builder, key);
              JsonNode *value = json_object_get_member(obj, key);
              json_builder_add_value(builder, value);
          }
      }
      g_list_free(members);
  }
  
  // Add model preference
  json_builder_set_member_name(builder, "preferred_model");
  json_builder_add_string_value(builder, model);
  
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

static gboolean load_theme_preference(void) {
  gchar *path = get_prefs_path();
  gchar *contents = NULL;
  gboolean dark_theme = TRUE;
  
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

static GtkWidget* create_loading_bubble(void) {
  GtkWidget *bubble = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(bubble, "message-bubble");
  gtk_widget_add_css_class(bubble, "assistant-message");
  gtk_widget_set_halign(bubble, GTK_ALIGN_START);
  
  GtkWidget *loading_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(loading_box, GTK_ALIGN_START);
  
  for (int i = 0; i < 3; i++) {
    GtkWidget *dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(dot, 8, 8);
    gtk_widget_add_css_class(dot, "loading-dot");
    gtk_box_append(GTK_BOX(loading_box), dot);
  }
  
  gtk_box_append(GTK_BOX(bubble), loading_box);
  return bubble;
}

/* ---------- Enhanced Markdown Parsing ---------- */

static GtkWidget* parse_markdown(const gchar *text) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gchar **lines = g_strsplit(text, "\n", -1);
  
  gboolean in_code_block = FALSE;
  GString *code_block_content = NULL;
  gchar *code_language = NULL;
  GtkWidget *code_scroll = NULL;
  GtkWidget *code_view = NULL;
  
  for (gchar **line = lines; *line; line++) {
    gchar *trimmed = g_strstrip(g_strdup(*line));
    
    // Code blocks
    if (g_str_has_prefix(trimmed, "```")) {
      if (!in_code_block) {
        in_code_block = TRUE;
        code_block_content = g_string_new("");
        code_language = g_strdup(trimmed + 3);
        
        // Create scrollable code block
        code_scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(code_scroll),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(code_scroll, -1, 200);
        
        code_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(code_view), FALSE);
        gtk_text_view_set_monospace(GTK_TEXT_VIEW(code_view), TRUE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(code_view), GTK_WRAP_NONE);
        gtk_widget_add_css_class(code_view, "code-block");
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(code_scroll), code_view);
      } else {
        in_code_block = FALSE;
        
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(code_view));
        gtk_text_buffer_set_text(buffer, code_block_content->str, -1);
        gtk_box_append(GTK_BOX(box), code_scroll);
        
        g_string_free(code_block_content, TRUE);
        g_free(code_language);
        code_block_content = NULL;
        code_language = NULL;
      }
      g_free(trimmed);
      continue;
    }
    
    if (in_code_block) {
      if (code_block_content->len > 0) {
        g_string_append(code_block_content, "\n");
      }
      g_string_append(code_block_content, trimmed);
      g_free(trimmed);
      continue;
    }
    
    // Inline code
    if (g_strrstr(trimmed, "`")) {
      GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
      gchar **parts = g_strsplit(trimmed, "`", -1);
      gboolean is_code = FALSE;
      
      for (gchar **part = parts; *part; part++) {
        if (**part) {
          GtkWidget *label = gtk_label_new(*part);
          gtk_label_set_wrap(GTK_LABEL(label), TRUE);
          gtk_label_set_selectable(GTK_LABEL(label), TRUE);
          
          if (is_code) {
            gtk_widget_add_css_class(label, "inline-code");
          } else {
            gtk_widget_add_css_class(label, "message-content");
          }
          gtk_box_append(GTK_BOX(hbox), label);
        }
        is_code = !is_code;
      }
      
      g_strfreev(parts);
      gtk_box_append(GTK_BOX(box), hbox);
      g_free(trimmed);
      continue;
    }
    
    // Headers
    if (g_str_has_prefix(trimmed, "# ")) {
      GtkWidget *label = gtk_label_new(trimmed + 2);
      gtk_label_set_wrap(GTK_LABEL(label), TRUE);
      gtk_label_set_xalign(GTK_LABEL(label), 0.0);
      gtk_label_set_selectable(GTK_LABEL(label), TRUE);
      gtk_widget_add_css_class(label, "header-1");
      gtk_box_append(GTK_BOX(box), label);
      g_free(trimmed);
      continue;
    }
    
    if (g_str_has_prefix(trimmed, "## ")) {
      GtkWidget *label = gtk_label_new(trimmed + 3);
      gtk_label_set_wrap(GTK_LABEL(label), TRUE);
      gtk_label_set_xalign(GTK_LABEL(label), 0.0);
      gtk_label_set_selectable(GTK_LABEL(label), TRUE);
      gtk_widget_add_css_class(label, "header-2");
      gtk_box_append(GTK_BOX(box), label);
      g_free(trimmed);
      continue;
    }
    
    if (g_str_has_prefix(trimmed, "### ")) {
      GtkWidget *label = gtk_label_new(trimmed + 4);
      gtk_label_set_wrap(GTK_LABEL(label), TRUE);
      gtk_label_set_xalign(GTK_LABEL(label), 0.0);
      gtk_label_set_selectable(GTK_LABEL(label), TRUE);
      gtk_widget_add_css_class(label, "header-3");
      gtk_box_append(GTK_BOX(box), label);
      g_free(trimmed);
      continue;
    }
    
    // Blockquotes
    if (g_str_has_prefix(trimmed, "> ")) {
      GtkWidget *label = gtk_label_new(trimmed + 2);
      gtk_label_set_wrap(GTK_LABEL(label), TRUE);
      gtk_label_set_xalign(GTK_LABEL(label), 0.0);
      gtk_label_set_selectable(GTK_LABEL(label), TRUE);
      gtk_widget_add_css_class(label, "blockquote");
      gtk_widget_set_margin_start(label, 12);
      gtk_box_append(GTK_BOX(box), label);
      g_free(trimmed);
      continue;
    }
    
    // Lists
    if (g_str_has_prefix(trimmed, "- ") || g_str_has_prefix(trimmed, "* ")) {
      GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
      GtkWidget *bullet = gtk_label_new("•");
      gtk_widget_add_css_class(bullet, "list-bullet");
      gtk_box_append(GTK_BOX(hbox), bullet);
      
      GtkWidget *label = gtk_label_new(trimmed + 2);
      gtk_label_set_wrap(GTK_LABEL(label), TRUE);
      gtk_label_set_xalign(GTK_LABEL(label), 0.0);
      gtk_label_set_selectable(GTK_LABEL(label), TRUE);
      gtk_widget_set_hexpand(label, TRUE);
      gtk_widget_add_css_class(label, "list-item");
      gtk_box_append(GTK_BOX(hbox), label);
      
      gtk_box_append(GTK_BOX(box), hbox);
      g_free(trimmed);
      continue;
    }
    
    // Numbered lists
    if (g_ascii_isdigit(*trimmed)) {
      const gchar *dot = strchr(trimmed, '.');
      if (dot && *(dot+1) == ' ') {
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        
        gchar *number = g_strndup(trimmed, dot - trimmed + 1);
        GtkWidget *num_label = gtk_label_new(number);
        gtk_widget_add_css_class(num_label, "list-bullet");
        gtk_box_append(GTK_BOX(hbox), num_label);
        g_free(number);
        
        GtkWidget *label = gtk_label_new(dot + 2);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_label_set_selectable(GTK_LABEL(label), TRUE);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_widget_add_css_class(label, "list-item");
        gtk_box_append(GTK_BOX(hbox), label);
        
        gtk_box_append(GTK_BOX(box), hbox);
        g_free(trimmed);
        continue;
      }
    }
    
    // Regular text
    if (*trimmed != '\0') {
      GtkWidget *label = gtk_label_new(trimmed);
      gtk_label_set_wrap(GTK_LABEL(label), TRUE);
      gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
      gtk_label_set_xalign(GTK_LABEL(label), 0.0);
      gtk_label_set_selectable(GTK_LABEL(label), TRUE);
      gtk_widget_add_css_class(label, "message-content");
      gtk_box_append(GTK_BOX(box), label);
    } else {
      GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_size_request(spacer, -1, 8);
      gtk_box_append(GTK_BOX(box), spacer);
    }
    
    g_free(trimmed);
  }
  
  // Handle unclosed code block
  if (in_code_block && code_block_content) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(code_view));
    gtk_text_buffer_set_text(buffer, code_block_content->str, -1);
    gtk_box_append(GTK_BOX(box), code_scroll);
    g_string_free(code_block_content, TRUE);
    g_free(code_language);
  }
  
  g_strfreev(lines);
  return box;
}

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
  
  if (g_strcmp0(role, "assistant") == 0 && content && *content) {
    GtkWidget *content_box = parse_markdown(content);
    gtk_box_append(GTK_BOX(bubble), content_box);
  } else {
    GtkWidget *label = gtk_label_new(content);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_widget_add_css_class(label, "message-content");
    gtk_box_append(GTK_BOX(bubble), label);
  }
  
  return bubble;
}

static void append_message_bubble(AppWidgets *aw, const gchar *role, const gchar *content) {
  if (!aw || !aw->chat_box) return;
  
  GtkWidget *bubble = create_message_bubble(role, content);
  gtk_box_append(aw->chat_box, bubble);
  
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

static void update_action_button(AppWidgets *aw) {
  if (!aw || !aw->action_btn) return;
  
  if (aw->in_progress) {
    gtk_button_set_label(aw->action_btn, "⏹ Stop");
    gtk_widget_remove_css_class(GTK_WIDGET(aw->action_btn), "suggested-action");
    gtk_widget_add_css_class(GTK_WIDGET(aw->action_btn), "destructive-action");
  } else {
    gtk_button_set_label(aw->action_btn, "⬆ Send");
    gtk_widget_remove_css_class(GTK_WIDGET(aw->action_btn), "destructive-action");
    gtk_widget_add_css_class(GTK_WIDGET(aw->action_btn), "suggested-action");
  }
}

static void set_streaming_state(AppWidgets *aw, gboolean running) {
  if (!aw || !aw->alive) return;
  aw->in_progress = running;
  
  if (aw->prompt_text_view) 
    gtk_widget_set_sensitive(GTK_WIDGET(aw->prompt_text_view), !running);
  if (aw->model_dropdown) 
    gtk_widget_set_sensitive(GTK_WIDGET(aw->model_dropdown), !running);
  if (aw->new_chat_btn) 
    gtk_widget_set_sensitive(GTK_WIDGET(aw->new_chat_btn), !running);
  if (aw->attach_btn)
    gtk_widget_set_sensitive(GTK_WIDGET(aw->attach_btn), !running);
  if (aw->audio_btn)
    gtk_widget_set_sensitive(GTK_WIDGET(aw->audio_btn), !running);
  
  update_action_button(aw);
}

/* --------- Key press Enter Send Msg -------- */


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

static void apply_theme(AppWidgets *aw, gboolean dark_theme) {
  GtkCssProvider *css_provider = gtk_css_provider_new();
  AdwStyleManager *style = adw_style_manager_get_default();
  adw_style_manager_set_color_scheme(style, dark_theme ? ADW_COLOR_SCHEME_FORCE_DARK : ADW_COLOR_SCHEME_FORCE_LIGHT);
  
  if (dark_theme) {
    gtk_css_provider_load_from_string(css_provider, DARK_CSS);
  } else {
    gtk_css_provider_load_from_string(css_provider, LIGHT_CSS);
  }
  
  GdkDisplay *display = gdk_display_get_default();
  gtk_style_context_add_provider_for_display(
      display,
      GTK_STYLE_PROVIDER(css_provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
  );
  
  if (aw->theme_btn) {
    if (dark_theme) {
      gtk_button_set_icon_name(GTK_BUTTON(aw->theme_btn), "weather-clear-night-symbolic");
    } else {
      gtk_button_set_icon_name(GTK_BUTTON(aw->theme_btn), "weather-clear-symbolic");
    }
  }
  
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

/* ---------- Image Handling ---------- */

static gchar* image_to_base64(const gchar *filepath) {
  gchar *contents = NULL;
  gsize length = 0;
  
  if (!g_file_get_contents(filepath, &contents, &length, NULL)) {
    return NULL;
  }
  
  gchar *base64 = g_base64_encode((const guchar*)contents, length);
  g_free(contents);
  return base64;
}

static void on_image_selected(GtkFileDialog *dialog, GAsyncResult *result, gpointer user_data) {
  AppWidgets *aw = (AppWidgets*)user_data;
  GError *error = NULL;
  
  GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);
  if (error) {
    g_error_free(error);
    return;
  }
  
  if (!file) return;
  
  gchar *filepath = g_file_get_path(file);
  gchar *base64 = image_to_base64(filepath);
  
  if (base64) {
    g_ptr_array_add(aw->pending_images, base64);
    
    // Show image preview in input area
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(aw->prompt_text_view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    
    gchar *markup = g_strdup_printf("\n[Image: %s attached]\n", g_path_get_basename(filepath));
    gtk_text_buffer_insert(buffer, &end, markup, -1);
    g_free(markup);
  }
  
  g_free(filepath);
  g_object_unref(file);
}

static void on_attach_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  AppWidgets *aw = (AppWidgets*)user_data;
  if (!aw) return;
  
  GtkFileDialog *dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, "Select Image");
  
  GtkFileFilter *filter = gtk_file_filter_new();
  gtk_file_filter_set_name(filter, "Images");
  gtk_file_filter_add_mime_type(filter, "image/png");
  gtk_file_filter_add_mime_type(filter, "image/jpeg");
  gtk_file_filter_add_mime_type(filter, "image/gif");
  gtk_file_filter_add_mime_type(filter, "image/webp");
  
  GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
  g_list_store_append(filters, filter);
  gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
  
  GtkWindow *window = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(btn)));
  gtk_file_dialog_open(dialog, window, NULL, 
                      (GAsyncReadyCallback)on_image_selected, aw);
  
  g_object_unref(filters);
  g_object_unref(filter);
}

/* ---------- Audio Handling (Placeholder) ---------- */

static void on_audio_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
  (void)response_id;
  (void)user_data;
  gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_audio_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  AppWidgets *aw = (AppWidgets*)user_data;
  if (!aw) return;
  
  // TODO: Implement audio recording/playback
  GtkWidget *dialog = adw_message_dialog_new(
      GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(btn))),
      "Audio Feature",
      "Audio recording and playback feature coming soon!"
  );
  
  adw_message_dialog_add_responses(ADW_MESSAGE_DIALOG(dialog),
      "ok", "OK",
      NULL);
  
  g_signal_connect(dialog, "response", G_CALLBACK(on_audio_response), NULL);
  gtk_window_present(GTK_WINDOW(dialog));
}

/* ---------- Callback postados no main loop ---------- */

typedef struct {
  AppWidgets *aw;
  char       *chunk;
} AppendChunkData;

static gboolean ui_append_chunk_cb(gpointer data) {
  AppendChunkData *d = (AppendChunkData*)data;
  if (d->aw && d->aw->alive && d->aw->current_assistant_box) {
      GtkWidget *first_child = gtk_widget_get_first_child(d->aw->current_assistant_box);
      if (first_child && gtk_widget_has_css_class(first_child, "loading-dot")) {
          GtkWidget *new_bubble = create_message_bubble("assistant", d->chunk);
          gtk_widget_set_hexpand(new_bubble, TRUE);
          
          GtkWidget *parent = gtk_widget_get_parent(d->aw->current_assistant_box);
          GtkWidget *prev_sibling = gtk_widget_get_prev_sibling(d->aw->current_assistant_box);
          
          gtk_box_remove(GTK_BOX(parent), d->aw->current_assistant_box);
          if (prev_sibling) {
              gtk_box_insert_child_after(GTK_BOX(parent), new_bubble, prev_sibling);
          } else {
              gtk_box_prepend(GTK_BOX(parent), new_bubble);
          }
          
          d->aw->current_assistant_box = new_bubble;
      } else {
          if (d->aw->current_conversation && d->aw->current_conversation->messages->len > 0) {
              Message *last_msg = g_ptr_array_index(d->aw->current_conversation->messages, 
                                                  d->aw->current_conversation->messages->len - 1);
              if (g_strcmp0(last_msg->role, "assistant") == 0) {
                  gchar *new_content = g_strconcat(last_msg->content, d->chunk, NULL);
                  
                  GtkWidget *parent = gtk_widget_get_parent(d->aw->current_assistant_box);
                  GtkWidget *prev_sibling = gtk_widget_get_prev_sibling(d->aw->current_assistant_box);
                  
                  GtkWidget *new_bubble = create_message_bubble("assistant", new_content);
                  gtk_box_remove(GTK_BOX(parent), d->aw->current_assistant_box);
                  if (prev_sibling) {
                      gtk_box_insert_child_after(GTK_BOX(parent), new_bubble, prev_sibling);
                  } else {
                      gtk_box_prepend(GTK_BOX(parent), new_bubble);
                  }
                  
                  d->aw->current_assistant_box = new_bubble;
                  g_free(last_msg->content);
                  last_msg->content = new_content;
              }
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
      GtkWidget *bubble = create_loading_bubble();
      gtk_box_append(aw->chat_box, bubble);
      aw->current_assistant_box = bubble;
      
      if (aw->current_conversation) {
          conversation_add_message(aw->current_conversation, "assistant", "");
      }
      
      GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(aw->chat_scroller);
      gtk_adjustment_set_value(vadj, gtk_adjustment_get_upper(vadj));
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
      
      // Add images if present
      if (msg->images && msg->images->len > 0) {
          json_builder_set_member_name(b, "images");
          json_builder_begin_array(b);
          for (guint j = 0; j < msg->images->len; j++) {
              const gchar *img = g_ptr_array_index(msg->images, j);
              json_builder_add_string_value(b, img);
          }
          json_builder_end_array(b);
      }
      
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
  
  Message *msg = message_new("user", user_text);
  
  // Add pending images
  if (aw->pending_images && aw->pending_images->len > 0) {
      for (guint i = 0; i < aw->pending_images->len; i++) {
          gchar *img = g_ptr_array_index(aw->pending_images, i);
          g_ptr_array_add(msg->images, g_strdup(img));
      }
      g_ptr_array_remove_range(aw->pending_images, 0, aw->pending_images->len);
  }
  
  g_ptr_array_add(aw->current_conversation->messages, msg);
  
  if (!aw->current_conversation->title) {
      gchar *title = g_strdup(user_text);
      if (strlen(title) > 50) {
          title[47] = '.';
          title[48] = '.';
          title[49] = '.';
          title[50] = '\0';
      }
      aw->current_conversation->title = title;
  }
  
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

static void on_action_btn_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  AppWidgets *aw = (AppWidgets*)user_data;
  if (!aw || !aw->alive) return;
  
  if (aw->in_progress) {
      // Stop generation
      if (aw->cancellable) {
          g_cancellable_cancel(aw->cancellable);
      }
  } else {
      // Send message
      GtkTextBuffer *buffer = gtk_text_view_get_buffer(aw->prompt_text_view);
      GtkTextIter start, end;
      gtk_text_buffer_get_bounds(buffer, &start, &end);
      gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
      
      if (text && *text) {
          start_ollama_stream(aw, text);
          gtk_text_buffer_set_text(buffer, "", -1);
      }
      g_free(text);
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
  
  if (aw->pending_images) {
      g_ptr_array_unref(aw->pending_images);
  }
  
  g_free(aw->selected_model);
}

/* ---------- Text View Auto-resize ---------- */

static void on_text_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
  AppWidgets *aw = (AppWidgets*)user_data;
  if (!aw || !aw->prompt_scroller) return;
  
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  gint line_count = gtk_text_iter_get_line(&end) + 1;
  
  // Adjust height based on line count
  if (line_count <= 3) {
    gtk_scrolled_window_set_max_content_height(aw->prompt_scroller, 120);
  } else if (line_count <= 6) {
    gtk_scrolled_window_set_max_content_height(aw->prompt_scroller, 200);
  } else {
    gtk_scrolled_window_set_max_content_height(aw->prompt_scroller, 300);
  }
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
  
  // Sidebar
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request(sidebar, 250, -1);
  
  // Sidebar header
  GtkWidget *sidebar_header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class(sidebar_header, "sidebar-header");
  
  GtkWidget *conversations_title = gtk_label_new("Conversations");
  gtk_widget_add_css_class(conversations_title, "title-4");
  gtk_widget_set_halign(conversations_title, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(conversations_title, 12);
  gtk_widget_set_margin_bottom(conversations_title, 8);
  
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
  
  // Model selector
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
  
  // Chat area
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
  
  // Input area with multi-line text view
  GtkWidget *input_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(input_container, "input-container");
  gtk_widget_set_margin_start(input_container, 16);
  gtk_widget_set_margin_end(input_container, 16);
  gtk_widget_set_margin_top(input_container, 12);
  gtk_widget_set_margin_bottom(input_container, 16);
  
  // Text view in scrolled window
  GtkWidget *prompt_scroller = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(prompt_scroller),
                                GTK_POLICY_NEVER,
                                GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(prompt_scroller), 60);
  gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(prompt_scroller), 300);
  gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(prompt_scroller), TRUE);
  
  GtkWidget *text_view = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text_view), FALSE);
  gtk_widget_set_hexpand(text_view, TRUE);
  gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text_view), 8);
  gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text_view), 8);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 12);
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 12);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(prompt_scroller), text_view);
  
  // Button row
  GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(button_box, GTK_ALIGN_END);
  gtk_widget_set_margin_top(button_box, 8);
  
  GtkWidget *attach_btn = gtk_button_new_from_icon_name("mail-attachment-symbolic");
  gtk_widget_add_css_class(attach_btn, "flat");
  gtk_widget_set_tooltip_text(attach_btn, "Attach image");
  
  GtkWidget *audio_btn = gtk_button_new_from_icon_name("audio-input-microphone-symbolic");
  gtk_widget_add_css_class(audio_btn, "flat");
  gtk_widget_set_tooltip_text(audio_btn, "Voice input (coming soon)");
  
  GtkWidget *action_btn = gtk_button_new_with_label("⬆ Send");
  gtk_widget_add_css_class(action_btn, "suggested-action");
  
  gtk_box_append(GTK_BOX(button_box), attach_btn);
  gtk_box_append(GTK_BOX(button_box), audio_btn);
  gtk_box_append(GTK_BOX(button_box), action_btn);
  
  gtk_box_append(GTK_BOX(input_container), prompt_scroller);
  gtk_box_append(GTK_BOX(input_container), button_box);
  
  gtk_box_append(GTK_BOX(vbox), model_hbox);
  gtk_box_append(GTK_BOX(vbox), chat_scroller);
  gtk_box_append(GTK_BOX(vbox), input_container);
  
  gtk_paned_set_start_child(GTK_PANED(paned), sidebar);
  gtk_paned_set_end_child(GTK_PANED(paned), vbox);
  gtk_paned_set_position(GTK_PANED(paned), 250);
  gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
  gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);
  
  // Initialize app widgets
  AppWidgets *aw = g_new0(AppWidgets, 1);
  aw->chat_box = GTK_BOX(chat_box);
  aw->chat_scroller = GTK_SCROLLED_WINDOW(chat_scroller);
  aw->prompt_text_view = GTK_TEXT_VIEW(text_view);
  aw->prompt_scroller = GTK_SCROLLED_WINDOW(prompt_scroller);
  aw->action_btn = GTK_BUTTON(action_btn);
  aw->attach_btn = GTK_BUTTON(attach_btn);
  aw->audio_btn = GTK_BUTTON(audio_btn);
  aw->new_chat_btn = GTK_BUTTON(new_chat_btn);
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
  aw->pending_images = g_ptr_array_new_with_free_func(g_free);
  
  load_conversations(aw);
  
  if (aw->conversations->len == 0) {
      aw->current_conversation = conversation_new();
      g_ptr_array_add(aw->conversations, aw->current_conversation);
  } else {
      aw->current_conversation = g_ptr_array_index(aw->conversations, aw->conversations->len - 1);
      display_conversation(aw, aw->current_conversation);
  }
  
  apply_theme(aw, aw->dark_theme);
  
  // Connect signals
  g_signal_connect(action_btn, "clicked", G_CALLBACK(on_action_btn_clicked), aw);
  g_signal_connect(attach_btn, "clicked", G_CALLBACK(on_attach_clicked), aw);
  g_signal_connect(audio_btn, "clicked", G_CALLBACK(on_audio_clicked), aw);
  g_signal_connect(new_chat_btn, "clicked", G_CALLBACK(on_new_chat_clicked), aw);
  g_signal_connect(model_dropdown, "notify::selected", G_CALLBACK(on_model_selected), aw);
  g_signal_connect(conversations_list, "row-activated", G_CALLBACK(on_conversation_selected), aw);
  g_signal_connect(theme_btn, "clicked", G_CALLBACK(on_theme_toggled), aw);
  
  // Connect text buffer change signal for auto-resize
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(aw->prompt_text_view);
  g_signal_connect(buffer, "changed", G_CALLBACK(on_text_buffer_changed), aw);
  
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