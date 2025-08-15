# nx - High-Performance CLI Notes Application

**nx** is a local-first, plaintext Markdown note-taking tool for Linux terminals that prioritizes speed, security, and composability. Built in modern C++, it delivers instant operations on thousands of notes while maintaining full offline functionality.

## 🚀 Features

### **Core Features (Implemented)**
- ⚡ **Blazing Fast**: Sub-100ms operations on 10k+ notes
- 📝 **Markdown Native**: Notes stored as plaintext Markdown with YAML front-matter
- 🔍 **Powerful Search**: Full-text search with SQLite FTS5 and ripgrep fallback
- 🏷️ **Smart Tagging**: Manual and AI-powered tag management
- 📚 **Notebooks**: Organize notes into collections
- 🔗 **Backlinks**: Automatic relationship discovery
- 🎨 **Rich TUI**: Interactive terminal interface with 3-pane layout
- 🤖 **AI Integration**: Optional Claude/GPT integration for summaries, titles, and Q&A
- 📤 **Export**: Multiple formats (Markdown, JSON, PDF, HTML)
- 🔐 **Encryption**: Per-file encryption with age/rage (basic implementation)
- 🔄 **Git Sync**: Version control and synchronization (basic implementation)

### **AI-Powered Features**
- 🧠 **Ask Questions**: RAG-powered Q&A over your note collection
- 📋 **Auto-Summarize**: Generate concise summaries with different styles
- 🏷️ **Auto-Tagging**: AI-suggested tags based on content
- 📝 **Title Suggestions**: AI-generated descriptive titles
- ✍️ **Content Rewriting**: Adjust tone and style
- ✅ **Task Extraction**: Extract action items and todos
- 🔗 **Link Suggestions**: Find related notes automatically
- 📊 **Outline Generation**: Create structured outlines for topics

## 🎯 Quick Start

### Installation
```bash
# Build from source (requires CMake, vcpkg)
git clone https://github.com/your-org/nx
cd nx
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Basic Usage
```bash
# Create your first note
nx new "My First Note" --tags personal,ideas

# Launch interactive TUI
nx ui

# Search notes
nx grep "machine learning"

# List recent notes
nx ls --since yesterday

# Ask AI questions about your notes
nx ask "What did I learn about Python this week?"
```

### Interactive TUI
Run `nx` or `nx ui` to launch the full-featured terminal interface:

```
┌─Tags─────────────┐│┌─Notes────────────────────────────────┐│┌─Preview──┐
│📂 ai (12)        ││││⚡ Meeting Notes - Q3 Planning        │││# Meeting  │
│📂 python (8)     │││  2024-08-15 14:23 📋 ai,work        │││          │
│📂 work (25)      │││▶ Python AI Integration Ideas         │││## Agenda │
│📂 ideas (15)     │││  2024-08-14 09:15 🏷️ python,ai     │││- Review   │
│📂 meetings (18)  │││  Understanding Neural Networks       │││- Plan     │
│                  │││  2024-08-12 16:45 📚 learning      │││- Discuss  │
│                  │││  Weekly Review Template              │││          │
│                  │││  2024-08-10 12:00 ⭐ template      │││## Notes   │
└──────────────────┘│└──────────────────────────────────────┘│└──────────┘
```

**Key bindings:**
- `h/j/k/l` or arrow keys - Navigate
- `/` - Search notes
- `n` - New note
- `e` - Edit selected note
- `a` - AI auto-tag selected note
- `A` - AI auto-title selected note
- `?` - Help

## 📚 Commands Reference

### **Note Management**
```bash
nx new [title] [--tags a,b] [--nb notebook]     # Create note
nx edit <id>                                     # Edit in $EDITOR
nx view <id>                                     # Display note
nx rm <id>                                       # Delete note
nx mv <id> --nb <notebook>                       # Move to notebook
```

### **Search & Discovery**
```bash
nx ls [--tag work] [--since yesterday]           # List notes
nx grep <query> [--regex] [--content]            # Search content
nx open <fuzzy-match>                            # Fuzzy find and open
nx backlinks <id>                                # Show backlinks
nx tags                                          # List all tags
```

### **AI Features**
```bash
nx ask "What did I learn about X?"               # RAG Q&A
nx summarize <id> [--style bullets] [--apply]    # AI summary
nx tag-suggest <id> [--apply]                    # AI tag suggestions
nx title <id> [--apply]                          # AI title suggestions
nx rewrite <id> [--tone crisp]                   # Content rewriting
nx tasks <id>                                    # Extract action items
nx suggest-links <id>                            # Find related notes
nx outline "topic"                               # Generate outline
```

### **Import/Export**
```bash
nx export md [--to /path]                        # Export as Markdown
nx export json [--filter "tag:work"]             # Export as JSON
nx export pdf [--to /path]                       # Export as PDF
```

### **System**
```bash
nx ui                                            # Launch TUI
nx sync [push|pull]                              # Git synchronization
nx --version                                     # Show version
```

## 🔧 Configuration

Configuration is stored in `~/.config/nx/config.toml`:

```toml
[core]
notes_dir = "~/.local/share/nx/notes"
editor = "nvim"
default_notebook = "inbox"

