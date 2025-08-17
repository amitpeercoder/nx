# nx Interactive TUI Design Specification

## Overview

This document specifies the design and implementation of an interactive Terminal User Interface (TUI) for the nx notes application. The TUI addresses the core UX problem of requiring users to interact with complex ULIDs by providing a modern, visual note-taking interface similar to Obsidian, Notion, or Bear.

## Problem Statement

### Current Issues
- **Poor UX**: Users must memorize or copy complex ULIDs like `01J8Y4N9W8K6W3K4T4S0S3QF4N`
- **Cognitive overhead**: No visual browsing of note collections
- **Discoverability**: Users don't know what notes exist without listing them
- **Context switching**: Must exit editor to find related notes
- **CLI barriers**: Not intuitive for non-technical users

### User Stories
- **As a researcher**, I want to visually browse my notes to find related content quickly
- **As a writer**, I want to see all my draft notes in one view with previews
- **As a student**, I want to filter notes by subject/tag without remembering commands
- **As a knowledge worker**, I want familiar note-taking app interactions in the terminal

## Solution: Interactive TUI Mode

### Launch Behavior
```bash
nx           # Opens TUI if notes exist, shows getting started if empty
nx ui        # Explicit TUI mode
nx [cmd]     # Traditional CLI (backward compatible)
```

### Core Principles
1. **Human-readable display**: Show titles, dates, tags - hide ULIDs
2. **Visual browsing**: See note collections at a glance
3. **Progressive disclosure**: Simple operations visible, advanced via command palette
4. **Keyboard-first**: Efficient navigation without mouse
5. **Familiar patterns**: Borrow from popular note-taking apps

## Architecture

### Component Hierarchy
```
Application
├── MainView (3-pane layout)
│   ├── TagsPanel (left)
│   ├── NotesPanel (center)
│   │   ├── NoteList
│   │   ├── SearchBox
│   │   └── StatusBar
│   └── PreviewPane (right)
├── Modals
│   ├── NewNoteModal
│   ├── CommandPalette
│   ├── SettingsModal
│   └── HelpModal
└── StatusLine (bottom)
```

### State Management

#### Application State
```cpp
struct AppState {
    // View state
    ActivePane current_pane = ActivePane::Notes;
    ViewMode mode = ViewMode::ThreePane;
    
    // Data state
    std::vector<NoteMetadata> notes;
    std::vector<std::string> tags;
    NoteId selected_note_id;
    std::set<NoteId> selected_notes;
    
    // Filter state
    std::string search_query;
    std::set<std::string> active_tag_filters;
    SortMode sort_mode = SortMode::Modified;
    
    // UI state
    bool show_help = false;
    bool command_palette_open = false;
    std::string status_message;
};
```

#### State Updates
- **Reactive pattern**: State changes trigger re-render
- **Command pattern**: All operations as undoable commands
- **Optimistic updates**: Fast UI response, async persistence
- **Event sourcing**: Track user actions for undo/redo

### Component Specifications

#### 1. TagsPanel (Left Pane)
```
┌─ Tags ──────────────┐
│ 📂 work (15)        │
│ 📂 personal (8)     │
│ 📂 ideas (23)       │
│ 📂 archive (5)      │
│ ────────────────────│
│ 🔍 [Filter tags]    │
│                     │
│ Active filters:     │
│ × work × ideas      │
└─────────────────────┘
```

**Features:**
- Hierarchical tag display (notebook → tags)
- Tag counts in parentheses
- Quick filter toggle (click/enter to filter)
- Active filter indicators with removal
- Search box for tag filtering
- Collapsible tag groups

**State:**
- `std::map<std::string, int> tag_counts`
- `std::set<std::string> active_filters`
- `std::string tag_search_query`

#### 2. NotesPanel (Center Pane)
```
┌─ Notes ─────────────────────────────┐
│ 🔍 [Search notes...]               │
│ ────────────────────────────────────│
│ ▶ Design meeting notes              │
│   2024-01-14 10:30 📝 work,planning │
│                                     │
│   Project roadmap Q1                │
│   2024-01-13 15:22 📝 work,strategy │
│                                     │
│   Weekend hiking ideas              │
│   2024-01-12 09:15 📝 personal,fun  │
│ ────────────────────────────────────│
│ 📄 234 notes | 🏷️ 12 tags | ↑ sort │
└─────────────────────────────────────┘
```

**Features:**
- Real-time search with fuzzy matching
- Note list with rich metadata display
- Visual selection indicators
- Sort controls (date, title, relevance)
- Pagination for large collections
- Multi-select support (space key)

**Metadata Display:**
- **Primary**: Note title (truncated if needed)
- **Secondary**: Last modified date/time
- **Tertiary**: Tag pills with colors
- **Visual**: Icons for note type/status

