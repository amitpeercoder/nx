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

**nx** is a high-performance command-line Markdown notes application designed for developers, researchers, and knowledge workers who prefer working in the terminal. Version 1.0.0 represents a complete, production-ready system that combines the power of plain-text Markdown files with advanced features like AI integration, full-text search, and an intuitive Terminal User Interface (TUI).

### Key Features

- **Plain Text**: All notes stored as Markdown files with YAML front-matter
- **Local-First**: Full offline functionality with optional Git synchronization
- **High Performance**: Sub-50ms operations on collections of 10,000+ notes
- **Interactive TUI**: 3-pane interface for browsing, editing, and organizing notes
- **Auto-Title Derivation**: Note titles automatically derived from first line of content
- **AI Integration**: Claude and GPT support for summarization, tagging, and Q&A
- **Powerful Search**: SQLite FTS5 with ripgrep fallback for lightning-fast text search
- **Rich Organization**: Notebooks, tags, templates, and file attachments
- **Export Options**: Multiple formats including Markdown, JSON, PDF, and HTML

## Installation

### Quick Install (Recommended)

Download the latest release and run the install script:

```bash
# Download release package (replace with your platform)
wget https://github.com/amitpeercoder/nx/releases/latest/download/nx-release-v1.0.0-darwin-arm64.tar.gz

# Extract and install
tar -xzf nx-release-v1.0.0-darwin-arm64.tar.gz
cd nx-release-v1.0.0-darwin-arm64
sudo ./install.sh
```

### Prerequisites

- **Operating System**: Linux or macOS (ARM64/x86_64)
- **Runtime Dependencies**: libsqlite3, ripgrep (optional), age/rage (for encryption)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/amitpeercoder/nx.git
cd nx

# Configure release build
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build-release

# Install system-wide
cd build-release
sudo ./install.sh
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
- **Automatic titles**: Title is automatically derived from the first line of content

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

### Title Derivation

nx automatically derives note titles from the first line of content, making note management seamless:

- **Markdown headers**: `# My Title` becomes "My Title"
- **Plain text**: Any first line becomes the title
- **Smart processing**: Removes markdown syntax and extra whitespace
- **Length limit**: Titles are capped at 200 characters
- **Fallback**: Empty notes get "Untitled" as title

**Examples**:
```markdown
# Meeting Notes 2024-03-15    →  Title: "Meeting Notes 2024-03-15"
## Project Planning           →  Title: "Project Planning" 
Weekly Review                 →  Title: "Weekly Review"
```

This means:
- **No manual title entry required** when creating notes
- **Titles update automatically** as you edit the first line
- **Consistent title handling** across CLI and TUI
- **No separate rename function needed** - just edit the first line

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
# Basic note creation (title will be derived from first line of content)
nx new

# With tags and notebook  
nx new --tags meeting,work --nb project-alpha

# From template
nx new --template weekly-review
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

# List notebooks
nx notebook list

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

# Rewrite content
nx rewrite abc123 --tone professional --apply

# Extract action items
nx tasks abc123 --priority high

# Find related notes
nx suggest-links abc123 --apply

