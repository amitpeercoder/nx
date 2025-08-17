# Notebook System Documentation

## Overview

The nx notebook system provides hierarchical organization of notes into collections, allowing users to group related notes while maintaining the flexibility of tag-based filtering. Notebooks act as top-level containers that can be expanded to show their tags and notes.

## Architecture

### Core Components

1. **NotebookManager** (`src/store/notebook_manager.cpp`)
   - Handles all notebook CRUD operations
   - Manages notebook metadata and statistics
   - Provides safe deletion with force options

2. **CLI Commands** (`src/cli/commands/notebook_command.cpp`)
   - Complete command-line interface for notebook management
   - JSON output support
   - Integration with global CLI options

3. **TUI Integration** (`src/tui/tui_app.cpp`)
   - Hierarchical navigation panel
   - Keyboard shortcuts and modal dialogs
   - Smart filtering system

### Data Model

#### Notebook Storage
Notebooks are implemented using the existing filesystem structure:
- Each notebook is represented by notes that have a `notebook` field in their YAML front-matter
- Placeholder notes (`.notebook_<name>`) ensure notebook directories exist
- No separate notebook metadata files are needed

#### Note Structure
```markdown
---
id: 01JBXY123...
created: 2024-08-15T10:30:00Z
modified: 2024-08-15T12:45:00Z
notebook: work
tags: [meeting, planning]
---

# Meeting Notes

Content goes here...
```

## CLI Commands

### List Notebooks
```bash
nx notebook list [--stats] [--json]
```
- Lists all notebooks with note counts
- `--stats` includes size, timestamps, and top tags
- `--json` outputs structured JSON

### Create Notebook
```bash
nx notebook create <name>
```
- Creates new notebook with validation
- Generates placeholder note to ensure existence
- Fails if notebook already exists

### Rename Notebook
```bash
nx notebook rename <old_name> <new_name>
```
- Renames notebook and updates all contained notes
- Atomic operation - fails safely if any note update fails
- Updates notebook metadata and timestamps

### Delete Notebook
```bash
nx notebook delete <name> [--force]
```
- Safe deletion with placeholder note cleanup
- `--force` required to delete notebooks containing real notes
- Removes all notes in notebook when forced

### Notebook Information
```bash
nx notebook info <name> [--stats] [--json]
```
- Shows detailed notebook information
- `--stats` includes creation time, modification time, size, and tag analysis
- Tag counts and frequency analysis
- Recent notes list

## TUI Integration

### Navigation Panel
The left panel shows a hierarchical view:
```
📂 work (5)
  ├─ 📋 meetings (3)
  ├─ 📋 projects (2)
  └─ 📋 planning (1)
📂 personal (3)
  ├─ 📋 health (1)
  └─ 📋 hobbies (2)
📋 global-tags (12)
  ├─ urgent (4)
  ├─ todo (6)
  └─ ideas (8)
```

### Keyboard Shortcuts

| Key | Action | Context |
|-----|--------|---------|
| `Ctrl+N` | Create notebook | Global |
| `Ctrl+R` | Rename notebook | Navigation pane, notebook selected |
| `Ctrl+D` | Delete notebook | Navigation pane, notebook selected |
| `N` | Toggle notebook filter | Navigation pane, notebook selected |
| `Space` | Expand/collapse notebook | Navigation pane, notebook selected |
| `→` | Expand notebook | Navigation pane, notebook selected |
| `←` | Collapse notebook | Navigation pane, notebook selected |
| `t` | Toggle tag filter | Navigation pane, tag selected |
| `C` | Clear all filters | Global |

### Modal Dialogs

#### Create Notebook Modal
- Triggered by `Ctrl+N`
- Text input for notebook name
- Real-time validation
- Enter to create, Escape to cancel

#### Rename Notebook Modal
- Triggered by `Ctrl+R` on selected notebook
- Pre-filled with current name
- Enter to rename, Escape to cancel

