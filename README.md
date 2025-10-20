# 🐘 Ganesha

**Ganesha** is a lightweight, native desktop AI chat client built with **C, GTK4, and Libadwaita**. It provides a modern chat interface for **Ollama** and other OpenAI-compatible APIs — with a fraction of the memory footprint of Electron-based alternatives.

> **Memory usage**: ~40-80 MB RAM vs 300-500+ MB for typical Electron apps

---

## ✨ Features

- 🪶 **Lightweight** — Native C/GTK4 implementation, minimal resource usage
- 💬 **Full conversation history** — Persistent chat sessions with context management
- 🔄 **Real-time streaming** — Token-by-token response display
- 🎨 **Modern UI** — Beautiful Libadwaita interface with dark mode support
- 💾 **Auto-save** — All conversations saved locally in JSON format
- 🔌 **Multi-model** — Switch between different Ollama models on the fly
- 🚀 **Fast startup** — Native binary, instant launch

---

## 🎯 Current Status

**✅ Working:**
- Streaming chat with Ollama API
- Conversation history and persistence
- Model selection and preferences
- Sidebar navigation between chats
- Start/stop generation controls

**🚧 Planned:**
- Markdown rendering with syntax highlighting (GtkSourceView)
- Copy/export conversation functionality
- Delete and rename conversations
- Multi-backend support (OpenAI, Anthropic, etc.)
- Keyboard shortcuts
- Message editing and regeneration

---

## 🔧 Tech Stack

| Component | Technology |
|-----------|-----------|
| **Language** | C (GLib/GObject) |
| **UI Framework** | GTK 4 + Libadwaita |
| **HTTP Client** | libsoup 3 |
| **JSON Parsing** | json-glib |
| **Build System** | GCC / Meson |
| **Storage** | JSON files |

---

## 📦 Installation

### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt install -y build-essential pkg-config \
  libgtk-4-dev libadwaita-1-dev \
  libsoup-3.0-dev libjson-glib-dev
```

**Fedora:**
```bash
sudo dnf install -y gcc pkg-config \
  gtk4-devel libadwaita-devel \
  libsoup3-devel json-glib-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel gtk4 libadwaita libsoup3 json-glib
```

### Build from Source

```bash
# Clone the repository
git clone https://github.com/yourusername/ganesha.git
cd ganesha

# Compile
gcc -o ganesha ganesha.c \
    $(pkg-config --cflags --libs libadwaita-1 libsoup-3.0 json-glib-1.0) \
    -Wall -Wextra -O2

# Run
./ganesha
```

### Optional: Install System-wide

```bash
sudo install -Dm755 ganesha /usr/local/bin/ganesha
```

---

## ⚙️ Configuration

### Connecting to Your Ollama Instance

By default, Ganesha connects to `http://192.168.0.3:11434`. To change this:

**Edit `ganesha.c` line 8:**
```c
static const char *OLLAMA_BASE_URL = "http://localhost:11434";  // Your Ollama address
```

**Common configurations:**

| Setup | URL |
|-------|-----|
| **Local Ollama** | `http://localhost:11434` |
| **Remote server** | `http://192.168.1.100:11434` |
| **Docker container** | `http://172.17.0.2:11434` |
| **Custom port** | `http://localhost:8080` |

After editing, recompile:
```bash
gcc -o ganesha ganesha.c \
    $(pkg-config --cflags --libs libadwaita-1 libsoup-3.0 json-glib-1.0) \
    -Wall -Wextra -O2
```

### OpenAI-Compatible APIs

Ganesha works with any OpenAI-compatible endpoint. To use services like:

- **LM Studio**: `http://localhost:1234/v1`
- **Text Generation WebUI**: `http://localhost:5000/v1`
- **vLLM**: `http://localhost:8000/v1`

**Edit line 8:**
```c
static const char *OLLAMA_BASE_URL = "http://localhost:1234/v1";
```

> **Note**: Currently uses Ollama's `/api/chat` endpoint format. OpenAI format (`/v1/chat/completions`) support coming soon.

### Default Model

To change the default model, edit **line 9**:
```c
static const char *DEFAULT_MODEL = "llama3.2:3b";  // Your preferred model
```

Popular models:
- `llama3.2:3b` (fast, 2GB RAM)
- `llama3.2:1b` (very fast, 1GB RAM)
- `mistral:7b` (balanced)
- `codellama:13b` (code-focused)