#### 3. PreviewPane (Right Pane)
```
┌─ Preview ───────────────────────────┐
│ # Design Meeting Notes              │
│ *Modified: 2024-01-14 10:30*        │
│                                     │
│ ## Agenda                           │
│ - [ ] Architecture discussion       │
│ - [x] Timeline review               │
│ - [ ] Resource allocation           │
│                                     │
│ ## Notes                            │
│ The team discussed the new feature  │
│ implementation approach...          │
│                                     │
│ [[Project roadmap Q1]] - Related    │
│                                     │
│ Tags: #work #planning               │
│ Links: 2 backlinks, 3 outlinks     │
└─────────────────────────────────────┘
```

**Features:**
- Markdown rendering with syntax highlighting
- Clickable links to other notes
- Metadata footer (tags, links, dates)
- Scroll support for long notes
- Link preview on hover
- Quick edit button

### Keyboard Navigation

#### Global Shortcuts
```
Navigation:
  h/←     Focus left pane (tags)
  j/↓     Move down in current pane  
  k/↑     Move up in current pane
  l/→     Focus right pane (preview)
  Tab     Cycle through panes
  Space   Multi-select toggle
  Enter   Open/activate selected item

Actions:
  n       New note
  e       Edit selected note
  d       Delete selected note(s)
  r       Rename selected note
  t       Tag selected note(s)
  /       Focus search box
  :       Open command palette
  ?       Toggle help
  q       Quit application

Operations:
  y       Copy note ID to clipboard
  p       Paste/link note reference
  s       Sync notes
  u       Undo last operation
  Ctrl+r  Redo last operation
  Ctrl+f  Find in current note
  Ctrl+g  Go to note (fuzzy finder)
  Ctrl+n  New note with template
```

#### Context-Sensitive Shortcuts
- **In search box**: `Esc` to clear/exit, `↓` to move to results
- **In tags panel**: `Enter` to toggle filter, `x` to remove filter
- **In notes list**: `Space` for multi-select, `Shift+j/k` for range select
- **In preview**: `↑/↓` to scroll, `Enter` on link to follow

### Rendering Pipeline

#### Frame Rendering (60 FPS target)
```cpp
class Renderer {
    void render_frame() {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 1. Calculate layout dimensions
        auto layout = calculate_layout(terminal_size());
        
        // 2. Render components
        render_tags_panel(layout.tags_rect);
        render_notes_panel(layout.notes_rect);
        render_preview_panel(layout.preview_rect);
        render_status_line(layout.status_rect);
        
        // 3. Handle overlays
        if (state.command_palette_open) {
            render_command_palette();
        }
        
        // 4. Flush to terminal
        terminal.flush();
        
        // 5. Performance tracking
        auto duration = std::chrono::high_resolution_clock::now() - start;
        fps_tracker.record_frame(duration);
    }
};
```

#### Responsive Layout
```
Terminal width < 80:  Single pane (notes only)
Terminal width 80-120: Two pane (notes + preview)  
Terminal width > 120:  Three pane (tags + notes + preview)

Terminal height < 24: Compact mode (reduced margins)
Terminal height > 40: Comfortable mode (more spacing)
```

### Performance Optimizations

#### Data Loading
- **Lazy loading**: Load note content only when previewing
- **Metadata caching**: Cache note metadata in SQLite for fast listing
- **Incremental updates**: Only re-read changed files
- **Background indexing**: Update search index asynchronously

#### Rendering Optimizations
- **Dirty region tracking**: Only re-render changed areas
- **Text caching**: Cache rendered text layouts
- **Viewport culling**: Only render visible list items
- **Debounced search**: Wait for typing pause before searching

#### Memory Management
```cpp
class NoteCache {
    // LRU cache for note content
    std::unordered_map<NoteId, std::shared_ptr<Note>> content_cache;
    std::list<NoteId> lru_order;
    size_t max_cache_size = 100;
    
    // Metadata always in memory
    std::vector<NoteMetadata> all_notes_metadata;
    
    void evict_lru() {
        while (content_cache.size() > max_cache_size) {
            auto oldest = lru_order.back();
            content_cache.erase(oldest);
            lru_order.pop_back();
        }
    }
};
```

### Search & Filtering

#### Real-time Search
```cpp
class SearchEngine {
    // Hybrid search combining FTS and fuzzy matching
    std::vector<NoteId> search(const std::string& query) {
        auto fts_results = sqlite_fts_search(query);
        auto fuzzy_results = fuzzy_title_search(query);
        
        return merge_and_rank_results(fts_results, fuzzy_results);
    }
    
    // Progressive search as user types
    void on_search_input(const std::string& partial_query) {
        if (partial_query.length() >= 2) {
            auto results = search(partial_query);
            ui_thread.post([=] { update_search_results(results); });
        }
    }
};
```

#### Filter Combinations
- **Tag filters**: AND/OR logic (configurable)
- **Date filters**: Before/after, date ranges
- **Content filters**: Search within results
- **Notebook filters**: Specific notebooks only
- **Type filters**: Regular notes vs templates

### Modal Dialogs

