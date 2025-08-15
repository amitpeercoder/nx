# Changelog

All notable changes to nx will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- AI auto-tag functionality in TUI (press `a` on selected note)
- AI auto-title functionality in TUI (press `A` on selected note)
- Dynamic tags panel sizing to use available vertical space
- Enhanced tag scrolling with visual indicators
- Comprehensive help panel with all keyboard shortcuts

### Changed
- Tags panel now auto-grows to fill available space
- Improved tag navigation with automatic scrolling
- Updated help documentation with AI features

### Fixed
- Tags list scrolling and space utilization
- Panel resizing with minimum width constraints
- Tag display truncation for long tag lists

## [0.1.0-alpha] - 2024-08-15

### Added
- **Core Note Management**
  - Create, edit, view, delete notes
  - ULID-based note identification
  - YAML front-matter support
  - Markdown content storage

- **Search & Discovery**
  - Full-text search with SQLite FTS5
  - Ripgrep fallback for text search
  - Tag-based filtering
  - Backlink discovery
  - Fuzzy note opening

- **Interactive TUI**
  - Three-pane layout (tags, notes, preview)
  - Real-time search
  - Keyboard navigation
  - Note editing with built-in editor
  - Panel resizing

- **AI Integration** (Optional)
  - Ask questions over note collection (RAG)
  - Auto-summarization with multiple styles
  - Tag suggestions based on content
  - Title suggestions and improvements
  - Content rewriting with tone adjustment
  - Task extraction from notes
  - Link suggestions between related notes
  - Outline generation for topics

- **Export System**
  - Markdown export
  - JSON export with metadata
  - PDF export (via pandoc/wkhtmltopdf)
  - HTML export with styling
  - Date-based filtering for exports

- **Storage & Organization**
  - XDG Base Directory compliance
  - Notebook organization
  - Tag management
  - Attachment storage interface
  - Git synchronization foundation

- **Configuration**
  - TOML-based configuration
  - Environment variable support
  - AI provider configuration (Anthropic/OpenAI)
  - Customizable editor settings

### Technical Features
- **Performance**: Sub-100ms operations on typical datasets
- **Security**: Basic encryption support with age/rage
- **Architecture**: Modern C++20/23 with clean modular design
- **Testing**: Unit tests, integration tests, and benchmarks
- **Build System**: CMake with vcpkg dependency management

### Commands Implemented
- `nx new` - Create notes with optional templates
- `nx edit` - Edit notes in $EDITOR
- `nx view` - Display notes with formatting
- `nx ls` - List notes with filtering
- `nx rm` - Delete notes (soft delete)
- `nx mv` - Move notes between notebooks
- `nx grep` - Search note content
- `nx open` - Fuzzy find and open notes
- `nx backlinks` - Show note relationships
- `nx tags` - Tag management
- `nx export` - Export in multiple formats
- `nx ui` - Launch interactive TUI
- `nx ask` - AI-powered Q&A over notes
- `nx summarize` - AI summarization
- `nx tag-suggest` - AI tag suggestions
- `nx title` - AI title suggestions
- `nx rewrite` - AI content rewriting
- `nx tasks` - Extract action items
- `nx suggest-links` - Find related notes
- `nx outline` - Generate topic outlines

### Known Limitations
- Encryption workflow is basic and needs completion
- Git sync has minimal conflict resolution
- Import functionality not implemented
- Template management system incomplete
- System maintenance commands missing

## Roadmap

### MVP2 - Core Completion (Target: Q4 2024)
- File attachment system (`nx attach`)
- Directory import (`nx import dir`)
- Template management (`nx tpl add/list/remove`)
- Metadata commands (`nx meta`)
- System maintenance (`nx reindex`, `nx backup`, `nx gc`, `nx doctor`)

### Future Releases
- Wiki-style `[[links]]` support
- Advanced search with boolean queries
- Shell completions for bash/zsh
- Performance optimizations for large datasets
- Enhanced Git sync with conflict resolution
- Plugin system for extensibility

---

## Development Notes

### Performance Targets
- Note operations: < 100ms P95 on 10k notes
- Search queries: < 200ms P95
- Full reindex: < 45s on mid-range hardware
- Memory usage: < 100MB for typical operations

### Security Considerations
- Command injection vulnerabilities identified and need fixing
- API key handling needs improvement
- Temporary file security requires hardening

### Architecture Principles
- Local-first design
- No network dependencies for core functionality
- Composable with Unix tools
- Clean separation of concerns
- Modern C++ best practices