# Generate topic outline
nx outline "Project Management" --create
```

**AI Explanation Feature in TUI**:

The TUI editor includes an AI-powered explanation feature to help understand technical terms and abbreviations:

1. **Brief Explanations**: Place cursor after a term and press `Alt+?` to get a concise explanation (5-10 words)
2. **Detailed Explanations**: Press `Ctrl+E` to expand a brief explanation into 2-3 sentences
3. **Context-Aware**: Uses surrounding text to provide more accurate explanations
4. **Caching**: Explanations are cached to reduce API calls and improve performance

Example workflow:
1. Type "API" in your note
2. Place cursor after "API" 
3. Press `Alt+?` → "Application Programming Interface"
4. Press `Ctrl+E` → "A set of protocols and tools for building software applications. APIs define how different software components should interact with each other."

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
- `n`: Create new note (title derived from first line)
- `e`: Edit selected note
- `d`: Delete selected note
- `r`: Refresh data
- `t`: Add/edit tags

**Editor Mode**:
- `Ctrl+S`: Save note
- `Esc`: Exit editor mode

**AI Features (Phase 1-2)**:
- `Ctrl+Q`: Generate brief AI explanation for term before cursor
- `Ctrl+E`: Expand brief explanation to detailed explanation
- `Ctrl+W`: Smart completion based on context and content
- `Ctrl+G`: Grammar & style check with configurable preferences
- `S`: Semantic search - find notes by meaning, not keywords
- `Ctrl+X`: Generate smart examples for concepts and terms
- `Ctrl+C`: Code generation from natural language descriptions
- `Ctrl+U`: Smart summarization of entire note

**AI Features (Phase 3)**:
- `Ctrl+R`: Note relationships analysis - discover connections
- `Ctrl+H`: Content enhancement suggestions
- `Ctrl+O`: Smart organization recommendations
- `Ctrl+A`: Research assistant for topic exploration
- `Ctrl+B`: Writing coach with comprehensive feedback

**AI Features (Phase 4)**:
- `Ctrl+G`: Smart content generation based on topic and context
- `Ctrl+T`: Intelligent template suggestions based on content analysis
- `Ctrl+I`: Cross-note insights - find patterns across notes
- `Ctrl+N`: Smart search enhancement with AI-powered expansion
- `Ctrl+M`: Smart note merging - detect consolidation opportunities

**AI Features (Phase 5)**:
- `Ctrl+F1`: Workflow orchestrator - automate multi-step processes
- `Ctrl+F2`: Project structure analyzer - analyze and improve organization
- `Ctrl+F3`: Learning path generator - create structured learning plans
- `Ctrl+F4`: Knowledge synthesizer - combine insights from multiple notes
- `Ctrl+F5`: Journal pattern analyzer - discover temporal patterns and insights

**AI Features (Phase 6 - Advanced AI Integration)**:
- `F6`: Multi-modal analysis - analyze notes with attached images and documents
- `F7`: Voice integration - process voice commands for note operations (demo mode)
- `F8`: Contextual awareness - analyze reading patterns and suggest related content
- `F9`: Workspace AI - optimize note organization and suggest improvements
- `F10`: Predictive AI - predict user needs and provide proactive recommendations

**AI Features (Phase 7 - Collaborative Intelligence & Knowledge Networks)**:
- `F11`: Collaborative AI - analyze team dynamics and suggest collaboration improvements
- `F12`: Knowledge Graphs - generate and visualize concept relationships across notes
- `Shift+E`: Expert Systems - get domain-specific advice and recommendations
- `Shift+S`: Intelligent Workflows - automate complex multi-step processes
- `Shift+M`: Meta-Learning - analyze learning patterns and optimize knowledge retention

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

## Practical Workflows

This section provides comprehensive examples and workflows for common use cases.

### Getting Started Workflows

#### First-Time Setup
```bash
# 1. Create your first note
nx new "Welcome to nx" --tags getting-started --edit

# 2. Launch the TUI to explore
nx ui

# 3. Set up Git sync (optional)
nx sync init --remote https://github.com/yourusername/my-notes.git \
  --user-name "Your Name" --user-email "you@example.com"

# 4. Create a notebook structure
nx notebook create "Personal" "Personal notes and thoughts"
nx notebook create "Work" "Work-related notes and projects"
nx notebook create "Learning" "Study notes and research"
```

#### Daily Note-Taking Workflow
```bash
# Morning: Create daily journal
nx new "$(date +%Y-%m-%d)" --nb journal --tags daily \
  --template daily-journal --edit

# Quick capture throughout the day
echo "Meeting insight: Focus on user needs" | \
  nx new "Quick Capture $(date +%H:%M)" --tags inbox

# Evening: Process inbox
nx ls --tag inbox | head -5 | while read id; do
  nx tag-suggest "$id" --apply
  nx mv "$id" --nb appropriate-notebook
done
```

### Content Creation Workflows

#### Research and Study Notes
```bash
# 1. Create research note with AI outline
nx outline "Machine Learning Fundamentals" --create
note_id=$(nx ls --limit 1 | head -1 | awk '{print $1}')

# 2. Attach research papers
nx attach "$note_id" ~/Downloads/paper1.pdf --description "Core ML paper"
nx attach "$note_id" ~/Downloads/paper2.pdf --description "Applications"

# 3. Use AI to enhance learning
nx summarize "$note_id" --style bullets --apply
nx tag-suggest "$note_id" --apply
nx suggest-links "$note_id" --apply
```

#### Meeting Notes Workflow
```bash
# Pre-meeting: Create from template
nx new "Team Meeting $(date +%Y-%m-%d)" \
  --template meeting --nb work --tags meeting,team

