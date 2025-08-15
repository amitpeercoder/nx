# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

nx is a high-performance Linux CLI Markdown notes application with AI integration. The project has completed MVP1 and is actively developing MVP2 features. The comprehensive technical specification is located at `docs/nx_cpp_notes_cli_spec_with_ai.md`.

## Technology Stack

- **Language**: Modern C++ (C++23 preferred, C++20 fallback)
- **Build System**: CMake + Ninja
- **Package Manager**: vcpkg
- **CLI Framework**: CLI11
- **Database**: SQLite with FTS5
- **Configuration**: toml++ (TOML files)
- **YAML**: yaml-cpp
- **JSON**: nlohmann/json
- **Logging**: spdlog
- **Version Control**: libgit2
- **Encryption**: age/rage (via shell-out)

## Architecture

The project follows a modular architecture with clear separation of concerns:

- `src/app/` - Application entry point and CLI wiring
- `src/cli/` - CLI command implementations (22+ commands implemented)
- `src/core/` - Core domain models (Note, NoteId, Metadata)
- `src/store/` - Filesystem operations and note storage
- `src/index/` - Search indexing (SQLite FTS5, ripgrep fallback)
- `src/tui/` - Interactive Terminal User Interface with 3-pane layout
- `src/sync/` - Git synchronization
- `src/crypto/` - Encryption interface  
- `src/import_export/` - Import/export functionality (Markdown, JSON, PDF, HTML)
- `src/util/` - Utilities and helpers

## Development Commands

### Build System (Active)
```bash
# Configure with CMake (Release build)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Debug build with sanitizers
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=address

# Build
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
ctest --test-dir build -R test_name

# Install
cmake --install build

# Package (.deb, .rpm)
cd build && cpack
```

### Testing Commands
```bash
# Unit tests only
ctest --test-dir build -L unit

# Integration tests
ctest --test-dir build -L integration

# Benchmarks
./build/tests/benchmarks/nx_bench

# Fuzz testing
./build/tests/fuzz/fuzz_parser corpus/
```

### Code Quality Tools
```bash
# Format code
clang-format -i src/**/*.cpp src/**/*.hpp

# Static analysis
clang-tidy src/**/*.cpp -- -I include/

# Check with cppcheck
cppcheck --enable=all src/
```

## Key Design Principles

1. **Local-first**: Full offline functionality with optional Git sync
2. **Performance**: All operations < 100ms on 10,000 notes
3. **Plain Text**: Markdown files with YAML front-matter
4. **XDG Compliance**: Follows XDG Base Directory specification
5. **Error Handling**: Use `expected<T, Error>` pattern, no exceptions

## File Format

Notes are stored as Markdown files with optional YAML front-matter:
```markdown
---
id: <ULID>
created: <ISO-8601>
modified: <ISO-8601>
tags: [tag1, tag2]
---

# Note Title

Note content...
```

## Core CLI Commands

### ✅ Implemented Commands

#### Note Operations
```bash
nx new [title] [--tags a,b] [--nb notebook]    # Create new note
nx edit <id>                                     # Edit note in $EDITOR  
nx view <id>                                     # Display note
nx ls [--tag=X] [--since=Y] [--nb=Z]           # List notes with filtering
nx rm <id>                                       # Delete note
nx mv <id> --nb <notebook>                       # Move note to notebook
nx open <fuzzy>                                  # Fuzzy find and open note
```

#### Search & Discovery
```bash
nx grep <query> [--regex] [--content]           # Search note content
nx backlinks <id>                                # Show note backlinks
nx tags                                          # List all tags with counts
```

#### Export System
```bash
nx export md|json|pdf|html [--to dir]           # Export notes in various formats
```

#### AI Features (Optional)
```bash
nx ask "question"                                # RAG Q&A over note collection
nx summarize <id> [--style bullets] [--apply]   # AI summarization
nx tag-suggest <id> [--apply]                    # AI tag suggestions
nx title <id> [--apply]                          # AI title suggestions
nx rewrite <id> [--tone crisp]                   # AI content rewriting
nx tasks <id>                                    # Extract action items
nx suggest-links <id>                            # Find related notes
nx outline "topic"                               # Generate structured outlines
```

#### Interactive TUI
```bash
nx ui                                            # Launch 3-pane terminal interface
nx                                               # Auto-launch TUI if notes exist
```

