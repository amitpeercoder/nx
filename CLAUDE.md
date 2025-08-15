# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

nx is a Linux CLI Markdown notes application currently in the specification phase. The comprehensive technical specification is located at `docs/nx_cpp_notes_cli_spec_with_ai.md`.

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
- `src/cli/` - CLI command implementations
- `src/core/` - Core domain models (Note, NoteId, Metadata)
- `src/store/` - Filesystem operations and note storage
- `src/index/` - Search indexing (SQLite FTS5, ripgrep fallback)
- `src/sync/` - Git synchronization
- `src/crypto/` - Encryption interface
- `src/import_export/` - Import/export functionality
- `src/util/` - Utilities and helpers

## Development Commands

### Build System (Planned)
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

### Note Operations
```bash
nx new [title]                    # Create new note
nx edit <id>                       # Edit note in $EDITOR
nx show <id>                       # Display note
nx ls [--tag=X] [--before=Y]     # List notes
nx rm <id>                        # Delete note
nx search <query>                 # Full-text search
```

### Advanced Features
```bash
nx tpl add <name> <file>          # Add template
nx new --from <template>          # Create from template
nx meta <id> --set key=val       # Set metadata
nx backlinks <id>                 # Show backlinks
nx sync [push|pull]               # Git sync
nx encrypt <id>                   # Encrypt note
nx decrypt <id>                   # Decrypt note
```

### Import/Export
```bash
nx import dir <path>              # Import directory
nx export md|zip|json [--to dir]  # Export notes
```

### AI Features (Optional)
```bash
nx ask "question"                 # RAG Q&A
nx ai summarize <id>              # Auto-summarize
nx ai tags <id>                   # Auto-tag
nx ai rewrite <id> --style=X     # Rewrite content
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

**Current Phase**: Specification only. The implementation has not begun. Refer to `docs/nx_cpp_notes_cli_spec_with_ai.md` for the complete technical specification including MVP features, AI integration, architecture details, and implementation roadmap.