# nx - User Manual

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Getting Started](#getting-started)
4. [Core Concepts](#core-concepts)
5. [Command Reference](#command-reference)
6. [Terminal User Interface (TUI)](#terminal-user-interface-tui)
7. [Advanced Features](#advanced-features)
8. [Configuration](#configuration)
9. [Troubleshooting](#troubleshooting)
10. [Tips and Best Practices](#tips-and-best-practices)

## Introduction

**nx** is a high-performance command-line Markdown notes application designed for developers, researchers, and knowledge workers who prefer working in the terminal. It combines the power of plain-text Markdown files with advanced features like AI integration, full-text search, and an intuitive Terminal User Interface (TUI).

### Key Features

- **Plain Text**: All notes stored as Markdown files with YAML front-matter
- **Local-First**: Full offline functionality with optional Git synchronization
- **High Performance**: Sub-50ms operations on collections of 10,000+ notes
- **Interactive TUI**: 3-pane interface for browsing, editing, and organizing notes
- **AI Integration**: Claude and GPT support for summarization, tagging, and Q&A
- **Powerful Search**: SQLite FTS5 with ripgrep fallback for lightning-fast text search
- **Rich Organization**: Notebooks, tags, templates, and file attachments
- **Export Options**: Multiple formats including Markdown, JSON, PDF, and HTML

## Installation

### Prerequisites

- **Operating System**: Linux or macOS
- **Dependencies**: Modern C++ compiler, CMake, vcpkg

### Building from Source

```bash
# Clone the repository
git clone <repository-url>
cd nx

# Configure build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Install (optional)
cmake --install build
```

### Verification

```bash
# Check installation
nx --version

# Run health check
nx doctor
```

## Getting Started

### First Steps

1. **Create your first note**:
   ```bash
   nx new "My First Note" --tags getting-started
   ```

2. **Launch the interactive interface**:
   ```bash
   nx ui
   ```

3. **List your notes**:
   ```bash
   nx ls
   ```

4. **Search for notes**:
   ```bash
   nx grep "first"
   ```

### File Structure

nx stores notes in a structured directory format:

```
~/.local/share/nx/
├── notes/
│   ├── 01H2ABC3DEF4GHI5JKL6MNO7PQ.md  # Individual note files
│   └── ...
├── .nx/
│   ├── config.toml                     # Configuration
│   ├── index.db                        # Search index
│   └── attachments/                    # File attachments
└── notebooks/                          # Notebook organization
```

## Core Concepts

### Notes

Notes are the fundamental unit in nx. Each note:
- Has a unique ULID (Universally Unique Lexicographically Sortable Identifier)
- Contains Markdown content with optional YAML front-matter
- Can belong to one notebook and have multiple tags
- Supports file attachments and internal links

**Example note structure**:
```markdown
---
id: 01H2ABC3DEF4GHI5JKL6MNO7PQ
title: "My Note Title"
created: 2024-01-15T10:30:00Z
modified: 2024-01-15T11:45:00Z
tags: [project, important]
notebook: work
---

# My Note Title

This is the note content written in **Markdown**.

- Support for lists
- *Emphasis* and **strong** text
- [Links](https://example.com)
- Code blocks and more
```

### Notebooks

Notebooks provide hierarchical organization for your notes:
- Think of them as folders or categories
- Each note can belong to one notebook
- Notebooks can have descriptions and metadata
- Use for project organization, topic separation, etc.

### Tags

Tags offer flexible, multi-dimensional organization:
- Notes can have multiple tags
- Use for cross-cutting concerns (priority, status, topic)
- Support both manual and AI-generated tagging
- Searchable and filterable

### Templates

Templates help create consistent note structures:
- Define reusable note formats
- Support placeholders and variables
- Ideal for meeting notes, project planning, etc.

## Command Reference

### Note Operations

#### Create Notes
```bash
# Basic note creation
nx new "Note Title"

# With tags and notebook
nx new "Meeting Notes" --tags meeting,work --nb project-alpha

# From template
nx new "Weekly Review" --template weekly-review
```

#### View and Edit Notes
```bash
# View a note (use fuzzy matching)
nx view meeting

# Edit in your default editor
nx edit abc123

# Open with fuzzy search
nx open "project"
```

#### List and Search Notes
```bash
# List all notes
nx ls

# Filter by tag
nx ls --tag work

# Filter by date
nx ls --since 2024-01-01

# Search content
nx grep "important project"

# Regex search
nx grep "TODO.*urgent" --regex
```

#### Delete and Move Notes
```bash
# Delete a note
nx rm abc123

# Move to different notebook
nx mv abc123 --nb archive
```

### Organization

#### Notebook Management
```bash
# List notebooks
nx notebook list

# Create notebook
nx notebook create "Project Alpha" "Main project workspace"

# Rename notebook
nx notebook rename "old-name" "new-name"

# Delete notebook
nx notebook delete "archive" --force

# Get notebook info
nx notebook info "project-alpha"
```

#### Tag Management
```bash
# List all tags
nx tags

# Add tags to note
nx meta abc123 --set tags=work,urgent

# Remove tag
nx meta abc123 --remove tag
```

#### Template Management
```bash
# List templates
nx tpl list

# Create template
nx tpl add "meeting" --file meeting-template.md

# Remove template
nx tpl remove "old-template"
```

### File Attachments

```bash
# Attach file to note
nx attach abc123 /path/to/document.pdf

# Attach with custom name
nx attach abc123 /path/to/file.jpg --name "Project Diagram"
```

### Import and Export

#### Import
```bash
# Import directory of Markdown files
nx import dir /path/to/notes --format obsidian

# Recursive import
nx import dir /path/to/notes --recursive
```

#### Export
```bash
# Export all notes to Markdown
nx export md --to /path/to/backup

# Export specific timeframe to PDF
nx export pdf --since 2024-01-01 --to /path/to/pdfs

# Export as JSON
nx export json --to data.json
```

### AI Features

```bash
# Ask questions about your notes
nx ask "What are my main project goals?"

# Summarize a note
nx summarize abc123 --style bullets --apply

# Get AI tag suggestions
nx tag-suggest abc123 --apply

# Generate better titles
nx title abc123 --apply

# Rewrite content
nx rewrite abc123 --tone professional --apply

# Extract action items
nx tasks abc123 --priority high

# Find related notes
nx suggest-links abc123 --apply

# Generate topic outline
nx outline "Project Management" --create
```

### System Maintenance

#### Search Index Management
```bash
# Rebuild search index
nx reindex rebuild

# Optimize index
nx reindex optimize

# Validate index
nx reindex validate

# Show index statistics
nx reindex stats
```

#### Backup Operations
```bash
# Create backup
nx backup create

# List backups
nx backup list

# Restore from backup
nx backup restore backup-2024-01-15.tar.gz

# Verify backup integrity
nx backup verify backup-2024-01-15.tar.gz
```

#### Garbage Collection
```bash
# Run all cleanup operations
nx gc all

# Clean orphaned files
nx gc cleanup

# Optimize database
nx gc optimize

# Show storage statistics
nx gc stats
```

#### Health Diagnostics
```bash
# Quick health check
nx doctor --quick

# Full system diagnosis
nx doctor

# Fix common issues
nx doctor --fix

# Check specific category
nx doctor --category index
```

## Terminal User Interface (TUI)

### Launching the TUI

```bash
# Launch interactive interface
nx ui

# Auto-launch if notes exist
nx
```

### Interface Layout

The TUI features a 3-pane layout:

```
┌─ Navigation ─┬─ Notes ─────────┬─ Preview/Editor ─┐
│ Notebooks   │ Note List       │ Content Display  │
│ Tags        │ Search Results  │ or Editor        │
│ Filters     │ Metadata        │                  │
└─────────────┴─────────────────┴──────────────────┘
```

### Navigation

#### Keyboard Shortcuts

**General Navigation**:
- `Tab` / `Shift+Tab`: Move between panes
- `↑` / `↓`: Navigate within pane
- `Enter`: Select/open item
- `q`: Quit application
- `?`: Show help

**Notes Pane**:
- `/`: Search notes
- `n`: Create new note
- `e`: Edit selected note
- `d`: Delete selected note
- `t`: Add/edit tags

**Editor Mode**:
- `Ctrl+S`: Save note
- `Esc`: Exit editor mode
- Standard editing keys (arrows, backspace, etc.)

**Search**:
- `/`: Start search
- `Enter`: Execute search
- `Esc`: Clear search

### Editor Features

The built-in editor includes:
- **Syntax highlighting** for Markdown
- **Auto-completion** for common patterns
- **Unicode support** for international text
- **Undo/redo** functionality
- **Large file support** (1GB+)
- **Virtual scrolling** for performance

## Advanced Features

### Metadata Management

```bash
# View note metadata
nx meta abc123

# Set custom field
nx meta abc123 --set priority=high

# Remove field
nx meta abc123 --remove priority

# List all metadata
nx meta abc123 --list
```

### Backlinking

```bash
# Find notes linking to current note
nx backlinks abc123
```

### Git Synchronization

```bash
# Sync with Git repository
nx sync push
nx sync pull
```

### Configuration Management

```bash
# View configuration
nx config get

# Set configuration value
nx config set editor.default_tags "draft,new"

# Reset to defaults
nx config reset
```

## Configuration

### Configuration File

nx uses TOML format for configuration:

```toml
# ~/.local/share/nx/.nx/config.toml

[notes]
default_notebook = "inbox"
auto_tags = ["draft"]

[editor]
default_editor = "vim"
line_numbers = true
tab_size = 2

[ai]
provider = "claude"
model = "claude-3-sonnet"
api_key_file = "~/.config/nx/api-key"

[search]
engine = "fts5"
fallback_to_ripgrep = true

[export]
default_format = "markdown"
include_metadata = true
```

### Environment Variables

- `NX_NOTES_DIR`: Override default notes directory
- `NX_CONFIG_DIR`: Override configuration directory
- `EDITOR`: Default text editor for editing notes

## Troubleshooting

### Common Issues

#### Search Not Working
```bash
# Rebuild search index
nx reindex rebuild

# Check index health
nx doctor --category index
```

#### Performance Issues
```bash
# Run optimization
nx gc optimize

# Check system resources
nx doctor --category performance
```

#### Corrupted Data
```bash
# Validate data integrity
nx doctor --category data

# Restore from backup
nx backup restore latest
```

#### AI Features Not Working
```bash
# Check AI configuration
nx config get ai

# Test API connectivity
nx doctor --category ai
```

### Getting Help

```bash
# Command help
nx --help
nx <command> --help

# System health check
nx doctor

# Community support
# Check GitHub issues and discussions
```

## Tips and Best Practices

### Organization

1. **Use consistent naming**: Develop a consistent approach to note titles
2. **Leverage notebooks**: Group related notes into logical notebooks
3. **Tag strategically**: Use tags for cross-cutting concerns (status, priority, type)
4. **Regular cleanup**: Periodically run `nx gc all` to optimize storage

### Workflow

1. **Daily notes**: Create daily notes for ongoing capture
2. **Template usage**: Develop templates for recurring note types
3. **Regular search**: Use search to rediscover forgotten notes
4. **Link notes**: Create connections between related notes

### Performance

1. **Index maintenance**: Run `nx reindex optimize` weekly
2. **Backup regularly**: Use `nx backup create` for data safety
3. **Monitor health**: Run `nx doctor` to catch issues early
4. **Clean up**: Remove unused attachments and orphaned files

### AI Integration

1. **Consistent tagging**: Let AI suggest tags for better organization
2. **Content improvement**: Use rewriting features to enhance clarity
3. **Knowledge extraction**: Use summarization for quick overviews
4. **Discovery**: Use AI to find connections between notes

---

For more detailed technical information, see the [technical specification](nx_cpp_notes_cli_spec_with_ai.md) and [development documentation](../CLAUDE.md).