### ✅ MVP2 Phase 1 Commands (Complete)
```bash
# File Attachments
nx attach <id> <file> [--name "Custom Name"]     # Attach files to notes

# Directory Import
nx import dir <path> [--format obsidian|notion] [--recursive] # Import directory of notes

# Template Management
nx tpl list                                      # List all templates
nx tpl add <name> [--file template.md]           # Create template
nx tpl remove <name>                             # Remove template

# Metadata Management
nx meta <id> [--set key=val]                     # View or modify metadata
nx meta <id> --remove <key>                      # Remove metadata key
nx meta <id> --list                              # List all metadata

# System Maintenance
nx reindex [rebuild|optimize|validate|stats]     # Search index management
nx backup [create|list|restore|verify] [file]    # Backup operations
nx gc [cleanup|optimize|vacuum|stats|all]        # Garbage collection
nx doctor [--quick] [--category] [--fix]         # System health checks

# Notebook Management (Complete)
nx notebook list [--json]                        # List all notebooks
nx notebook create <name> [description]          # Create new notebook
nx notebook rename <old> <new>                   # Rename notebook
nx notebook delete <name> [--force]              # Delete notebook
nx notebook info <name>                          # Show notebook details
```

### 🚧 MVP2 Phase 2 Commands (Next)
```bash
nx config get/set <key> [value]                  # Configuration management
nx graph --export dot                            # Export relationship graph
Advanced search with boolean queries (AND/OR/NOT)
Wiki-style [[links]] support with auto-completion
Shell completions for bash/zsh
```

## Performance Requirements

- Note creation/update: P95 < 100ms on 10k notes
- Search (FTS queries): P95 < 200ms
- List operations: P95 < 100ms
- Full reindex: < 45s on mid-range laptop
- Import/export: < 1s per 100 notes
- Memory usage: < 100MB for typical operations

## Security and Encryption

- Per-file encryption using age/rage (shell-out approach)
- No plaintext written to persistent disk when encryption enabled
- Use O_TMPFILE for secure temporary files on Linux
- Atomic file operations with rename()
- Keys stored in XDG_DATA_HOME with 0600 permissions

## Implementation Status

**Current Phase**: MVP1 Complete, MVP2 Phase 1 Complete

### ✅ MVP1 Completed Features
- **Core Note Management**: Create, edit, view, delete notes with ULID-based identification
- **Interactive TUI**: Full 3-pane hierarchical interface with notebook/tag navigation
- **Search System**: SQLite FTS5 with ripgrep fallback for full-text search
- **AI Integration**: Complete Claude/GPT integration for Q&A, summarization, tagging, etc.
- **Export System**: Markdown, JSON, PDF, HTML export with date filtering
- **Tag Management**: Manual and AI-powered tag operations
- **Git Sync**: Basic synchronization support
- **Encryption**: Foundation with age/rage integration

### ✅ MVP2 Phase 1 Complete Features
- **Notebook Management**: Full CRUD operations for organizing notes into collections
- **File Attachment System**: Complete `nx attach` with TUI integration and file management
- **Directory Import System**: `nx import dir` with support for Obsidian, Notion, and generic Markdown
- **Template Management**: `nx tpl` for creating, managing, and using note templates
- **Metadata Management**: `nx meta` for flexible key-value metadata operations
- **System Maintenance Suite**: Complete set of maintenance and diagnostic commands
  - **Search Index Management**: `nx reindex` with rebuild, optimize, validate, stats operations
  - **Backup System**: `nx backup` with create, list, restore, verify, cleanup operations
  - **Garbage Collection**: `nx gc` with cleanup, optimize, vacuum, stats operations
  - **Health Diagnostics**: `nx doctor` with comprehensive system health checks

### 🚧 MVP2 Phase 2 - Power Features (Next)
- **Wiki-links**: `[[note-title]]` syntax with auto-completion and link resolution
- **Advanced Search**: Boolean queries (AND/OR/NOT) with field-specific search
- **Shell Integration**: Bash/zsh completions and better Unix tool integration
- **Configuration Management**: `nx config` command for settings management
- **Enhanced Export**: More formats and advanced filtering options

### 🚧 MVP2 Phase 3 - Advanced Features (Future)
- **Complete Encryption**: Seamless per-file encryption with transparent operations
- **Advanced Git Sync**: Automatic conflict resolution and merge strategies
- **Performance Optimizations**: Support for 100k+ notes with sub-50ms operations
- **Automation & Scripting**: Hooks, triggers, and extensibility framework

### 📊 Current Metrics
- **Commands Implemented**: 35+ core commands across all functional areas
- **Lines of Code**: ~18,000+ lines of modern C++
- **Test Coverage**: Comprehensive unit tests, integration tests, benchmarks
- **Performance**: Meeting sub-100ms targets on 10k+ note collections
- **Features Complete**: MVP1 (100%) + MVP2 Phase 1 (100%)

Refer to `docs/nx_cpp_notes_cli_spec_with_ai.md` for the complete technical specification and `mvp2-plan.md` for the development roadmap.