# During meeting: Quick edits
note_id=$(nx ls --tag meeting --limit 1 | head -1 | awk '{print $1}')
nx edit "$note_id"

# Post-meeting: Extract actions
nx tasks "$note_id" --priority high
# Title is automatically derived from first line of content
```

#### Project Documentation
```bash
# Create project structure
nx notebook create "Project Alpha" "New product development"

# Create project notes from templates
nx new --template project-kickoff --nb "Project Alpha" \
  --tags project,alpha,planning
nx new --template technical-spec --nb "Project Alpha" \
  --tags project,alpha,technical
nx new --template meeting-notes --nb "Project Alpha" \
  --tags project,alpha,meetings

# Link related notes
project_notes=$(nx ls --nb "Project Alpha")
for note in $project_notes; do
  nx suggest-links "$note" --apply --threshold 0.7
done
```

### File Management Workflows

#### Document Import and Organization
```bash
# Import existing documentation
nx import dir ~/Documents/old-notes --nb imported --recursive
nx import obsidian ~/ObsidianVault --preserve-structure

# Bulk processing of imported notes
nx ls --nb imported | while read id; do
  # Auto-suggest better organization
  nx tag-suggest "$id" --apply
  
  # Titles are automatically derived from first line
  
  # Find appropriate notebook
  tags=$(nx view "$id" --json | jq -r '.tags[]' | head -3)
  if echo "$tags" | grep -q "work"; then
    nx mv "$id" --nb "Work"
  elif echo "$tags" | grep -q "personal"; then
    nx mv "$id" --nb "Personal"
  fi
done
```

#### Attachment Management
```bash
# Attach multiple files to a project note
project_note=$(nx ls --tag project --limit 1 | head -1 | awk '{print $1}')

# Attach different file types
nx attach "$project_note" ~/Downloads/spec.pdf --description "Technical specification"
nx attach "$project_note" ~/Downloads/design.sketch --description "UI mockups"
nx attach "$project_note" ~/Downloads/data.csv --description "Analysis data"

# Bulk attach from directory
find ~/project-files -name "*.pdf" | while read file; do
  nx attach "$project_note" "$file" --description "$(basename "$file")"
done
```

### Search and Discovery Workflows

#### Advanced Search Techniques
```bash
# Multi-criteria search
nx grep "machine learning" --ignore-case | \
  xargs -I {} nx view {} --json | \
  jq -r 'select(.tags[] | contains("python")) | .id'

# Find notes by content patterns
nx grep "TODO|FIXME|HACK" --regex | \
  while read note_id; do
    title=$(nx view "$note_id" --json | jq -r .title)
    echo "Action needed in: $title"
  done

# Time-based discovery
nx ls --since "2024-01-01" --tag work | \
  head -10 | \
  xargs -I {} nx summarize {} --style bullets

# Find orphaned or under-tagged notes
nx ls | while read id; do
  tag_count=$(nx view "$id" --json | jq '.tags | length')
  if [ "$tag_count" -lt 2 ]; then
    echo "Under-tagged: $id"
    nx tag-suggest "$id" --apply
  fi
done
```

#### Knowledge Discovery
```bash
# Find connections across notebooks
nx ask "What are the common themes across my work notes?"
nx ask "Which personal projects relate to my professional work?"

# Rediscover forgotten notes
old_notes=$(nx ls --since "2023-01-01" | tail -20)
for note in $old_notes; do
  title=$(nx view "$note" --json | jq -r .title)
  echo "Rediscovered: $title"
  nx suggest-links "$note" --apply
done
```

### Maintenance Workflows

#### Weekly Maintenance
```bash
#!/bin/bash
# weekly-maintenance.sh

echo "🔧 Running weekly nx maintenance..."

# 1. Health check
nx doctor --fix --category storage,index

# 2. Optimize search index
nx reindex optimize

# 3. Clean up storage
nx gc cleanup --force

# 4. Create backup
backup_file="$HOME/backups/nx-weekly-$(date +%Y%m%d).tar.gz"
nx backup create "$backup_file" --compress

# 5. Sync changes
nx sync sync --message "Weekly maintenance $(date +%Y-%m-%d)"

