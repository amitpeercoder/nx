# MVP3: Enhanced TUI Editor Development Plan

## Executive Summary

MVP3 focuses on enhancing the existing TUI (Terminal User Interface) editor in nx with practical editing capabilities while maintaining the Unix philosophy of using external editors for complex tasks. The goal is to provide users with convenient in-TUI editing for quick modifications without leaving the interface.

**Key Goals:**
- Enhance existing basic editor with professional editing features
- Maintain <100ms response time for all operations
- Ensure broad terminal compatibility (xterm, gnome-terminal, kitty, alacritty)
- Support UTF-8 and multi-byte characters properly
- Provide graceful fallbacks for limited terminals
- Integrate with system clipboard when possible

## Current State Analysis

### Existing TUI Editor Features
The nx TUI already includes a basic editor with:
- ✅ Basic text input and character insertion
- ✅ Cursor movement with arrow keys  
- ✅ Save functionality (Ctrl+S)
- ✅ Cancel editing (Escape)
- ✅ Line-based navigation
- ✅ Simple backspace/delete
- ✅ Auto-edit mode when focusing preview pane

### Current Limitations
- ❌ No word-level navigation
- ❌ No text selection capabilities
- ❌ No copy/paste operations
- ❌ No undo/redo functionality
- ❌ Limited cursor movement options
- ❌ No search within note
- ❌ No auto-save capabilities
- ❌ Poor handling of large files (>1000 lines)
- ❌ No UTF-8/multi-byte character support
- ❌ No system clipboard integration
- ❌ Limited terminal compatibility

## Implementation Strategy

### Design Principles
1. **Preserve Unix Philosophy**: External editors remain primary; TUI editor for quick edits
2. **Performance First**: All operations must maintain <100ms response time
3. **Backward Compatible**: Existing keybindings and workflows unchanged
4. **Progressive Enhancement**: Add features incrementally without breaking existing functionality
5. **Memory Efficient**: Limit undo history and avoid memory bloat
6. **Terminal Agnostic**: Work across different terminal emulators and configurations
7. **Graceful Degradation**: Provide fallbacks when advanced features aren't available
8. **Safety First**: Auto-save and error recovery to prevent data loss
9. **Accessibility**: Support screen readers and keyboard-only navigation

### Technical Architecture

#### Prerequisites and Compatibility

**Terminal Compatibility Matrix:**
- ✅ xterm, gnome-terminal, konsole (full feature support)
- ✅ tmux, screen (with escape sequence translation)
- ✅ kitty, alacritty (modern terminal features)
- ⚠️ Basic terminals (limited key support, graceful degradation)
- ❌ Very old terminals (VT100) - external editor fallback only

**Text Handling:**
- UTF-8 support with proper character width calculation
- Multi-byte character cursor positioning
- Right-to-left text basic support
- Emoji and CJK character handling

#### Enhanced Data Structures
```cpp
// Enhanced editor state (to be added to AppState)
struct EnhancedEditorState {
  // Existing fields
  std::vector<std::string> lines;  // Changed: split content by lines for efficiency
  int cursor_line;
  int cursor_col;  // Character position, not byte position
  int cursor_byte_col;  // Byte position for UTF-8 handling
  int scroll_offset;
  bool has_changes;
  
  // Undo/Redo with diff-based approach
  struct EditOperation {
    enum Type { Insert, Delete, Replace } type;
    int line, col;
    std::string text;
    std::chrono::steady_clock::time_point timestamp;
  };
  std::vector<EditOperation> undo_history;
  std::vector<EditOperation> redo_history;
  int undo_index = -1;
  static constexpr size_t MAX_UNDO_HISTORY = 50;
  
  // Selection state
  struct Selection {
    int start_line = -1;
    int start_col = -1;
    int end_line = -1;
    int end_col = -1;
    bool active = false;
    bool word_mode = false;  // For double-click word selection
    bool line_mode = false;  // For triple-click line selection
  } selection;
  
  // Clipboard with system integration
  struct ClipboardState {
    std::string internal_content;
    bool has_system_clipboard = false;
    std::chrono::steady_clock::time_point last_updated;
  } clipboard;
  
  // Auto-save with configurable delay
  std::chrono::steady_clock::time_point last_edit_time;
  std::chrono::steady_clock::time_point last_save_time;
  bool auto_save_enabled = true;
  std::chrono::seconds auto_save_delay{5};
  bool auto_save_pending = false;
  
  // Find functionality with caching
  struct FindState {
    std::string query;
    std::string last_query;  // For caching
    bool active = false;
    bool case_sensitive = false;
    bool whole_words = false;
    std::vector<std::pair<int, int>> matches;  // line, col pairs
    std::vector<std::pair<int, int>> cached_matches;
    int current_match = -1;
    bool search_from_cursor = true;
  } find_state;
  
  // Configuration
  struct EditorConfig {
    bool word_wrap = false;
    int tab_size = 4;
    bool show_line_numbers = false;
    bool highlight_current_line = true;
    int max_line_length = 120;  // For performance warnings
  } config;
  
  // Terminal capabilities
  struct TerminalCaps {
    bool supports_true_color = false;
    bool supports_mouse = false;
    bool supports_bracketed_paste = false;
    std::set<std::string> supported_key_combos;
  } terminal_caps;
  
  // Error state
  std::string last_error;
  std::chrono::steady_clock::time_point error_time;
};
```

