# 🐘 Ganesha

**Ganesha** is a lightweight, native desktop client built with **C and GTK4/Libadwaita**, designed to connect with multiple AI backends (like **Ollama**, **OpenAI-compatible APIs**, and **local LLM servers**) — without the heavy memory footprint of Electron or web frameworks.

The goal is to create a **fast, low-RAM universal AI interface** that runs natively on Linux and scales through modular “provider” drivers (HTTP/JSON).  
It uses modern GNOME technologies, async network tasks, and streaming support for real-time responses.

---

## ✨ Key Features (planned)

- 🪶 Lightweight (~30–80 MB RAM footprint)  
- 🧩 Modular provider system (Ollama, OpenAI, llama.cpp)  
- ⚡ Real-time streaming chat output  
- 💾 Persistent chat history and configuration (SQLite / GKeyFile)  
- 🧠 Extensible “Tools” / plugin interface for local commands and files  
- 🖥️ Pure C/GTK4 + Libadwaita — no Electron, no webview  

---

## 🔧 Tech Stack

| Layer | Technology |
|-------|-------------|
| Language | C (GLib / GObject) |
| UI | GTK 4 + Libadwaita |
| HTTP | libsoup 3 |
| JSON | json-glib |
| Build System | Meson + Ninja |
| Persistence | SQLite3 |

---

## 🐘 Why “Ganesha”?

Ganesha symbolizes **wisdom, intellect, and the removal of obstacles** — a fitting name for a project that bridges humans and AI systems in a clean, efficient, and open way.

---

## ⚙️ Build Instructions

```bash
sudo apt install -y build-essential meson ninja-build pkg-config   libgtk-4-dev libadwaita-1-dev libsoup-3.0-dev libjson-glib-dev libsqlite3-dev

meson setup build
meson compile -C build
./build/ganesha
```

---

## 📄 License

MIT License — © 2025 Hangell & Contributors.