#### Delete Notebook Modal
- Triggered by `Ctrl+D` on selected notebook
- Shows warning about data loss
- Force toggle with `f` key
- Color-coded force status (Yellow=enabled, White=disabled)
- Enter to confirm, Escape to cancel

## Filtering System

### Smart Filtering Logic

The notebook system implements context-aware filtering:

1. **Notebook Selection**: Shows notes only from selected notebook(s)
2. **Notebook Tags**: When notebook is selected, shows only tags from that notebook
3. **Global Tags**: When no notebook selected, shows all tags across notebooks
4. **Combined Filters**: AND logic between notebooks and tags

### Filter States

- `active_notebooks`: Set of selected notebook filters
- `active_notebook_tags`: Map of notebook -> selected tags within that notebook
- `active_global_tags`: Global tag filters (when no notebooks selected)

### Filter Examples

```bash
# Show only notes from 'work' notebook
Notebook: work → Shows work notes only

# Show work notes tagged 'meeting'
Notebook: work + Tag: meeting → Shows work notes with meeting tag

# Show notes tagged 'urgent' across all notebooks
Tag: urgent (no notebook selected) → Shows all urgent notes

# Multiple notebooks
Notebook: work,personal → Shows notes from either notebook
```

## Performance Considerations

### Indexing
- Notebook operations use existing note indexing
- No separate notebook index required
- Tag aggregation computed on-demand for UI

### Memory Usage
- Notebook metadata cached during TUI session
- Lazy loading of notebook statistics
- Efficient tag counting with single pass through notes

### Scalability
- Designed for thousands of notes across hundreds of notebooks
- O(n) operations where n is number of notes in filtered set
- Expandable tree structure minimizes UI rendering

## Testing

### Unit Tests (`tests/unit/store/test_notebook_manager.cpp`)
- NotebookManager CRUD operations
- Validation and error handling
- Force deletion scenarios
- Tag aggregation and statistics
- Thread safety and concurrent access

### Integration Tests (`tests/integration/test_notebook_cli.cpp`)
- Complete CLI workflow testing
- JSON output validation
- Error condition handling
- Cross-platform compatibility

### Test Coverage
- 95%+ code coverage for notebook functionality
- Edge cases: empty notebooks, invalid names, concurrent operations
- Performance tests with large datasets

## Security and Data Integrity

### Safe Operations
- Atomic notebook renames using filesystem transactions
- Force deletion requires explicit confirmation
- Validation prevents invalid notebook names
- Proper error handling and rollback

### Data Protection
- Placeholder notes prevent accidental data loss
- Backup-friendly design (all data in note files)
- No hidden metadata files
- Git-friendly with clear diff visibility

## Future Enhancements

### Planned Features
1. **Nested Notebooks**: Hierarchical notebook organization
2. **Notebook Templates**: Pre-configured notebook structures
3. **Bulk Operations**: Move multiple notes between notebooks
4. **Notebook Metadata**: Custom notebook properties and descriptions
5. **Import/Export**: Notebook-specific export formats

### API Extensions
- REST API endpoints for notebook management
- Webhook support for notebook events
- External tool integration (Obsidian, etc.)

## Troubleshooting

### Common Issues

1. **Notebook Not Showing**
   - Check for placeholder note (`.notebook_<name>`)
   - Verify notebook field in note YAML

2. **Deletion Fails**
   - Use `--force` flag for notebooks with real notes
   - Check file permissions

3. **Performance Issues**
   - Consider notebook size (>1000 notes per notebook)
   - Check filesystem performance

### Debugging

```bash
# Debug notebook structure
ls -la ~/.local/share/nx/notes/ | grep notebook

# Verify note notebook assignment
nx grep "notebook:" --regex

# Check notebook statistics
nx notebook list --stats --json | jq .
```

### Log Files
- TUI events: `~/.cache/nx/tui.log`
- CLI operations: `~/.cache/nx/cli.log`
- Error details in JSON format for programmatic debugging