#### Keybinding Conflict Resolution

**Conflicting Keybindings:**
- Ctrl+D: Changed from "duplicate line" to "delete character forward" (standard)
- Ctrl+W: Changed from "delete word" to "close editor" (with confirmation)
- Ctrl+K: "Delete to end of line" (keeping standard meaning)
- Ctrl+L: Avoided conflict by using "Ctrl+Shift+L" for links

**Alternative Keybindings:**
- Line duplication: Ctrl+Shift+D
- Word deletion: Alt+D (forward), Alt+Backspace (backward)
- Quick actions available via command palette (Ctrl+P)

## Phase Implementation Plan

### Phase 1: Foundation and Safety (Week 1)

#### 1.0 Terminal Capability Detection
**Objective**: Detect terminal capabilities and set up compatibility layer
**Files Modified**: `src/tui/terminal_detect.cpp` (new), `include/nx/tui/terminal_caps.hpp` (new)

**Features**:
- **Terminal detection**: Identify terminal type and capabilities
- **Key combination testing**: Determine which key combos are supported
- **Escape sequence handling**: Normalize escape sequences across terminals
- **Graceful degradation**: Provide fallbacks for limited terminals

**Implementation**:
```cpp
class TerminalCapabilityDetector {
  static TerminalCaps detectCapabilities();
  static bool testKeyCombo(const std::string& combo);
  static void setupEscapeSequences();
};
```

#### 1.1 Enhanced Cursor Movement
**Objective**: Provide word-level and line-level navigation
**Files Modified**: `src/tui/tui_app.cpp`, `include/nx/tui/tui_app.hpp`

**Features**:
- **Ctrl+Left/Right**: Move cursor by word boundaries
- **Home/End**: Jump to line start/end
- **Ctrl+Home/End**: Jump to document start/end
- **Page Up/Down**: Navigate by screen height

**Implementation**:
```cpp
void TUIApp::handleWordNavigation(const ftxui::Event& event);
void TUIApp::moveToWordBoundary(bool forward);
int TUIApp::findWordBoundary(const std::string& line, int pos, bool forward);
```

#### 1.2 Text Selection
**Objective**: Enable text selection for copy/cut operations
**Features**:
- **Shift+Arrow Keys**: Select text while moving cursor
- **Shift+Ctrl+Arrow**: Select by word
- **Shift+Home/End**: Select to line boundaries
- Visual selection highlighting in editor

**Implementation**:
```cpp
void TUIApp::updateSelection(int new_line, int new_col);
void TUIApp::clearSelection();
std::string TUIApp::getSelectedText();
```

#### 1.3 Clipboard Operations
**Objective**: Cut, copy, and paste functionality
**Features**:
- **Ctrl+C**: Copy selected text to internal clipboard
- **Ctrl+X**: Cut selected text
- **Ctrl+V**: Paste from clipboard
- **Ctrl+A**: Select all text