---

## 🚀 Usage

### Starting Ollama

Make sure Ollama is running before launching Ganesha:

```bash
# Check if Ollama is running
curl http://localhost:11434/api/tags

# If not running, start it
ollama serve
```

### First Launch

1. **Start Ganesha**: `./ganesha`
2. **Select a model** from the dropdown (auto-detected from Ollama)
3. **Type your message** and press Enter or click "Send"
4. **New Chat** button creates a fresh conversation
5. **Click conversations** in the sidebar to switch between them

### Keyboard Shortcuts

- `Enter` — Send message
- `Ctrl+C` — Copy selected text
- `Escape` — Stop generation (when "Stop" button is active)

---

## 📁 Data Storage

Ganesha stores all data in `~/.config/ganesha/`:

```
~/.config/ganesha/
├── ganesha-prefs.json          # User preferences (selected model)
└── ganesha-conversations.json  # All chat history
```

### Backup Your Conversations

```bash
# Backup
cp ~/.config/ganesha/ganesha-conversations.json ~/ganesha-backup.json

# Restore
cp ~/ganesha-backup.json ~/.config/ganesha/ganesha-conversations.json
```

### Clear All Data

```bash
rm -rf ~/.config/ganesha/
```

---

## 🐛 Troubleshooting

### "Network error" when sending messages

**Problem**: Can't connect to Ollama
```bash
# Check if Ollama is running
curl http://localhost:11434/api/tags

# Start Ollama if needed
ollama serve
```

### No models appearing in dropdown

**Problem**: Ollama not responding or wrong URL
```bash
# Test connection manually
curl http://localhost:11434/api/tags

# Check if you need to change OLLAMA_BASE_URL in ganesha.c
```

### Compilation errors

**Missing dependencies**:
```bash
# Check what's installed
pkg-config --modversion libadwaita-1 libsoup-3.0 json-glib-1.0

# Install missing packages (Ubuntu/Debian)
sudo apt install libadwaita-1-dev libsoup-3.0-dev libjson-glib-dev
```

### App crashes on startup

**Check GTK version**:
```bash
pkg-config --modversion gtk4
# Should be >= 4.0
```

---

## 🗺️ Roadmap

### Phase 1 - Core Stability (Current)
- [x] Streaming chat interface
- [x] Conversation history
- [x] Model selection
- [x] Persistent storage
- [ ] Bug fixes and stability

### Phase 2 - Essential Features
- [ ] Markdown rendering
- [ ] Code syntax highlighting (GtkSourceView)
- [ ] Copy message button
- [ ] Delete/rename conversations
- [ ] Export to text/markdown

### Phase 3 - Advanced Features
- [ ] Multi-backend support (OpenAI, Anthropic, etc.)
- [ ] System prompts
- [ ] Temperature and parameter controls
- [ ] Search conversations
- [ ] Keyboard shortcuts

### Phase 4 - Power User
- [ ] Plugins/extensions system
- [ ] RAG document search
- [ ] Custom themes
- [ ] Multi-language support

---

## 🤝 Contributing

Contributions are welcome! Whether it's:

- 🐛 Bug reports
- 💡 Feature requests
- 📝 Documentation improvements
- 🔧 Code contributions

Please open an issue or pull request on GitHub.

---

## 🐘 Why "Ganesha"?

In Hindu mythology, **Ganesha** is the deity of **wisdom, intellect, and the removal of obstacles**. This project aims to remove the obstacles between humans and AI systems — providing a clean, efficient, and accessible interface without the bloat of modern web technologies.

---

## 📊 Performance Comparison

| Client | RAM Usage | Startup Time | Tech Stack |
|--------|-----------|--------------|------------|
| **Ganesha** | ~45 MB | <0.5s | C + GTK4 |
| Typical Electron App | 300-500 MB | 2-3s | Chromium + Node.js |
| Web Browser Tab | 200-400 MB | 1-2s | Browser engine |

---

## 📄 License

MIT License — © 2025 Hangell & Contributors

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software...
```

See [LICENSE](LICENSE) file for full text.

---

## 🙏 Acknowledgments

- **Ollama** team for the excellent local LLM server
- **GNOME** project for GTK4 and Libadwaita
- The open-source community for inspiration

---

## 📞 Support

- **Email**: rodrigo@hangell.org

---

**Made with ❤️ and C** — Because not everything needs to be Electron.