# 6. Report statistics
echo "📊 Statistics:"
echo "Total notes: $(nx ls | wc -l)"
echo "Total notebooks: $(nx notebook list | wc -l)"
echo "Total tags: $(nx tags | wc -l)"

echo "✅ Weekly maintenance complete!"
```

#### Data Migration
```bash
# Migrate from other note systems
# From Obsidian
nx import obsidian ~/ObsidianVault --recursive --preserve-structure
nx ls --nb ObsidianVault | while read id; do
  nx tag-suggest "$id" --apply
  nx suggest-links "$id" --apply
done

# From Notion export
nx import notion ~/notion-export --nb notion-import
nx ls --nb notion-import | while read id; do
  nx rewrite "$id" --tone crisp --apply  # Clean up formatting
  # Titles automatically derived from first line
done

# Reorganize after import
nx ls --nb imported | while read id; do
  # Use AI to determine best notebook
  summary=$(nx summarize "$id" --style bullets)
  if echo "$summary" | grep -i "meeting\|standup\|team"; then
    nx mv "$id" --nb "Work"
  elif echo "$summary" | grep -i "personal\|journal\|diary"; then
    nx mv "$id" --nb "Personal"
  fi
done
```

### Collaboration Workflows

#### Team Note Sharing
```bash
# Set up team repository
nx sync init --remote git@github.com:team/shared-notes.git

# Daily team sync
nx sync pull  # Get team updates
# ... work on notes ...
nx sync sync --message "Daily updates from $(whoami) - $(date +%Y-%m-%d)"

# Share specific project notes
nx export md --nb "Project Alpha" --to ~/shared/project-alpha-export/
# Team members can then import: nx import dir ~/shared/project-alpha-export/
```

#### Review and Feedback
```bash
# Prepare notes for review
review_notes=$(nx ls --tag "needs-review")
for note in $review_notes; do
  # Clean up for review
  nx rewrite "$note" --tone professional --apply
  # Title automatically derived from first line
  
  # Export for sharing
  nx export pdf --note "$note" --to ~/reviews/
done
```

### Automation and Integration

#### Automated Capture
```bash
# Add to .bashrc or .zshrc for quick capture
note_quick() {
  echo "$*" | nx new "Quick: $(date +%H:%M)" --tags inbox,quick
}

# Use: note_quick "Remember to follow up on the client proposal"
```

#### Integration with Other Tools
```bash
# Capture from clipboard with processing  
content=$(pbpaste)
note_id=$(nx new --tags inbox)
echo "# Clipboard $(date +%Y-%m-%d %H:%M)" > temp_content.md
echo "" >> temp_content.md
echo "$content" >> temp_content.md
nx edit "$note_id" < temp_content.md
nx tag-suggest "$note_id" --apply
rm temp_content.md

# Create note from URL  
url_to_note() {
  local url="$1"
  local title=$(curl -s "$url" | grep -o '<title>[^<]*' | sed 's/<title>//')
  # Create note and add title as first line
  note_id=$(nx new --tags web,research)
  echo "# $title" > temp_content.md
  echo "" >> temp_content.md
  echo "Source: $url" >> temp_content.md
  nx edit "$note_id" < temp_content.md
  rm temp_content.md
}
```

### Performance Optimization Workflows

#### Large Collection Management
```bash
# For collections with 10k+ notes
# Optimize index regularly
nx reindex rebuild  # Monthly
nx reindex optimize  # Weekly

# Archive old notes
cutoff_date="2023-01-01"
old_notes=$(nx ls --until "$cutoff_date")
nx notebook create "Archive" "Archived notes"
echo "$old_notes" | while read id; do
  nx mv "$id" --nb "Archive"
done

# Compress archives
nx export md --nb "Archive" --to ~/archives/notes-archive-$(date +%Y)/
```

## Tips and Best Practices

### Organization

1. **Write meaningful first lines**: Since titles are derived from the first line, make it descriptive
2. **Use consistent first line patterns**: Develop consistent approaches (e.g., "# Meeting Notes - 2024-03-15")
3. **Leverage notebooks**: Group related notes into logical notebooks
4. **Tag strategically**: Use tags for cross-cutting concerns (status, priority, type)
5. **Regular cleanup**: Periodically run `nx gc all` to optimize storage

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