**Implementation**:
```cpp
void TUIApp::copySelection();
void TUIApp::cutSelection();
void TUIApp::pasteFromClipboard();
void TUIApp::selectAll();
```

#### 1.4 Auto-save and Safety
**Objective**: Prevent data loss with automatic saving and error recovery
**Features**:
- **5-second auto-save** after last edit
- **Visual indicator** for unsaved changes
- **Background save** without interrupting editing
- **Error recovery**: Handle save failures gracefully
- **Backup creation**: Keep previous version on save

**Implementation**:
```cpp
void TUIApp::scheduleAutoSave();
void TUIApp::performAutoSave();
bool TUIApp::hasUnsavedChanges() const;
void TUIApp::handleSaveError(const std::string& error);
void TUIApp::createBackup();
```

#### 1.5 Basic Undo/Redo
**Objective**: Diff-based undo/redo functionality with operation batching
**Features**:
- **Ctrl+Z**: Undo last operation
- **Ctrl+Y**: Redo operation
- **Maximum 50 operations** in history
- **Operation batching** for continuous typing
- **Diff-based storage** for memory efficiency

**Implementation**:
```cpp
void TUIApp::saveEditOperation(EditOperation::Type type, int line, int col, const std::string& text);
void TUIApp::performUndo();
void TUIApp::performRedo();
void TUIApp::clearUndoHistory();
void TUIApp::batchOperations(std::chrono::milliseconds timeout);
```

### Phase 2: Navigation and Search (Week 2)

#### 2.1 Find in Note
**Objective**: Search within current note content
**Features**:
- **Ctrl+F**: Open find dialog
- **Enter/F3**: Find next occurrence
- **Shift+F3**: Find previous occurrence
- **Escape**: Close find dialog
- **Highlight all matches** in editor

**Implementation**:
```cpp
void TUIApp::openFindDialog();
void TUIApp::performFind(const std::string& query);
void TUIApp::findNext();
void TUIApp::findPrevious();
Element TUIApp::renderFindDialog() const;
```

#### 2.2 Go to Line
**Objective**: Quick navigation to specific line numbers
**Features**:
- **Ctrl+G**: Open go-to-line dialog
- **Line number input** with validation
- **Center line** in viewport after jump

**Implementation**:
```cpp
void TUIApp::openGoToLineDialog();
void TUIApp::goToLine(int line_number);
Element TUIApp::renderGoToLineDialog() const;
```

#### 2.3 Enhanced Scrolling
**Objective**: Improved viewport management
**Features**:
- **Smart centering**: Keep cursor in view
- **Smooth scrolling**: For large jumps
- **Scroll indicators**: Show position in document

**Implementation**:
```cpp
void TUIApp::ensureCursorVisible();
void TUIApp::centerCursorInView();
void TUIApp::updateScrollPosition();
```

### Phase 3: Quality of Life Improvements (Week 3)

#### 3.1 Large File Handling
**Objective**: Efficiently handle large documents
**Features**:
- **Line-based storage** for memory efficiency
- **Virtual scrolling** for very large files
- **Performance warnings** for extremely large files
- **Lazy loading** of file content

**Implementation**:
```cpp
class LargeFileHandler {
  void loadFileInChunks(const std::filesystem::path& path);
  void handleVirtualScrolling();
  void checkPerformanceThresholds();
};
```

#### 3.2 Enhanced Line Operations
**Objective**: Common line manipulation operations
**Features**:
- **Ctrl+Shift+D**: Duplicate current line
- **Ctrl+K**: Delete to end of line (standard behavior)
- **Alt+D**: Delete current word forward
- **Alt+Backspace**: Delete word backward

**Implementation**:
```cpp
void TUIApp::duplicateLine();
void TUIApp::deleteLine();
void TUIApp::deleteWord(bool forward);
```

#### 3.3 Word Wrapping
**Objective**: Better handling of long lines
**Features**:
- **Smart word wrap** at terminal width
- **Maintain indentation** on wrapped lines
- **Visual wrap indicators**