#### Command Palette
```
┌─ Command Palette ───────────────────┐
│ > git sync                          │
│ ────────────────────────────────────│
│ 🔄 Git: Sync notes                  │
│ 📝 Edit: Rename note                │
│ 🏷️  Tag: Add tag to selection       │
│ 📁 File: Export selection           │
│ ⚙️  Config: Open settings           │
│ 🔧 System: Rebuild index            │
└─────────────────────────────────────┘
```

**Features:**
- Fuzzy command search
- Recent commands history
- Context-aware suggestions
- Keyboard shortcuts display
- Command preview/help text

#### New Note Modal
```
┌─ New Note ──────────────────────────┐
│ Title: [Meeting with design team]   │
│ ────────────────────────────────────│
│ Notebook: [ work       ▼ ]          │
│ Tags:     [ planning, team     ]     │
│ Template: [ meeting-notes   ▼ ]     │
│ ────────────────────────────────────│
│          [Create]  [Cancel]         │
└─────────────────────────────────────┘
```

### Configuration Options

#### TUI Settings (config.toml)
```toml
[ui]
# Layout
default_layout = "three_pane"  # single_pane, two_pane, three_pane
show_line_numbers = true
wrap_text = true
preview_width_ratio = 0.4

# Appearance  
theme = "default"  # default, dark, light, custom
show_icons = true
highlight_search = true
dim_unselected = false

# Behavior
auto_preview = true
follow_links_in_preview = true
vim_keys = true
mouse_support = false

# Performance
max_preview_size_kb = 500
cache_preview_count = 50
lazy_load_threshold = 1000

[ui.shortcuts]
# Allow customizing key bindings
new_note = "n"
edit_note = "e"
delete_note = "d"
search = "/"
command_palette = ":"
```

### Error Handling & Edge Cases

#### Graceful Degradation
- **No notes**: Show welcome screen with getting started guide
- **Large collections**: Pagination and lazy loading
- **Terminal too small**: Switch to minimal single-pane mode
- **Note corruption**: Show error in preview, allow recovery
- **Index corruption**: Fall back to filesystem scanning

#### Error States
- **Search errors**: Show "No results found" with suggestions
- **File access errors**: Show permissions error with fix instructions
- **Git errors**: Show sync status with resolution options
- **Template errors**: Use fallback template or plain note

### Implementation Phases

#### Phase 1: Core Framework (Week 1)
- [ ] Set up FTXUI or ncurses integration
- [ ] Create basic 3-pane layout
- [ ] Implement keyboard event handling
- [ ] Add resize handling and responsive layout
- [ ] Create component base classes

#### Phase 2: Data Integration (Week 1-2)
- [ ] Connect to existing note storage
- [ ] Implement metadata caching
- [ ] Add real-time file watching
- [ ] Create search integration
- [ ] Build filtering system

#### Phase 3: Core Interactions (Week 2)
- [ ] Implement note selection and navigation
- [ ] Add preview pane with markdown rendering
- [ ] Create tag filtering
- [ ] Build search-as-you-type
- [ ] Add basic note operations (new, edit, delete)

#### Phase 4: Advanced Features (Week 3)
- [ ] Command palette implementation
- [ ] Multi-select operations
- [ ] Undo/redo system
- [ ] Link navigation
- [ ] Template integration

#### Phase 5: Polish & Performance (Week 3-4)
- [ ] Performance optimizations
- [ ] Error handling and edge cases
- [ ] Customizable themes and shortcuts
- [ ] Comprehensive testing
- [ ] Documentation and help system

### Testing Strategy

#### User Experience Testing
- **Usability tests**: Time-to-completion for common tasks
- **Keyboard navigation**: Ensure all features accessible via keyboard
- **Accessibility**: Screen reader compatibility where possible
- **Performance tests**: Responsive with 10k+ notes

#### Integration Testing
- **Data consistency**: Ensure TUI and CLI see same data
- **File watching**: Changes reflected immediately
- **Concurrent access**: Handle multiple nx instances
- **Error recovery**: Graceful handling of corrupted data

### Success Metrics

#### User Experience
- **Discovery time**: Find specific note in < 10 seconds
- **Creation time**: New note created in < 5 seconds
- **Navigation efficiency**: Switch between related notes quickly
- **Learning curve**: Basic operations obvious without documentation

#### Technical Performance
- **Startup time**: TUI opens in < 500ms
- **Search latency**: Results appear in < 200ms
- **Render performance**: Smooth scrolling at 60fps
- **Memory usage**: < 50MB for 10k notes with preview cache

### Future Enhancements

#### Advanced Features (Post-MVP)
- **Split pane editing**: Edit multiple notes simultaneously
- **Graph view**: Visual representation of note connections
- **Quick switcher**: Cmd+P style note jumping
- **Recent notes**: MRU list for quick access
- **Workspaces**: Save and restore panel configurations

#### AI Integration
- **Smart suggestions**: Suggest tags and links while typing
- **Content preview**: AI-generated summaries in search results
- **Related notes**: ML-powered note recommendations
- **Auto-organization**: AI-suggested notebook and tag structure

This TUI design transforms nx from a CLI tool into a modern, visual note-taking experience while maintaining all the power and flexibility of the underlying command-line interface.