[search]
engine = "sqlite"  # or "ripgrep"
case_sensitive = false

[ai]
provider = "anthropic"  # or "openai"
model = "claude-3.5-sonnet"
api_key = "env:ANTHROPIC_API_KEY"
max_tokens = 1000

[ui]
theme = "dark"
show_line_numbers = true
auto_save = true
```

### AI Setup
To enable AI features, set your API key:

```bash
# For Anthropic Claude
export ANTHROPIC_API_KEY="your-api-key"

# For OpenAI
export OPENAI_API_KEY="your-api-key"
```

Then configure in `~/.config/nx/config.toml`.

## 📁 Data Storage

nx follows XDG Base Directory specification:

```
~/.local/share/nx/
  notes/                    # Your notes (01J8...-title.md)
  attachments/              # File attachments
  .nx/
    index.sqlite           # Search index
    templates/             # Note templates
    trash/                 # Soft deletes

~/.config/nx/
  config.toml              # Configuration

~/.cache/nx/               # Temporary files
```

## 🔄 Current Status & Roadmap

### ✅ **MVP1 Complete** 
- Core note CRUD operations
- Full-text search with SQLite FTS5
- Interactive TUI with 3-pane layout
- Comprehensive AI integration
- Basic export functionality
- Git sync foundation
- Encryption foundation

### 🚧 **MVP2 In Development** (see [mvp2-plan.md](./mvp2-plan.md))

**Phase 1 - Core Features:**
- [ ] File attachment system (`nx attach`)
- [ ] Directory import (`nx import dir`)
- [ ] Template management (`nx tpl`)
- [ ] Metadata commands (`nx meta`)
- [ ] System maintenance (`nx reindex`, `nx backup`, `nx gc`, `nx doctor`)

**Phase 2 - Power Features:**
- [ ] Wiki-style `[[links]]` support
- [ ] Advanced search (boolean queries, field-specific)
- [ ] Shell completions
- [ ] Enhanced export formats

**Phase 3 - Advanced:**
- [ ] Complete encryption workflow
- [ ] Advanced Git sync with conflict resolution
- [ ] Performance optimizations
- [ ] Automation hooks

## 🏗️ Architecture

nx is built with modern C++ practices:

- **C++23** with ranges, `std::expected`, and strong typing
- **Modular design**: Clean separation between CLI, core, storage, and UI
- **Performance-first**: SQLite FTS5 for search, optimized data structures
- **Local-first**: No network dependencies for core functionality
- **Composable**: Works with standard Unix tools and workflows

### Core Modules
- `nx::core` - Note, NoteId, Metadata models
- `nx::store` - Filesystem and attachment storage
- `nx::index` - Search indexing (SQLite FTS5, ripgrep)
- `nx::cli` - Command implementations
- `nx::tui` - Interactive terminal interface
- `nx::sync` - Git synchronization
- `nx::crypto` - Encryption with age/rage

## 🧪 Development

### Building
```bash
# Debug build with sanitizers
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=address

# Run tests
ctest --test-dir build --output-on-failure

# Run benchmarks
./build/tests/benchmarks/nx_bench
```

### Testing
```bash
# Unit tests
ctest --test-dir build -L unit

# Integration tests
ctest --test-dir build -L integration

# Performance tests
./build/tests/benchmarks/nx_bench corpus/
```

## 📄 License

[License TBD]

## 🤝 Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) for development guidelines.

## 📖 Documentation

- [MVP2 Development Plan](./mvp2-plan.md) - Roadmap and feature planning
- [UX Design](./nx-ux-plan.md) - TUI design and user experience
- [Technical Specification](./docs/nx_cpp_notes_cli_spec_with_ai.md) - Complete spec
- [Technical Debt Report](./tech-debt.md) - Code quality analysis

---

**nx** - Because your thoughts deserve a fast, secure, and powerful home. 🏠✨