**Implementation**:
```cpp
std::vector<std::string> TUIApp::wrapText(const std::string& text, int width);
void TUIApp::updateWordWrap();
bool TUIApp::shouldWrapLine(const std::string& line, int width);
```

#### 3.4 UTF-8 and Multi-byte Support
**Objective**: Proper handling of international characters
**Features**:
- **Character width calculation**: Handle double-width CJK characters
- **Cursor positioning**: Accurate positioning with multi-byte chars
- **Text boundaries**: Proper word boundaries for international text
- **Input handling**: Correct input method editor (IME) support

**Implementation**:
```cpp
class UTF8Handler {
  static int calculateDisplayWidth(const std::string& text);
  static int getCharacterAt(const std::string& text, int display_pos);
  static std::pair<int, int> getBytePosition(const std::string& text, int char_pos);
  static bool isWordBoundary(const std::string& text, int pos);
};
```

#### 3.5 System Clipboard Integration
**Objective**: Integrate with system clipboard when available
**Features**:
- **System clipboard detection**: Check for pbcopy/pbpaste, xclip, wl-clipboard
- **Fallback to internal**: Use internal clipboard when system unavailable
- **Clipboard monitoring**: Detect external clipboard changes
- **Secure handling**: Avoid clipboard leakage in encrypted mode

**Implementation**:
```cpp
class SystemClipboard {
  static bool isAvailable();
  static std::optional<std::string> getSystemClipboard();
  static bool setSystemClipboard(const std::string& content);
  static ClipboardProvider detectProvider();
};
```

#### 3.6 Status Enhancements
**Objective**: Better feedback about editor state
**Features**:
- **Line/column display** in status bar
- **Document statistics** (lines, words, characters)
- **Modification indicators**
- **Operation feedback**

### Phase 4: Markdown Features (Week 4) - Final Phase

#### 4.1 Syntax Shortcuts
**Objective**: Quick markdown formatting
**Features**:
- **Ctrl+B**: Wrap selection in bold markers (`**text**`)
- **Ctrl+I**: Wrap selection in italic markers (`*text*`)
- **Ctrl+L**: Create link template (`[text](url)`)
- **Alt+1-6**: Convert line to header level

#### 4.2 List Handling
**Objective**: Smart list operations
**Features**:
- **Auto-continue lists** on Enter
- **Tab/Shift+Tab**: Indent/outdent list items
- **Smart bullet detection** (-, *, +, numbers)

#### 4.3 Basic Syntax Highlighting
**Objective**: Visual markdown indicators
**Features**:
- **Header highlighting**: Different colors for H1-H6
- **Bold/italic styling**: Visual emphasis
- **Link coloring**: Different color for `[text](url)`
- **Code block highlighting**: Monospace for ``` blocks

#### 4.4 Auto-completion
**Objective**: Context-aware completion
**Features**:
- **[[wiki-links]]**: Complete from existing note titles
- **Tag completion**: Complete with '#' trigger
- **Link suggestions**: Based on note content

## File Modifications Required

### Core Files
```
include/nx/tui/tui_app.hpp
├── Enhanced AppState structure
├── New editor method declarations
└── Find/GoTo dialog state

src/tui/tui_app.cpp
├── Enhanced handleEditModeInput()
├── New editor command implementations
├── Find/GoTo dialog rendering
└── Auto-save logic

src/tui/editor_operations.cpp (new)
├── Word navigation functions
├── Selection management
├── Clipboard operations
└── Undo/redo implementation
```

### Test Files
```
tests/unit/tui/editor_test.cpp (new)
├── Cursor movement tests
├── Selection tests
├── Clipboard tests
├── Undo/redo tests
├── UTF-8 handling tests
└── Terminal compatibility tests

tests/unit/tui/utf8_handler_test.cpp (new)
├── Character width calculation tests
├── Multi-byte cursor positioning
├── Word boundary detection
└── Edge case handling

tests/unit/tui/terminal_caps_test.cpp (new)
├── Terminal detection tests
├── Key combination tests
├── Escape sequence handling
└── Fallback mechanism tests

tests/integration/tui_editor_integration_test.cpp (new)
├── End-to-end editing scenarios
├── Performance benchmarks
├── Memory usage tests
├── Large file handling tests
├── Terminal compatibility tests
└── Error recovery tests

tests/integration/clipboard_integration_test.cpp (new)
├── System clipboard integration
├── Multiple clipboard provider tests
├── Fallback mechanism tests
└── Security isolation tests
```

## Testing Strategy

### Unit Tests
- **Cursor movement**: Test all navigation combinations including UTF-8
- **Text operations**: Copy, cut, paste, undo/redo with multi-byte chars
- **Find functionality**: Search algorithms, regex, and edge cases
- **Auto-save**: Timer and trigger conditions, error handling
- **UTF-8 handling**: Character width, positioning, word boundaries
- **Terminal capabilities**: Detection, key combos, escape sequences
- **Clipboard**: Internal, system integration, fallbacks

### Integration Tests
- **TUI interaction**: Full editing workflows across terminals
- **Performance**: Large document handling (1MB+ files)
- **Memory**: Undo history and selection management
- **Compatibility**: Existing keybindings preserved
- **Terminal matrix**: Test across xterm, gnome-terminal, kitty, alacritty
- **SSH/tmux**: Remote terminal compatibility
- **Error recovery**: File save failures, corrupted states

### Edge Case Testing
- **Empty documents**: Single char, empty lines, whitespace-only
- **Large files**: 10k+ lines, very long single lines (>1000 chars)
- **Special characters**: Emoji, CJK, RTL text, control characters
- **Terminal resize**: During editing, mid-operation
- **Concurrent access**: Multiple instances, file changes
- **Resource limits**: Memory exhaustion, disk full

### Performance Benchmarks
- **Response time**: <100ms for all operations, <50ms for cursor movement
- **Memory usage**: <50MB additional for editor state
- **Large documents**: Test with 10k+ line files, 1MB+ files
- **Startup time**: Editor mode activation <200ms
- **Search performance**: <500ms for 100k+ character documents

## Success Metrics

### Functional Metrics
- ✅ All Phase 1-3 features implemented and tested
- ✅ Backward compatibility maintained
- ✅ Performance targets met (<100ms response)
- ✅ Memory usage within limits (<50MB additional)

### User Experience Metrics
- ✅ Seamless transition between view and edit modes
- ✅ Intuitive keybindings following standard conventions
- ✅ Clear visual feedback for all operations
- ✅ Helpful error messages and status updates

### Quality Metrics
- ✅ 90%+ test coverage for new editor functions
- ✅ Zero regressions in existing functionality
- ✅ Clean code following nx style guidelines
- ✅ Complete documentation for new features

## Error Handling and Recovery

### File Operation Errors
- **Save failures**: Retry with exponential backoff, create backup
- **Permission denied**: Clear error message, suggest solutions
- **Disk full**: Warn user, attempt to save to temp location
- **File corruption**: Validate on load, offer recovery options

### Memory and Resource Errors
- **Out of memory**: Graceful degradation, reduce undo history
- **Very large files**: Warn user, offer external editor option
- **Terminal resize**: Adjust viewport, maintain cursor position
- **Process interruption**: Auto-save recovery on restart

### Terminal Compatibility Errors
- **Unsupported keys**: Show alternative keybindings
- **Display issues**: Fall back to basic rendering
- **Escape sequence problems**: Use compatibility mode
- **Color support**: Graceful degradation to monochrome

### User Input Errors
- **Invalid characters**: Filter or convert invalid input
- **Clipboard failures**: Fall back to internal clipboard
- **Search regex errors**: Show helpful error messages
- **Undo corruption**: Reset undo history, warn user

## Configuration and User Preferences

### Configurable Settings
```toml
[editor]
auto_save = true
auto_save_delay = 5  # seconds
tab_size = 4
word_wrap = false
show_line_numbers = false
highlight_current_line = true
max_undo_history = 50

[editor.search]
case_sensitive = false
whole_words = false
highlight_all_matches = true

[editor.clipboard]
prefer_system_clipboard = true
clipboard_timeout = 1000  # ms

[editor.terminal]
detect_capabilities = true
force_basic_mode = false
key_timeout = 100  # ms for escape sequences
```

### User Preference Management
```cpp
class EditorPreferences {
  static EditorConfig loadConfig();
  static void saveConfig(const EditorConfig& config);
  static void resetToDefaults();
  static bool validateConfig(const EditorConfig& config);
};
```

## Risk Mitigation

### Technical Risks
- **Performance degradation**: Extensive profiling and optimization
- **Memory leaks**: Careful resource management and testing  
- **Compatibility issues**: Thorough testing on different terminals
- **UTF-8 bugs**: Comprehensive multi-byte character testing
- **Terminal detection failures**: Always provide basic fallback

### User Experience Risks
- **Feature creep**: Strict adherence to "light editing" scope
- **Keybinding conflicts**: Careful mapping and user testing
- **Learning curve**: Comprehensive help system and documentation
- **Data loss**: Multiple auto-save and backup mechanisms
- **Performance on slow terminals**: Optimize for minimal escape sequences

### Security Risks
- **Clipboard leakage**: Secure clipboard handling in encrypted mode
- **Terminal injection**: Sanitize all escape sequences
- **File permissions**: Respect and maintain original permissions
- **Temporary files**: Secure cleanup of temporary edit files

## Implementation Timeline

```
Week 1: Phase 1 - Foundation and Safety
├── Days 1-2: Terminal capability detection and compatibility layer
├── Days 3-4: Enhanced cursor movement and UTF-8 support
├── Days 5-6: Text selection and auto-save implementation
└── Day 7: Undo/redo system with diff-based storage

Week 2: Phase 2 - Navigation and Search
├── Days 1-3: Find functionality with caching and highlighting
├── Days 4-5: Go-to-line, enhanced scrolling, and viewport management
└── Days 6-7: System clipboard integration and testing

Week 3: Phase 3 - Quality of Life and Performance
├── Days 1-2: Large file handling and virtual scrolling
├── Days 3-4: Enhanced line operations and word wrapping
├── Days 5-6: Status enhancements and error handling
└── Day 7: Performance optimization and memory management

Week 4: Phase 4 - Markdown Features and Polish
├── Days 1-2: Syntax shortcuts and formatting helpers
├── Days 3-4: List handling, auto-completion, and basic highlighting
├── Days 5-6: Configuration system and user preferences
└── Day 7: Final testing, documentation, and accessibility review

Week 5: Testing and Hardening (Optional)
├── Days 1-3: Comprehensive terminal compatibility testing
├── Days 4-5: Edge case testing and stress testing
├── Days 6-7: Performance profiling and optimization
```

## Future Considerations

### Post-MVP3 Enhancements
- **Collaborative editing**: Real-time multi-user support
- **Plugin system**: User-extensible functionality
- **Custom themes**: User-configurable color schemes
- **Split panes**: Multiple document editing

### Integration Opportunities
- **External editor sync**: Two-way synchronization
- **Git integration**: Inline diff and merge conflict resolution
- **AI assistance**: In-editor AI suggestions and corrections

## Documentation Requirements

### User Documentation
- **Update README.md**: Add TUI editor section with feature overview
- **Create editor guide**: Comprehensive guide for TUI editor features
- **Keybinding reference**: Quick reference card for all editor shortcuts
- **Video tutorials**: Screen recordings of key editing workflows
- **Migration guide**: Help users transition from external-editor-only workflow

### Developer Documentation
- **API documentation**: Complete documentation for all new editor functions
- **Architecture decisions**: ADRs for major design choices (UTF-8 handling, undo system)
- **Testing guide**: How to run editor tests, add new test cases
- **Contributing guide**: Guidelines for editor feature contributions
- **Performance guide**: Optimization techniques and profiling instructions

### Help System Updates
- **Context-sensitive help**: Show relevant shortcuts based on current mode
- **Error message improvements**: Clear, actionable error messages
- **Accessibility documentation**: Screen reader compatibility and keyboard navigation
- **Terminal compatibility guide**: Recommended terminals and settings

---

**Document Version**: 2.0  
**Created**: 2025-08-15  
**Updated**: 2025-08-15  
**Status**: Enhanced Planning Phase  
**Next Review**: After terminal compatibility testing