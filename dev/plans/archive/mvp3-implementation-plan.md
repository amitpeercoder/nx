# MVP3: TUI Editor Enhancement - Comprehensive Implementation Plan

## Executive Summary

This document provides a detailed implementation plan for MVP3, which focuses on enhancing the existing TUI (Terminal User Interface) editor in nx with professional editing capabilities while maintaining the Unix philosophy and performance requirements.

**⚠️ CRITICAL ANALYSIS UPDATE**: After thorough codebase analysis, significant gaps and architectural issues have been identified in the original plan. This document now reflects a comprehensive approach that properly integrates with existing nx infrastructure.

### Key Objectives
- Enhance existing basic editor with professional editing features
- Maintain <100ms response time for all operations (upgraded target: <50ms)
- Ensure broad terminal compatibility using FTXUI's existing capabilities
- Support UTF-8 and multi-byte characters using proven libraries (ICU)
- Integrate properly with existing security patterns (`nx::util::Security`, `SensitiveString`)
- Use existing error handling patterns (`std::expected<T, Error>`)
- Maintain compatibility with current FTXUI component architecture

## Critical Analysis and Current State

### Existing TUI Editor Features (src/tui/tui_app.cpp:2657-2800+)
The nx TUI already includes a basic editor with:
- ✅ Basic text input and character insertion (ASCII only)
- ✅ Cursor movement with arrow keys  
- ✅ Save functionality (Ctrl+S)
- ✅ Cancel editing (Escape)
- ✅ Line-based navigation with scroll management
- ✅ Simple backspace/delete with line merging
- ✅ Auto-edit mode when focusing preview pane
- ✅ Line splitting on Enter with proper cursor positioning

### Critical Architectural Issues Identified
- ⚠️ **Performance**: Content rebuilding on every edit (`rebuildEditContent()`) - O(n) complexity
- ⚠️ **Memory**: No bounds checking for large files, potential memory exhaustion
- ⚠️ **Security**: No input validation, vulnerable to terminal injection
- ⚠️ **Architecture**: Violates existing patterns, doesn't use `std::expected<T, Error>`
- ⚠️ **Unicode**: ASCII-only support (c >= 32 && c <= 126), broken for international text
- ⚠️ **FTXUI Integration**: Doesn't leverage FTXUI's advanced capabilities

### Security Infrastructure Available
- ✅ `nx::util::Security` class with input sanitization
- ✅ `SensitiveString` RAII wrapper for secure memory handling
- ✅ Existing error handling with `std::expected<T, Error>` pattern
- ✅ Secure file operations and temporary file management

### Current Limitations Requiring Immediate Attention
- ❌ **CRITICAL**: No bounds checking on buffer operations
- ❌ **CRITICAL**: Inefficient O(n) edit operations will fail on large files
- ❌ **SECURITY**: Raw character input without validation
- ❌ No word-level navigation or text selection
- ❌ No clipboard integration with security considerations
- ❌ No undo/redo functionality
- ❌ No UTF-8/multi-byte character support
- ❌ No auto-save with dirty bit tracking
- ❌ No search within note functionality

## Technical Architecture

### **CRITICAL**: Architecture Overhaul Required

The original plan assumed non-existent infrastructure. Based on codebase analysis, we need:

1. **Integration with Existing Patterns**:
   - Use `std::expected<T, nx::Error>` for all operations
   - Integrate with `nx::util::Security` for safe operations
   - Follow FTXUI component architecture
   - Respect existing memory management patterns

2. **Performance-First Design**:
   - Replace string rebuilding with efficient buffer structure
   - Implement lazy rendering for large documents
   - Add memory pressure detection and mitigation

### Enhanced Data Structures

#### Core Enhancement to AppState (include/nx/tui/tui_app.hpp:151-157)
Current basic editor state will be **completely refactored** with:

```cpp
// REFACTORED: Performance-optimized editor state
struct EnhancedEditorState {
  // HIGH-PERFORMANCE BUFFER: Replace inefficient string rebuilding
  class EditorBuffer {
    // Gap buffer or rope structure for O(1) local edits
    std::vector<char> buffer_;
    size_t gap_start_ = 0;
    size_t gap_end_ = 0;
    std::vector<size_t> line_offsets_;  // For O(1) line access
  public:
    std::expected<void, nx::Error> insertAt(size_t pos, std::string_view text);
    std::expected<void, nx::Error> deleteRange(size_t start, size_t end);
    std::expected<std::string_view, nx::Error> getLine(size_t line_num) const;
    size_t getLineCount() const { return line_offsets_.size(); }
    std::expected<void, nx::Error> validatePosition(size_t line, size_t col) const;
  };
  
  EditorBuffer buffer;  // Replaces inefficient string operations
  
  // CURSOR WITH BOUNDS CHECKING
  struct CursorPosition {
    size_t line = 0;
    size_t col = 0;  // Unicode code point position
    size_t byte_offset = 0;  // UTF-8 byte position
    
    std::expected<void, nx::Error> validate(const EditorBuffer& buffer) const;
  } cursor;
  
  size_t scroll_offset = 0;
  bool has_changes = false;
  std::chrono::steady_clock::time_point last_edit_time;
  
  // SECURE UNDO/REDO: Command pattern with proper memory management
  class EditorCommand {
  public:
    virtual ~EditorCommand() = default;
    virtual std::expected<void, nx::Error> execute(EditorBuffer& buffer) = 0;
    virtual std::expected<void, nx::Error> undo(EditorBuffer& buffer) = 0;
    virtual bool canMergeWith(const EditorCommand& other) const = 0;
    virtual std::chrono::steady_clock::time_point getTimestamp() const = 0;
  };
  
  // Circular buffer for memory efficiency
  std::array<std::unique_ptr<EditorCommand>, 100> undo_history;
  size_t undo_head = 0;
  size_t undo_tail = 0;
  size_t undo_current = 0;
  static constexpr size_t MAX_UNDO_HISTORY = 100;
  
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
  
  // SECURE CLIPBOARD: Integration with nx::util::Security
  struct SecureClipboardState {
    nx::util::SensitiveString internal_content;  // RAII secure memory
    bool has_system_clipboard = false;
    std::chrono::steady_clock::time_point last_updated;
    
    std::expected<void, nx::Error> setContent(std::string_view content);
    std::expected<std::string, nx::Error> getContent() const;
    std::expected<void, nx::Error> syncWithSystem();
    void clearSecurely();
  } clipboard;
  
  // Auto-save with configurable delay
  std::chrono::steady_clock::time_point last_edit_time;
  std::chrono::steady_clock::time_point last_save_time;
  bool auto_save_enabled = true;
  std::chrono::seconds auto_save_delay{5};
  bool auto_save_pending = false;
  
  // OPTIMIZED SEARCH: Performance and security
  struct FindState {
    nx::util::SensitiveString query;  // Protect search queries
    nx::util::SensitiveString last_query;  // For caching
    bool active = false;
    bool case_sensitive = false;
    bool whole_words = false;
    bool regex_mode = false;
    
    // Memory-efficient match storage
    struct Match {
      size_t line;
      size_t start_col;
      size_t end_col;
    };
    std::vector<Match> matches;
    std::vector<Match> cached_matches;
    size_t current_match = 0;
    bool search_from_cursor = true;
    
    std::expected<void, nx::Error> performSearch(const EditorBuffer& buffer);
    std::expected<void, nx::Error> validateRegex() const;
  } find_state;
  
  // Configuration
  struct EditorConfig {
    bool word_wrap = false;
    int tab_size = 4;
    bool show_line_numbers = false;
    bool highlight_current_line = true;
    int max_line_length = 120;  // For performance warnings
  } config;
  
  // FTXUI-COMPATIBLE TERMINAL CAPABILITIES
  struct TerminalCapabilities {
    bool supports_true_color = false;
    bool supports_mouse = false;
    bool supports_bracketed_paste = false;
    bool supports_extended_keys = false;
    
    // Capability detection using FTXUI's event system
    static std::expected<TerminalCapabilities, nx::Error> detect();
    std::expected<void, nx::Error> validateKeyEvent(const ftxui::Event& event) const;
  } terminal_caps;
  
  // MEMORY PRESSURE MONITORING
  struct MemoryMonitor {
    size_t max_buffer_size = 100 * 1024 * 1024;  // 100MB limit
    size_t current_usage = 0;
    bool pressure_detected = false;
    
    std::expected<void, nx::Error> checkMemoryPressure();
    void handleMemoryPressure();  // Reduce undo history, clear caches
  } memory_monitor;
  
  // Error state
  std::string last_error;
  std::chrono::steady_clock::time_point error_time;
};
```

### Security and Performance Integration

#### Input Validation and Security
```cpp
// include/nx/tui/editor_security.hpp
class EditorInputValidator {
public:
  // Integration with nx::util::Security
  static std::expected<std::string, nx::Error> validateAndSanitize(
    const ftxui::Event& event);
  
  static std::expected<void, nx::Error> validateFileSize(size_t size);
  
  static bool isSecureClipboardOperation(const std::string& content);
  
  // Prevent terminal injection attacks
  static std::expected<std::string, nx::Error> sanitizeControlSequences(
    std::string_view input);
    
private:
  static constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;  // 100MB
  static constexpr size_t MAX_LINE_LENGTH = 10000;
  static bool containsMaliciousSequences(std::string_view input);
};
```

#### Unicode Support (ICU Integration)
```cpp
// include/nx/tui/unicode_handler.hpp
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/utext.h>

class UnicodeTextHandler {
public:
  // Production-ready Unicode handling using ICU
  static std::expected<size_t, nx::Error> calculateDisplayWidth(
    std::string_view utf8_text);
    
  static std::expected<size_t, nx::Error> findCodePointAt(
    std::string_view text, size_t byte_offset);
    
  static std::expected<bool, nx::Error> isWordBoundary(
    std::string_view text, size_t position);
    
  static std::expected<std::pair<size_t, size_t>, nx::Error> getWordBounds(
    std::string_view text, size_t position);
    
  // Validate UTF-8 and handle invalid sequences gracefully
  static std::expected<std::string, nx::Error> validateAndNormalize(
    std::string_view input);
    
  // Performance-optimized character iteration
  class CodePointIterator {
  public:
    explicit CodePointIterator(std::string_view text);
    bool hasNext() const;
    std::expected<char32_t, nx::Error> next();
    size_t currentByteOffset() const;
    size_t currentCharOffset() const;
  private:
    UText* utext_ = nullptr;
  };
  
private:
  // Use ICU's proven implementations
  static bool isDoubleWidth(char32_t codepoint);
  static bool isEmoji(char32_t codepoint);
  static bool isCJK(char32_t codepoint);
};
```

## **REVISED** Phase Implementation Plan

### **Phase 1: Critical Infrastructure (Week 1)**

#### Day 1-2: Security-First Foundation
**Objective**: Build secure, performant foundation using existing nx patterns

**Files to Create**:
- `include/nx/tui/editor_security.hpp` - Input validation & sanitization
- `include/nx/tui/editor_buffer.hpp` - High-performance gap buffer
- `src/tui/editor_security.cpp` - Security implementation
- `src/tui/editor_buffer.cpp` - Buffer implementation
- `tests/unit/tui/editor_security_test.cpp` - Security validation tests

**Implementation Details**:
```cpp
// SECURITY-FIRST: Input validation using existing nx patterns
std::expected<std::string, nx::Error> 
EditorInputValidator::validateAndSanitize(const ftxui::Event& event) {
  // Validate event type
  if (!event.is_character() && !event.is_special()) {
    return std::unexpected(nx::Error(nx::ErrorCode::kInvalidArgument, 
                                    "Invalid event type"));
  }
  
  if (event.is_character()) {
    const auto& input = event.character();
    
    // Check for malicious control sequences
    if (containsMaliciousSequences(input)) {
      return std::unexpected(nx::Error(nx::ErrorCode::kSecurityError,
                                      "Potentially malicious input detected"));
    }
    
    // Validate UTF-8 encoding
    return UnicodeTextHandler::validateAndNormalize(input);
  }
  
  return "";  // Special keys return empty string
}

// GAP BUFFER: O(1) local edits, O(n) only for distant operations
std::expected<void, nx::Error> 
EditorBuffer::insertAt(size_t pos, std::string_view text) {
  // Validate position bounds
  if (pos > size()) {
    return std::unexpected(nx::Error(nx::ErrorCode::kInvalidArgument,
                                    "Position out of bounds"));
  }
  
  // Check memory limits
  if (buffer_.size() + text.size() > MAX_BUFFER_SIZE) {
    return std::unexpected(nx::Error(nx::ErrorCode::kMemoryError,
                                    "Buffer size limit exceeded"));
  }
  
  moveGapTo(pos);
  
  // Insert into gap
  if (gap_end_ - gap_start_ < text.size()) {
    expandGap(text.size() * 2);  // Exponential growth
  }
  
  std::copy(text.begin(), text.end(), buffer_.begin() + gap_start_);
  gap_start_ += text.size();
  
  updateLineOffsets(pos, text.size());
  return {};
}
```

**Testing Strategy** (Security & Performance Focus):
- Input fuzzing with malicious sequences (control chars, escape sequences)
- Buffer stress testing with large files (up to 100MB)
- Memory pressure simulation and recovery testing  
- UTF-8 validation with invalid sequences
- Performance benchmarking for <50ms operation targets

#### Day 3-4: Performance-Optimized Unicode & Navigation
**Objective**: Replace ASCII-only editor with production-ready Unicode support

**Files to Create**:
- `include/nx/tui/unicode_handler.hpp` - ICU-based Unicode handling
- `include/nx/tui/editor_commands.hpp` - Command pattern for undo/redo
- `src/tui/unicode_handler.cpp` - Unicode implementation
- `src/tui/editor_commands.cpp` - Command implementations
- `tests/unit/tui/unicode_handler_test.cpp` - Comprehensive Unicode tests

**Files to Modify**:
- `src/tui/tui_app.cpp` - **COMPLETE REWRITE** of `handleEditModeInput()` method
- `include/nx/tui/tui_app.hpp` - Add enhanced editor state

**Implementation Details** (Complete Architectural Overhaul):
```cpp
// COMPLETELY REWRITTEN: Security-first, performance-optimized input handling
std::expected<void, nx::Error> TUIApp::handleEditModeInput(const ftxui::Event& event) {
  // 1. SECURITY: Validate and sanitize input
  auto validated_input = EditorInputValidator::validateAndSanitize(event);
  if (!validated_input) {
    spdlog::warn("Invalid input rejected: {}", validated_input.error().message());
    return validated_input.error();
  }
  
  // 2. PERFORMANCE: Memory pressure check
  if (auto memory_check = enhanced_editor_state_.memory_monitor.checkMemoryPressure(); 
      !memory_check) {
    return memory_check.error();
  }
  
  // 3. NAVIGATION: Unicode-aware cursor movement
  if (event == ftxui::Event::ArrowLeft) {
    if (event.modifier() & ftxui::Event::Modifier::Ctrl) {
      return moveToWordBoundary(false);  // Unicode word boundaries
    } else {
      return moveCursorByCodePoint(-1);
    }
  }
  
  // 4. COMMAND PATTERN: All edits go through command system for undo/redo
  if (event.is_character()) {
    auto command = std::make_unique<InsertTextCommand>(
      enhanced_editor_state_.cursor, *validated_input);
    return executeCommand(std::move(command));
  }
  
  return {};
}

// UNICODE-AWARE: Word boundary detection using ICU
std::expected<void, nx::Error> TUIApp::moveToWordBoundary(bool forward) {
  auto current_line_result = enhanced_editor_state_.buffer.getLine(
    enhanced_editor_state_.cursor.line);
  if (!current_line_result) {
    return current_line_result.error();
  }
  
  auto word_bounds = UnicodeTextHandler::getWordBounds(
    *current_line_result, enhanced_editor_state_.cursor.col);
  if (!word_bounds) {
    return word_bounds.error();
  }
  
  size_t new_col = forward ? word_bounds->second : word_bounds->first;
  return setCursorPosition(enhanced_editor_state_.cursor.line, new_col);
}
```

**Key Features** (Production-Ready Security & Performance):
- **🌍 Unicode Support**: ICU-based word boundary detection for all languages
- **🛡️ Memory Safety**: All operations bounds-checked with graceful error handling
- **⚡ Performance**: O(1) local edits, guaranteed <50ms response times
- **🔄 Reversible Operations**: Command pattern with memory-efficient undo/redo
- **🔒 Security**: Complete input validation preventing injection attacks
- **📊 Monitoring**: Real-time memory pressure detection and mitigation
- **🎯 Reliability**: Zero-data-loss guarantee with atomic operations

#### Day 5-6: Secure Clipboard & Advanced Selection
**Objective**: Implement secure text selection with proper clipboard integration

**Implementation Details** (Security & Unicode Integration):
```cpp
// SECURE SELECTION: Unicode-aware with bounds checking
std::expected<void, nx::Error> TUIApp::updateSelection(size_t new_line, size_t new_col) {
  // Validate selection bounds
  auto validation = enhanced_editor_state_.cursor.validate(enhanced_editor_state_.buffer);
  if (!validation) {
    return validation.error();
  }
  
  if (!enhanced_editor_state_.selection.active) {
    // Start new selection
    enhanced_editor_state_.selection.start_line = enhanced_editor_state_.cursor.line;
    enhanced_editor_state_.selection.start_col = enhanced_editor_state_.cursor.col;
    enhanced_editor_state_.selection.active = true;
  }
  
  enhanced_editor_state_.selection.end_line = new_line;
  enhanced_editor_state_.selection.end_col = new_col;
  
  // Unicode-aware selection normalization
  return normalizeSelectionUnicode();
}

// SECURE CLIPBOARD: Integration with nx::util::Security
std::expected<void, nx::Error> SecureClipboardState::setContent(std::string_view content) {
  // Check for sensitive data before clipboard operation
  if (nx::util::Security::containsSensitiveData(std::string(content))) {
    spdlog::warn("Potentially sensitive data detected in clipboard operation");
    // Continue but log the operation
  }
  
  // Use RAII secure string
  internal_content = nx::util::SensitiveString(std::string(content));
  last_updated = std::chrono::steady_clock::now();
  
  // Attempt system clipboard sync
  return syncWithSystem();
}

// ATOMIC AUTO-SAVE: Prevent corruption with proper error handling
std::expected<void, nx::Error> TUIApp::performAutoSave() {
  if (!enhanced_editor_state_.has_changes) {
    return {};  // Nothing to save
  }
  
  // Create atomic save operation
  auto temp_path = createTempFile();
  if (!temp_path) {
    return temp_path.error();
  }
  
  // Write to temporary file first
  auto write_result = writeBufferToFile(enhanced_editor_state_.buffer, *temp_path);
  if (!write_result) {
    std::filesystem::remove(*temp_path);  // Cleanup
    return write_result.error();
  }
  
  // Atomic rename (POSIX guarantees atomicity)
  auto rename_result = atomicRename(*temp_path, current_note_path_);
  if (!rename_result) {
    std::filesystem::remove(*temp_path);  // Cleanup
    return rename_result.error();
  }
  
  enhanced_editor_state_.has_changes = false;
  return {};
}
```

**Key Features**:
- Shift+Arrow keys for text selection
- Visual selection highlighting
- 5-second auto-save timer
- Backup file creation
- Error recovery mechanisms

#### Day 7: Production-Ready Command System
**Objective**: Implement robust command pattern with memory-efficient undo/redo

**Implementation Details** (Command Pattern with Circular Buffer):
```cpp
// COMMAND PATTERN: Memory-efficient, type-safe undo/redo
class InsertTextCommand : public EditorCommand {
public:
  InsertTextCommand(CursorPosition pos, std::string text)
    : position_(pos), text_(std::move(text)), 
      timestamp_(std::chrono::steady_clock::now()) {}
  
  std::expected<void, nx::Error> execute(EditorBuffer& buffer) override {
    return buffer.insertAt(position_.line, position_.col, text_);
  }
  
  std::expected<void, nx::Error> undo(EditorBuffer& buffer) override {
    return buffer.deleteRange(position_.line, position_.col, 
                            position_.line, position_.col + text_.size());
  }
  
  bool canMergeWith(const EditorCommand& other) const override {
    auto* other_insert = dynamic_cast<const InsertTextCommand*>(&other);
    if (!other_insert) return false;
    
    // Merge consecutive single-character insertions within 1 second
    auto time_diff = other_insert->timestamp_ - timestamp_;
    return time_diff < std::chrono::seconds(1) && 
           text_.size() == 1 && other_insert->text_.size() == 1 &&
           arePositionsAdjacent(position_, other_insert->position_);
  }
  
private:
  CursorPosition position_;
  std::string text_;
  std::chrono::steady_clock::time_point timestamp_;
};

// MEMORY-EFFICIENT: Circular buffer prevents memory leaks
std::expected<void, nx::Error> TUIApp::executeCommand(
    std::unique_ptr<EditorCommand> command) {
  
  // Execute the command
  auto execute_result = command->execute(enhanced_editor_state_.buffer);
  if (!execute_result) {
    return execute_result.error();
  }
  
  // Check for command merging (optimize memory usage)
  if (enhanced_editor_state_.undo_current > 0) {
    auto& previous_cmd = enhanced_editor_state_.undo_history[
      (enhanced_editor_state_.undo_current - 1) % MAX_UNDO_HISTORY];
    
    if (previous_cmd && previous_cmd->canMergeWith(*command)) {
      // Replace previous command with merged version
      previous_cmd = std::make_unique<MergedCommand>(
        std::move(previous_cmd), std::move(command));
      return {};
    }
  }
  
  // Add to circular buffer
  enhanced_editor_state_.undo_history[
    enhanced_editor_state_.undo_current % MAX_UNDO_HISTORY] = std::move(command);
  enhanced_editor_state_.undo_current++;
  
  // Clear redo history (branching undo tree not supported)
  if (enhanced_editor_state_.undo_current < enhanced_editor_state_.undo_head) {
    enhanced_editor_state_.undo_head = enhanced_editor_state_.undo_current;
  }
  
  enhanced_editor_state_.has_changes = true;
  return {};
}
```

### **Phase 2: Advanced Features (Week 2)**

#### Day 1-3: High-Performance Search with Security
**Objective**: Implement secure, regex-capable search with caching optimizations

**Implementation Details** (Security & Performance Focus):
```cpp
// SECURE SEARCH: Protect search queries and validate regex
std::expected<void, nx::Error> FindState::performSearch(const EditorBuffer& buffer) {
  // Validate regex if in regex mode
  if (regex_mode) {
    auto regex_validation = validateRegex();
    if (!regex_validation) {
      return regex_validation.error();
    }
  }
  
  // Check cache first
  if (query.value() == last_query.value() && !cached_matches.empty()) {
    matches = cached_matches;
    return {};
  }
  
  matches.clear();
  
  // PERFORMANCE: Search directly in buffer without string copies
  const size_t line_count = buffer.getLineCount();
  for (size_t line_idx = 0; line_idx < line_count; ++line_idx) {
    auto line_result = buffer.getLine(line_idx);
    if (!line_result) {
      continue;  // Skip invalid lines
    }
    
    const auto& line_view = *line_result;
    
    if (regex_mode) {
      auto regex_matches = searchLineRegex(line_view, query.value());
      if (regex_matches) {
        for (const auto& match : *regex_matches) {
          matches.push_back({line_idx, match.first, match.second});
        }
      }
    } else {
      auto simple_matches = searchLineSimple(line_view, query.value());
      for (const auto& match : simple_matches) {
        matches.push_back({line_idx, match.first, match.second});
      }
    }
    
    // PERFORMANCE: Limit matches to prevent memory exhaustion
    if (matches.size() > 10000) {
      return std::unexpected(nx::Error(nx::ErrorCode::kMemoryError,
                                      "Too many search results"));
    }
  }
  
  // Cache results for future searches
  cached_matches = matches;
  last_query = nx::util::SensitiveString(query.value());
  
  return {};
}

// REGEX VALIDATION: Prevent regex DoS attacks
std::expected<void, nx::Error> FindState::validateRegex() const {
  try {
    std::regex test_regex(query.value());
    
    // Check for potentially expensive patterns
    if (containsExpensiveRegexPatterns(query.value())) {
      return std::unexpected(nx::Error(nx::ErrorCode::kSecurityError,
                                      "Potentially expensive regex pattern"));
    }
    
    return {};
  } catch (const std::regex_error& e) {
    return std::unexpected(nx::Error(nx::ErrorCode::kValidationError,
                                    std::string("Invalid regex: ") + e.what()));
  }
}
```

**Key Features** (Enterprise-Grade Search):
- **🔒 Secure Queries**: All search terms protected with `SensitiveString` RAII
- **🔍 Advanced Regex**: Full regex support with DoS attack prevention
- **⚡ Zero-Copy Search**: Direct buffer operations, no memory allocation
- **🛡️ Memory Protection**: Bounded results prevent memory exhaustion
- **💾 Smart Caching**: Intelligent result caching for performance
- **🌍 Unicode Ready**: Proper international text search with ICU
- **🎯 Precision**: Word boundary and case-sensitive options

#### Day 4-5: Advanced Navigation & Large File Support
**Objective**: Implement virtual scrolling and optimized navigation for large files

**Implementation Details** (Virtual Scrolling & Performance):
```cpp
// VIRTUAL SCROLLING: Handle files with millions of lines
class VirtualViewport {
public:
  struct ViewportInfo {
    size_t visible_start_line;
    size_t visible_end_line;
    size_t total_lines;
    size_t viewport_height;
  };
  
  std::expected<void, nx::Error> goToLine(size_t line_number) {
    // Validate bounds without loading entire file
    if (line_number == 0 || line_number > buffer_.getLineCount()) {
      return std::unexpected(nx::Error(nx::ErrorCode::kInvalidArgument,
                                      "Line number out of bounds"));
    }
    
    cursor_line_ = line_number - 1;  // Convert to 0-based
    cursor_col_ = 0;
    
    // PERFORMANCE: Only update viewport if line is not visible
    if (!isLineVisible(cursor_line_)) {
      centerLineInViewport(cursor_line_);
    }
    
    return {};
  }
  
  // SMART SCROLLING: Minimize redraws
  std::expected<ViewportInfo, nx::Error> updateViewport() {
    const size_t viewport_height = getTerminalHeight() - 5;  // Reserve space for UI
    
    // Calculate visible range
    size_t start_line = scroll_offset_;
    size_t end_line = std::min(scroll_offset_ + viewport_height, 
                              buffer_.getLineCount());
    
    // PERFORMANCE: Pre-load buffer lines for smooth scrolling
    auto preload_result = buffer_.preloadLines(start_line, end_line + 10);
    if (!preload_result) {
      return preload_result.error();
    }
    
    return ViewportInfo{start_line, end_line, buffer_.getLineCount(), viewport_height};
  }
  
private:
  bool isLineVisible(size_t line) const {
    return line >= scroll_offset_ && 
           line < scroll_offset_ + getTerminalHeight() - 5;
  }
  
  void centerLineInViewport(size_t line) {
    const size_t viewport_height = getTerminalHeight() - 5;
    if (line >= viewport_height / 2) {
      scroll_offset_ = line - viewport_height / 2;
    } else {
      scroll_offset_ = 0;
    }
  }
};

// LARGE FILE OPTIMIZATION: Memory-mapped files for huge documents
class LargeFileHandler {
public:
  static constexpr size_t LARGE_FILE_THRESHOLD = 10 * 1024 * 1024;  // 10MB
  
  std::expected<void, nx::Error> loadFile(const std::filesystem::path& path) {
    auto file_size = std::filesystem::file_size(path);
    
    if (file_size > LARGE_FILE_THRESHOLD) {
      // Use memory mapping for large files
      return loadFileMemoryMapped(path);
    } else {
      // Load normally for smaller files
      return loadFileNormal(path);
    }
  }
  
private:
  std::expected<void, nx::Error> loadFileMemoryMapped(const std::filesystem::path& path);
  std::expected<void, nx::Error> loadFileNormal(const std::filesystem::path& path);
};
```

#### Day 6-7: System Clipboard Integration
**Objective**: Integrate with system clipboard across platforms

**Files to Create**:
- `src/tui/system_clipboard.cpp`
- `tests/integration/clipboard_integration_test.cpp`

**Implementation Details**:
```cpp
// System clipboard integration
class SystemClipboard {
public:
  static bool isAvailable() {
    return detectProvider() != ClipboardProvider::None;
  }
  
  static std::optional<std::string> getSystemClipboard() {
    auto provider = detectProvider();
    switch (provider) {
      case ClipboardProvider::Pbcopy: // macOS
        return executePasteCommand("pbpaste");
      case ClipboardProvider::Xclip: // X11
        return executePasteCommand("xclip -selection clipboard -out");
      case ClipboardProvider::WlClipboard: // Wayland
        return executePasteCommand("wl-paste");
      default:
        return std::nullopt;
    }
  }
  
  static bool setSystemClipboard(const std::string& content) {
    auto provider = detectProvider();
    switch (provider) {
      case ClipboardProvider::Pbcopy:
        return executeCopyCommand("pbcopy", content);
      case ClipboardProvider::Xclip:
        return executeCopyCommand("xclip -selection clipboard", content);
      case ClipboardProvider::WlClipboard:
        return executeCopyCommand("wl-copy", content);
      default:
        return false;
    }
  }
  
private:
  enum class ClipboardProvider { None, Pbcopy, Xclip, WlClipboard };
  
  static ClipboardProvider detectProvider() {
    if (commandExists("pbcopy")) return ClipboardProvider::Pbcopy;
    if (commandExists("xclip")) return ClipboardProvider::Xclip;
    if (commandExists("wl-copy")) return ClipboardProvider::WlClipboard;
    return ClipboardProvider::None;
  }
};
```

### Phase 3: Quality of Life Improvements (Week 3)

#### Day 1-2: Large File Handling
**Objective**: Optimize performance for large documents

**Files to Create**:
- `src/tui/large_file_handler.cpp`
- `tests/unit/tui/large_file_test.cpp`

**Implementation Details**:
```cpp
// Large file optimization
class LargeFileHandler {
public:
  static constexpr size_t LARGE_FILE_THRESHOLD = 1000; // lines
  static constexpr size_t VERY_LARGE_FILE_THRESHOLD = 10000;
  
  void loadFileInChunks(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::string line;
    size_t line_count = 0;
    
    while (std::getline(file, line)) {
      lines_.push_back(std::move(line));
      ++line_count;
      
      if (line_count > VERY_LARGE_FILE_THRESHOLD) {
        showLargeFileWarning();
        break;
      }
    }
  }
  
  void handleVirtualScrolling() {
    // Implement virtual scrolling for very large files
    // Only render visible lines + buffer
  }
  
private:
  std::vector<std::string> lines_;
  size_t visible_start_ = 0;
  size_t visible_end_ = 0;
};
```

#### Day 3-4: Enhanced Line Operations
**Objective**: Implement common line manipulation features

**Implementation Details**:
```cpp
// Enhanced line operations
void TUIApp::duplicateLine() {
  const auto& lines = splitContentIntoLines(editor_state_.edit_content);
  if (editor_state_.cursor_line >= 0 && 
      editor_state_.cursor_line < static_cast<int>(lines.size())) {
    
    std::string line_to_duplicate = lines[editor_state_.cursor_line];
    
    // Save operation for undo
    saveEditOperation(EditOperation::Insert, 
                     editor_state_.cursor_line + 1, 0, 
                     line_to_duplicate + "\n");
    
    // Insert duplicated line
    insertLineAt(editor_state_.cursor_line + 1, line_to_duplicate);
    
    // Move cursor to duplicated line
    editor_state_.cursor_line++;
    editor_state_.cursor_col = 0;
  }
}

void TUIApp::deleteWord(bool forward) {
  const auto& lines = splitContentIntoLines(editor_state_.edit_content);
  if (lines.empty()) return;
  
  const std::string& current_line = lines[editor_state_.cursor_line];
  
  if (forward) {
    int end_pos = findWordBoundary(current_line, editor_state_.cursor_col, true);
    std::string deleted_text = current_line.substr(editor_state_.cursor_col, 
                                                  end_pos - editor_state_.cursor_col);
    
    saveEditOperation(EditOperation::Delete, 
                     editor_state_.cursor_line, editor_state_.cursor_col, 
                     deleted_text);
    
    deleteRange(editor_state_.cursor_line, editor_state_.cursor_col, 
               editor_state_.cursor_line, end_pos);
  } else {
    int start_pos = findWordBoundary(current_line, editor_state_.cursor_col, false);
    std::string deleted_text = current_line.substr(start_pos, 
                                                  editor_state_.cursor_col - start_pos);
    
    saveEditOperation(EditOperation::Delete, 
                     editor_state_.cursor_line, start_pos, 
                     deleted_text);
    
    deleteRange(editor_state_.cursor_line, start_pos, 
               editor_state_.cursor_line, editor_state_.cursor_col);
    
    editor_state_.cursor_col = start_pos;
  }
}
```

#### Day 5-6: Word Wrapping & Status Enhancements
**Objective**: Improve text display and user feedback

**Implementation Details**:
```cpp
// Word wrapping implementation
std::vector<std::string> TUIApp::wrapText(const std::string& text, int width) {
  std::vector<std::string> wrapped_lines;
  std::istringstream iss(text);
  std::string word;
  std::string current_line;
  
  while (iss >> word) {
    // Check if adding word would exceed width
    if (UTF8Handler::calculateDisplayWidth(current_line + " " + word) > width) {
      if (!current_line.empty()) {
        wrapped_lines.push_back(current_line);
        current_line.clear();
      }
    }
    
    if (!current_line.empty()) {
      current_line += " ";
    }
    current_line += word;
  }
  
  if (!current_line.empty()) {
    wrapped_lines.push_back(current_line);
  }
  
  return wrapped_lines;
}

// Enhanced status bar
Element TUIApp::renderStatusBar() const {
  std::vector<Element> status_elements;
  
  // File status
  std::string file_status = editor_state_.has_changes ? "Modified" : "Saved";
  status_elements.push_back(text(file_status) | bold);
  
  // Cursor position
  std::string position = "Line " + std::to_string(editor_state_.cursor_line + 1) + 
                        ", Col " + std::to_string(editor_state_.cursor_col + 1);
  status_elements.push_back(text(position));
  
  // Document statistics
  const auto& lines = splitContentIntoLines(editor_state_.edit_content);
  std::string stats = std::to_string(lines.size()) + " lines, " + 
                     std::to_string(editor_state_.edit_content.length()) + " chars";
  status_elements.push_back(text(stats));
  
  // Mode indicator
  std::string mode = editor_state_.selection.active ? "SELECT" : "EDIT";
  status_elements.push_back(text(mode) | inverted);
  
  return hbox({
    status_elements[0],
    separator(),
    status_elements[1], 
    separator(),
    status_elements[2],
    separator(),
    status_elements[3]
  });
}
```

#### Day 7: Configuration System
**Objective**: Implement user-configurable editor settings

**Implementation Details**:
```cpp
// Configuration structure for editor
struct EditorConfig {
  bool auto_save = true;
  int auto_save_delay = 5;  // seconds
  int tab_size = 4;
  bool word_wrap = false;
  bool show_line_numbers = false;
  bool highlight_current_line = true;
  int max_undo_history = 50;
  
  struct SearchConfig {
    bool case_sensitive = false;
    bool whole_words = false;
    bool highlight_all_matches = true;
  } search;
  
  struct ClipboardConfig {
    bool prefer_system_clipboard = true;
    int clipboard_timeout = 1000;  // ms
  } clipboard;
  
  struct TerminalConfig {
    bool detect_capabilities = true;
    bool force_basic_mode = false;
    int key_timeout = 100;  // ms for escape sequences
  } terminal;
};

// Configuration management
class EditorPreferences {
public:
  static EditorConfig loadConfig(const std::filesystem::path& config_path) {
    EditorConfig config;
    
    try {
      auto toml_data = toml::parse_file(config_path.string());
      
      if (auto editor_table = toml_data["editor"].as_table()) {
        config.auto_save = editor_table->get_or("auto_save", config.auto_save);
        config.auto_save_delay = editor_table->get_or("auto_save_delay", config.auto_save_delay);
        config.tab_size = editor_table->get_or("tab_size", config.tab_size);
        config.word_wrap = editor_table->get_or("word_wrap", config.word_wrap);
        // ... load other settings
      }
    } catch (const toml::parse_error& e) {
      // Log error and use defaults
    }
    
    return config;
  }
  
  static bool saveConfig(const EditorConfig& config, const std::filesystem::path& config_path) {
    try {
      toml::table toml_data;
      toml::table editor_table;
      
      editor_table.emplace("auto_save", config.auto_save);
      editor_table.emplace("auto_save_delay", config.auto_save_delay);
      editor_table.emplace("tab_size", config.tab_size);
      // ... save other settings
      
      toml_data.emplace("editor", std::move(editor_table));
      
      std::ofstream file(config_path);
      file << toml_data;
      return true;
    } catch (const std::exception& e) {
      return false;
    }
  }
};
```

### Phase 4: Markdown Features & Polish (Week 4)

#### Day 1-2: Markdown Shortcuts
**Objective**: Implement quick markdown formatting

**Implementation Details**:
```cpp
// Markdown formatting shortcuts
void TUIApp::wrapSelectionWithMarkdown(const std::string& prefix, const std::string& suffix) {
  if (!editor_state_.selection.active) {
    // No selection - insert markers at cursor
    insertText(prefix + suffix);
    // Move cursor between markers
    moveCursor(-static_cast<int>(suffix.length()), 0);
  } else {
    // Wrap selection
    std::string selected_text = getSelectedText();
    
    saveEditOperation(EditOperation::Replace, 
                     editor_state_.selection.start_line, 
                     editor_state_.selection.start_col,
                     selected_text);
    
    replaceSelection(prefix + selected_text + suffix);
    clearSelection();
  }
}

void TUIApp::handleMarkdownShortcuts(const ftxui::Event& event) {
  if (event.modifier() & ftxui::Event::Modifier::Ctrl) {
    switch (event.input()[0]) {
      case 'b': case 'B':
        wrapSelectionWithMarkdown("**", "**");
        break;
      case 'i': case 'I':
        wrapSelectionWithMarkdown("*", "*");
        break;
      case 'l': case 'L':
        wrapSelectionWithMarkdown("[", "](url)");
        break;
    }
  }
  
  if (event.modifier() & ftxui::Event::Modifier::Alt) {
    if (event.input().length() == 1 && std::isdigit(event.input()[0])) {
      int level = event.input()[0] - '0';
      if (level >= 1 && level <= 6) {
        convertToHeader(level);
      }
    }
  }
}

void TUIApp::convertToHeader(int level) {
  const auto& lines = splitContentIntoLines(editor_state_.edit_content);
  if (editor_state_.cursor_line >= 0 && 
      editor_state_.cursor_line < static_cast<int>(lines.size())) {
    
    std::string current_line = lines[editor_state_.cursor_line];
    
    // Remove existing header markers
    auto trimmed = std::regex_replace(current_line, std::regex("^#+\\s*"), "");
    
    // Add new header markers
    std::string new_line = std::string(level, '#') + " " + trimmed;
    
    saveEditOperation(EditOperation::Replace, 
                     editor_state_.cursor_line, 0, 
                     current_line);
    
    replaceLine(editor_state_.cursor_line, new_line);
  }
}
```

#### Day 3-4: List Handling & Auto-completion
**Objective**: Smart list operations and context-aware completion

**Implementation Details**:
```cpp
// Smart list handling
bool TUIApp::isListLine(const std::string& line) {
  std::regex list_pattern(R"(^\s*([*+-]|\d+\.)\s+)");
  return std::regex_search(line, list_pattern);
}

std::string TUIApp::getListMarker(const std::string& line) {
  std::regex pattern(R"(^(\s*)([*+-]|\d+\.))");
  std::smatch match;
  
  if (std::regex_search(line, match, pattern)) {
    return match[1].str() + match[2].str();
  }
  
  return "";
}

void TUIApp::handleListContinuation() {
  const auto& lines = splitContentIntoLines(editor_state_.edit_content);
  if (editor_state_.cursor_line >= 0 && 
      editor_state_.cursor_line < static_cast<int>(lines.size())) {
    
    const std::string& current_line = lines[editor_state_.cursor_line];
    
    if (isListLine(current_line)) {
      std::string marker = getListMarker(current_line);
      
      // Check if current line is empty list item
      if (std::regex_match(current_line, std::regex(R"(^\s*([*+-]|\d+\.)\s*$)"))) {
        // Exit list mode - remove the marker
        deleteLine();
      } else {
        // Continue list with same marker style
        if (std::regex_search(marker, std::regex(R"(\d+\.)"))) {
          // Increment numbered list
          int num = std::stoi(marker.substr(marker.find_last_of(' ') + 1));
          marker = std::regex_replace(marker, std::regex(R"(\d+)"), std::to_string(num + 1));
        }
        
        insertText("\n" + marker + " ");
      }
    } else {
      // Regular newline
      insertText("\n");
    }
  }
}

// Auto-completion for wiki-links and tags
void TUIApp::handleAutoCompletion(const std::string& trigger) {
  if (trigger == "[[") {
    // Wiki-link completion
    showNoteCompletionPopup();
  } else if (trigger == "#") {
    // Tag completion
    showTagCompletionPopup();
  }
}

void TUIApp::showNoteCompletionPopup() {
  // Get all note titles for completion
  std::vector<std::string> note_titles;
  for (const auto& note : state_.all_notes) {
    note_titles.push_back(note.title());
  }
  
  // Show completion popup
  // Implementation depends on FTXUI popup capabilities
}
```

#### Day 5-6: Basic Syntax Highlighting
**Objective**: Visual markdown indicators

**Implementation Details**:
```cpp
// Basic syntax highlighting for markdown
Element TUIApp::renderLineWithHighlighting(const std::string& line, bool is_cursor_line) {
  std::vector<Element> line_elements;
  
  // Header highlighting
  if (auto header_match = parseHeaderLine(line)) {
    int level = header_match->level;
    std::string text = header_match->text;
    
    ftxui::Color header_color = ftxui::Color::Default;
    switch (level) {
      case 1: header_color = ftxui::Color::Red; break;
      case 2: header_color = ftxui::Color::Blue; break;
      case 3: header_color = ftxui::Color::Green; break;
      case 4: header_color = ftxui::Color::Yellow; break;
      case 5: header_color = ftxui::Color::Magenta; break;
      case 6: header_color = ftxui::Color::Cyan; break;
    }
    
    line_elements.push_back(text(std::string(level, '#') + " ") | color(header_color));
    line_elements.push_back(text(text) | bold | color(header_color));
  } else {
    // Regular text with inline formatting
    line_elements = parseInlineFormatting(line);
  }
  
  // Highlight current line if enabled
  if (is_cursor_line && editor_state_.config.highlight_current_line) {
    return hbox(line_elements) | bgcolor(ftxui::Color::RGB(40, 40, 40));
  } else {
    return hbox(line_elements);
  }
}

std::vector<Element> TUIApp::parseInlineFormatting(const std::string& line) {
  std::vector<Element> elements;
  size_t pos = 0;
  
  // Simple regex-based parsing for bold, italic, links
  std::regex bold_pattern(R"(\*\*(.*?)\*\*)");
  std::regex italic_pattern(R"(\*(.*?)\*)");
  std::regex link_pattern(R"(\[(.*?)\]\((.*?)\))");
  std::regex code_pattern(R"(`(.*?)`)");
  
  // Parse and highlight inline elements
  // Implementation would tokenize and apply appropriate styling
  
  return elements;
}
```

#### Day 7: Final Integration & Documentation
**Objective**: Complete testing, optimization, and documentation

**Activities**:
1. **Performance Profiling**:
   - Profile all editor operations with valgrind/perf
   - Optimize hotpaths to meet <100ms targets
   - Memory usage analysis and optimization

2. **Integration Testing**:
   - Test across terminal matrix (xterm, gnome-terminal, kitty, alacritty)
   - SSH session testing
   - tmux/screen compatibility testing
   - Large file performance testing (10k+ lines)

3. **Documentation Updates**:
   - Update README.md with TUI editor features
   - Create comprehensive keybinding reference
   - Write user guide for TUI editor
   - API documentation for new classes

4. **Accessibility Review**:
   - Screen reader compatibility testing
   - Keyboard-only navigation verification
   - Color contrast validation

## Testing Strategy

### Unit Tests

#### Terminal Capability Tests (`tests/unit/tui/terminal_caps_test.cpp`)
```cpp
TEST(TerminalCapsTest, DetectBasicTerminal) {
  // Mock basic terminal environment
  setenv("TERM", "xterm", 1);
  
  auto caps = TerminalCapabilityDetector::detectCapabilities();
  
  EXPECT_TRUE(caps.supported_key_combos.count("Ctrl+Left"));
  EXPECT_TRUE(caps.supported_key_combos.count("Ctrl+Right"));
}

TEST(TerminalCapsTest, GracefulDegradationVT100) {
  setenv("TERM", "vt100", 1);
  
  auto caps = TerminalCapabilityDetector::detectCapabilities();
  
  // Should have minimal capabilities
  EXPECT_FALSE(caps.supports_true_color);
  EXPECT_FALSE(caps.supports_mouse);
}
```

#### UTF-8 Handler Tests (`tests/unit/tui/utf8_handler_test.cpp`)
```cpp
TEST(UTF8HandlerTest, DisplayWidthCalculation) {
  EXPECT_EQ(UTF8Handler::calculateDisplayWidth("hello"), 5);
  EXPECT_EQ(UTF8Handler::calculateDisplayWidth("你好"), 4);  // CJK characters
  EXPECT_EQ(UTF8Handler::calculateDisplayWidth("🚀"), 2);   // Emoji
}

TEST(UTF8HandlerTest, WordBoundaryDetection) {
  std::string text = "hello world, 你好世界";
  
  EXPECT_TRUE(UTF8Handler::isWordBoundary(text, 5));  // Space
  EXPECT_TRUE(UTF8Handler::isWordBoundary(text, 11)); // Comma
  EXPECT_TRUE(UTF8Handler::isWordBoundary(text, 13)); // Between ASCII and CJK
}
```

#### Editor Operations Tests (`tests/unit/tui/editor_test.cpp`)
```cpp
TEST(EditorTest, UndoRedoOperations) {
  TUIApp app(config, note_store, notebook_manager, search_index);
  
  // Simulate text insertion
  app.insertText("Hello");
  app.insertText(" World");
  
  // Test undo
  app.performUndo();
  EXPECT_EQ(app.getEditContent(), "Hello");
  
  // Test redo
  app.performRedo();
  EXPECT_EQ(app.getEditContent(), "Hello World");
}

TEST(EditorTest, SelectionAndClipboard) {
  TUIApp app(config, note_store, notebook_manager, search_index);
  app.setEditContent("Hello World");
  
  // Select "World"
  app.setSelection(0, 6, 0, 11);
  app.copySelection();
  
  // Move cursor and paste
  app.setCursorPosition(0, 0);
  app.pasteFromClipboard();
  
  EXPECT_EQ(app.getEditContent(), "WorldHello World");
}
```

### Integration Tests

#### TUI Editor Integration (`tests/integration/tui_editor_integration_test.cpp`)
```cpp
TEST(TUIEditorIntegration, CompleteEditingWorkflow) {
  // Test complete editing session
  auto app = createTestTUIApp();
  
  // Open note in editor
  app.openNoteInEditor(test_note_id);
  
  // Perform various operations
  app.simulateKeyPress("Ctrl+A");  // Select all
  app.simulateTextInput("# New Title\n\nThis is new content.");
  app.simulateKeyPress("Ctrl+S");  // Save
  
  // Verify changes were saved
  auto note = note_store.getNote(test_note_id);
  EXPECT_TRUE(note.has_value());
  EXPECT_EQ(note->content(), "# New Title\n\nThis is new content.");
}

TEST(TUIEditorIntegration, LargeFilePerformance) {
  // Test with large file (10k lines)
  std::string large_content = generateLargeContent(10000);
  auto note_id = createNoteWithContent(large_content);
  
  auto start_time = std::chrono::steady_clock::now();
  
  auto app = createTestTUIApp();
  app.openNoteInEditor(note_id);
  
  // Test cursor movement performance
  app.simulateKeyPress("Ctrl+End");  // Jump to end
  app.simulateKeyPress("Ctrl+Home"); // Jump to start
  
  auto elapsed = std::chrono::steady_clock::now() - start_time;
  EXPECT_LT(elapsed, std::chrono::milliseconds(100));
}
```

#### Clipboard Integration Tests (`tests/integration/clipboard_integration_test.cpp`)
```cpp
TEST(ClipboardIntegration, SystemClipboardMacOS) {
  if (!SystemClipboard::isAvailable()) {
    GTEST_SKIP() << "System clipboard not available";
  }
  
  std::string test_content = "Test clipboard content";
  
  EXPECT_TRUE(SystemClipboard::setSystemClipboard(test_content));
  
  auto retrieved = SystemClipboard::getSystemClipboard();
  EXPECT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, test_content);
}

TEST(ClipboardIntegration, FallbackToInternal) {
  // Mock system clipboard unavailable
  auto app = createTestTUIApp();
  app.disableSystemClipboard();
  
  app.setEditContent("Hello World");
  app.setSelection(0, 6, 0, 11);  // Select "World"
  app.copySelection();
  
  app.setCursorPosition(0, 0);
  app.pasteFromClipboard();
  
  EXPECT_EQ(app.getEditContent(), "WorldHello World");
}
```

### Performance Benchmarks

#### Response Time Benchmarks
```cpp
BENCHMARK(CursorMovement) {
  auto app = createTestTUIApp();
  app.setEditContent(generateLargeContent(1000));
  
  for (auto _ : state) {
    app.simulateKeyPress("ArrowRight");
  }
}

BENCHMARK(TextSelection) {
  auto app = createTestTUIApp();
  app.setEditContent(generateLargeContent(1000));
  
  for (auto _ : state) {
    app.simulateKeyPress("Shift+Ctrl+Right");
  }
}

BENCHMARK(UndoRedo) {
  auto app = createTestTUIApp();
  app.setEditContent("Sample text");
  
  // Build up undo history
  for (int i = 0; i < 50; ++i) {
    app.insertText(" " + std::to_string(i));
  }
  
  for (auto _ : state) {
    app.performUndo();
    app.performRedo();
  }
}
```

## Error Handling and Recovery

### File Operation Errors
```cpp
// Save failure recovery
void TUIApp::handleSaveError(const std::string& error) {
  // Log error
  spdlog::error("Save failed: {}", error);
  
  // Create emergency backup
  try {
    auto backup_path = createEmergencyBackup();
    showError("Save failed. Emergency backup created at: " + backup_path.string());
  } catch (const std::exception& e) {
    showError("Save failed and backup creation failed: " + std::string(e.what()));
  }
  
  // Schedule retry
  scheduleRetryWithBackoff();
}

std::filesystem::path TUIApp::createEmergencyBackup() {
  auto timestamp = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  
  std::stringstream ss;
  ss << "emergency_backup_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".md";
  
  auto backup_path = std::filesystem::temp_directory_path() / ss.str();
  
  std::ofstream file(backup_path);
  file << editor_state_.edit_content;
  
  return backup_path;
}
```

### Terminal Compatibility Errors
```cpp
// Graceful degradation for unsupported terminals
void TUIApp::handleUnsupportedTerminal() {
  if (!terminal_caps_.supports_true_color) {
    // Disable color highlighting
    editor_config_.use_syntax_highlighting = false;
    showInfo("Syntax highlighting disabled (terminal doesn't support colors)");
  }
  
  if (terminal_caps_.supported_key_combos.empty()) {
    // Show alternative keybindings
    showInfo("Limited key support detected. Press F1 for alternative shortcuts.");
  }
  
  // Enable compatibility mode
  editor_config_.compatibility_mode = true;
}
```

### Memory and Resource Errors
```cpp
// Handle out of memory conditions
void TUIApp::handleMemoryPressure() {
  spdlog::warn("Memory pressure detected, reducing undo history");
  
  // Reduce undo history size
  size_t new_size = std::max(size_t(10), editor_state_.undo_history.size() / 2);
  editor_state_.undo_history.resize(new_size);
  
  // Clear cached search results
  editor_state_.find_state.cached_matches.clear();
  
  // Force garbage collection if possible
  performGarbageCollection();
}
```

## Configuration and User Preferences

### TOML Configuration Format
```toml
[editor]
auto_save = true
auto_save_delay = 5  # seconds
tab_size = 4
word_wrap = false
show_line_numbers = false
highlight_current_line = true
max_undo_history = 50
use_syntax_highlighting = true

[editor.search]
case_sensitive = false
whole_words = false
highlight_all_matches = true
max_search_results = 1000

[editor.clipboard]
prefer_system_clipboard = true
clipboard_timeout = 1000  # ms
internal_clipboard_size = 10  # MB

[editor.terminal]
detect_capabilities = true
force_basic_mode = false
key_timeout = 100  # ms for escape sequences
color_support = "auto"  # auto, always, never

[editor.performance]
large_file_threshold = 1000    # lines
very_large_file_threshold = 10000
virtual_scrolling = true
lazy_rendering = true

[editor.markdown]
auto_continue_lists = true
smart_quotes = false
auto_link_detection = false
header_folding = false
```

### Dynamic Configuration Updates
```cpp
// Allow runtime configuration changes
void TUIApp::updateEditorConfig(const EditorConfig& new_config) {
  // Validate configuration
  if (!EditorPreferences::validateConfig(new_config)) {
    showError("Invalid configuration values");
    return;
  }
  
  // Apply changes that can be updated immediately
  editor_state_.config = new_config;
  
  // Update auto-save timer if needed
  if (new_config.auto_save_delay != old_config.auto_save_delay) {
    rescheduleAutoSave();
  }
  
  // Update terminal capabilities if forced
  if (new_config.terminal.force_basic_mode != old_config.terminal.force_basic_mode) {
    if (new_config.terminal.force_basic_mode) {
      enableBasicMode();
    } else {
      redetectTerminalCapabilities();
    }
  }
  
  // Trigger UI refresh
  requestRedraw();
}
```

## **COMPREHENSIVE** Risk Mitigation

### **CRITICAL** Security Risks and Mitigations

#### **Terminal Injection Attacks**
- **Risk**: Malicious escape sequences in editor content cause terminal compromise
- **Mitigation**: 
  - All output sanitized through `EditorInputValidator::sanitizeControlSequences()`
  - Input validation using `nx::util::Security` patterns
  - Comprehensive fuzzing with malicious sequences
  - Whitelist approach for allowed escape sequences

#### **Buffer Overflow Attacks**
- **Risk**: Unbounded input causes buffer overflow and potential code execution
- **Mitigation**:
  - All buffer operations bounds-checked with `std::expected<T, Error>`
  - Gap buffer implementation with overflow protection
  - Memory pressure detection and graceful degradation
  - Comprehensive buffer stress testing

#### **Clipboard Data Leakage**
- **Risk**: Sensitive data exposed through system clipboard
- **Mitigation**:
  - Mandatory `SensitiveString` usage for all clipboard operations
  - Sensitive data detection before clipboard operations
  - Configurable clipboard timeout and auto-clear
  - Audit logging for clipboard operations in encrypted mode

### **ENHANCED** Technical Risks and Mitigations

#### **Performance Degradation Under Load**
- **Risk**: Editor becomes unusable with large files or under memory pressure
- **Mitigation**: 
  - Virtual scrolling for files >10MB
  - Memory-mapped file support for very large files
  - Automatic memory pressure detection and mitigation
  - Performance testing with files up to 1GB
  - Configurable performance limits with user warnings

#### **Memory Exhaustion**
- **Risk**: Complex operations cause out-of-memory conditions
- **Mitigation**:
  - Circular buffer for undo history (bounded memory usage)
  - Smart pointer usage with RAII throughout
  - Memory monitoring with automatic cleanup
  - Graceful degradation when memory limits approached
  - Valgrind integration in CI pipeline

#### **Unicode Processing Vulnerabilities**
- **Risk**: Malformed UTF-8 causes crashes or security issues
- **Mitigation**:
  - ICU library usage for proven Unicode handling
  - Input validation and normalization for all text
  - Comprehensive Unicode fuzzing test suite
  - Graceful handling of invalid sequences
  - Character width calculation overflow protection

### **ENHANCED** User Experience Risks and Mitigations

#### **Data Loss Prevention**
- **Risk**: Editor bugs or system failures cause permanent data loss
- **Mitigation**:
  - Atomic file operations with rollback capability
  - Emergency backup creation on save failures
  - Command pattern ensures all operations are reversible
  - Auto-save with configurable intervals
  - Session persistence for crash recovery
  - File integrity validation on load

#### **Performance Degradation Under Real Usage**
- **Risk**: Editor becomes slow or unresponsive during normal use
- **Mitigation**:
  - Continuous performance monitoring in development
  - Real-world usage simulation in testing
  - Performance budgets enforced in CI
  - User feedback collection on performance
  - Configurable performance settings

#### **Accessibility Compliance**
- **Risk**: Editor unusable for users with disabilities
- **Mitigation**:
  - Screen reader compatibility testing
  - Keyboard-only navigation support
  - High contrast mode support
  - ARIA compatibility where applicable
  - User testing with accessibility experts

### **CRITICAL** Implementation Risks and Mitigations

#### **Architectural Complexity**
- **Risk**: Over-engineering leads to bugs and maintenance issues
- **Mitigation**:
  - Incremental implementation with continuous integration
  - Comprehensive code review process
  - Regular refactoring to maintain code quality
  - Clear separation of concerns
  - Extensive documentation and ADRs

#### **Integration Failures**
- **Risk**: New components don't integrate properly with existing nx infrastructure
- **Mitigation**:
  - Early integration testing with existing components
  - Comprehensive regression testing
  - Gradual rollout with feature flags
  - Backward compatibility guarantees
  - Rollback procedures for critical failures

## Implementation Timeline

### Week 1: Foundation and Safety
```
Day 1-2: Terminal Capability Detection
├── Create terminal detection framework
├── Implement key combination testing
├── Add escape sequence normalization
└── Write comprehensive terminal tests

Day 3-4: Enhanced Cursor Movement & UTF-8
├── Implement UTF-8 character handling
├── Add word-level navigation
├── Enhance cursor movement with multi-byte support
└── Write UTF-8 handling tests

Day 5-6: Text Selection & Auto-save
├── Implement selection tracking
├── Add visual selection highlighting
├── Create auto-save mechanism
└── Write selection and auto-save tests

Day 7: Undo/Redo System
├── Implement diff-based operations
├── Add operation batching
├── Create memory management
└── Write undo/redo tests
```

### Week 2: Navigation and Search
```
Day 1-3: Find in Note
├── Implement search functionality
├── Add result caching and highlighting
├── Create find dialog interface
└── Write search performance tests

Day 4-5: Go-to-line & Enhanced Scrolling
├── Implement go-to-line dialog
├── Add smart viewport management
├── Create smooth scrolling
└── Write navigation tests

Day 6-7: System Clipboard Integration
├── Detect clipboard providers across platforms
├── Implement secure clipboard handling
├── Add fallback mechanisms
└── Write clipboard integration tests
```

### Week 3: Quality of Life
```
Day 1-2: Large File Handling
├── Implement virtual scrolling
├── Add performance warnings
├── Create chunk loading
└── Write large file tests

Day 3-4: Enhanced Line Operations
├── Implement line duplication
├── Add word deletion
├── Create enhanced line editing
└── Write line operation tests

Day 5-6: Word Wrapping & Status
├── Implement smart word wrap
├── Enhance status bar display
├── Add document statistics
└── Write word wrap tests

Day 7: Configuration System
├── Create TOML configuration support
├── Implement preference management
├── Add configuration validation
└── Write configuration tests
```

### Week 4: Markdown Features & Polish
```
Day 1-2: Markdown Shortcuts
├── Implement bold/italic wrapping
├── Add link template insertion
├── Create header conversion
└── Write markdown formatting tests

Day 3-4: List Handling & Auto-completion
├── Implement smart list continuation
├── Add auto-completion framework
├── Create wiki-link completion
└── Write auto-completion tests

Day 5-6: Basic Syntax Highlighting
├── Implement header highlighting
├── Add inline formatting
├── Create link coloring
└── Write syntax highlighting tests

Day 7: Final Integration & Documentation
├── Complete integration testing
├── Performance profiling and optimization
├── Update documentation
└── Create user guides
```

## **ENHANCED** Success Metrics

### **CRITICAL** Security Requirements
- 🔒 **Zero High/Critical Security Issues**: Pass security audit
- 🔒 **Input Validation**: 100% of user input validated and sanitized
- 🔒 **Memory Safety**: Zero buffer overflows or memory corruption
- 🔒 **Clipboard Security**: All sensitive data protected with `SensitiveString`
- 🔒 **Terminal Injection Prevention**: All escape sequences sanitized
- 🔒 **File Operation Security**: Atomic operations prevent corruption

### **ENHANCED** Performance Requirements (Stricter Targets)
- ⚡ **Response Time**: All operations <50ms (upgraded from 100ms)
- ⚡ **Cursor Movement**: <10ms (upgraded from 50ms)
- ⚡ **Memory Usage**: <20MB additional overhead (upgraded from 50MB)
- ⚡ **Large Files**: Handle 100k+ lines with <500ms load time
- ⚡ **Search Performance**: <200ms for 1M+ character documents
- ⚡ **Startup Time**: Editor mode activation <100ms
- ⚡ **Memory Pressure**: Graceful degradation under memory constraints

### **COMPREHENSIVE** Quality Requirements
- ✅ **Test Coverage**: 95%+ coverage (upgraded from 90%)
- ✅ **Security Testing**: Fuzzing, injection testing, memory analysis
- ✅ **Performance Testing**: Automated benchmarking in CI
- ✅ **Integration Testing**: Real terminal compatibility matrix
- ✅ **Accessibility Testing**: Screen reader and keyboard-only navigation
- ✅ **Error Recovery**: Comprehensive fault injection testing
- ✅ **Memory Safety**: Valgrind clean, no leaks or corruption

### **PRODUCTION-READY** Functional Requirements
- ✅ **Unicode Support**: Full international text support with ICU
- ✅ **Large File Support**: Virtual scrolling for files up to 1GB
- ✅ **Backward Compatibility**: 100% existing functionality preserved
- ✅ **Error Handling**: All operations use `std::expected<T, Error>`
- ✅ **Command Pattern**: All edits reversible with efficient undo/redo
- ✅ **Terminal Compatibility**: Works on all major terminals via FTXUI

### **EXCEPTIONAL** User Experience Requirements
- ✅ **Zero Data Loss**: No scenarios where user data can be lost
- ✅ **Intuitive Operation**: Follows standard editor conventions
- ✅ **Clear Feedback**: Immediate visual and status feedback
- ✅ **Error Recovery**: Helpful error messages with recovery suggestions
- ✅ **Accessibility**: Full screen reader and keyboard navigation support
- ✅ **Performance Transparency**: Users informed of long operations

## Future Considerations

### Post-MVP3 Enhancements
- **Collaborative Editing**: Real-time multi-user support with conflict resolution
- **Plugin System**: User-extensible functionality with Lua scripting
- **Custom Themes**: User-configurable color schemes and UI customization
- **Split Panes**: Multiple document editing with synchronized scrolling
- **Advanced Git Integration**: Inline diff view, merge conflict resolution

### Integration Opportunities
- **External Editor Sync**: Two-way synchronization with VS Code, Vim, Emacs
- **AI Assistance**: In-editor AI suggestions, grammar checking, content generation
- **Enhanced Export**: Live preview for PDF/HTML export with real-time updates
- **Advanced Search**: Full-text search across all notes with relevance ranking

## Documentation Requirements

### User Documentation
1. **README.md Updates**: Add comprehensive TUI editor section
2. **Editor User Guide**: Complete guide with screenshots and examples
3. **Keybinding Reference**: Quick reference card (printable)
4. **Video Tutorials**: Screen recordings of key workflows
5. **Migration Guide**: Help existing users adapt to new features

### Developer Documentation
1. **API Documentation**: Complete doxygen documentation for all new classes
2. **Architecture Decision Records**: Document major design choices
3. **Testing Guide**: How to run tests, add new tests, debug issues
4. **Contributing Guide**: Guidelines for editor feature contributions
5. **Performance Guide**: Optimization techniques and profiling instructions

### Help System Integration
1. **Context-sensitive Help**: Show relevant shortcuts based on current mode
2. **Error Message Improvements**: Clear, actionable error messages with suggestions
3. **Accessibility Documentation**: Screen reader compatibility and keyboard navigation
4. **Terminal Compatibility Guide**: Recommended terminals and configuration

---

## **IMPLEMENTATION READINESS CHECKLIST**

### **Phase 1 Prerequisites** ✅
- [ ] Security review of current codebase completed
- [ ] Performance baseline established for existing editor
- [ ] ICU library integration approved and tested
- [ ] Team training on security patterns completed
- [ ] CI pipeline updated for enhanced testing

### **Security Approval** 🔒
- [ ] Security team review of implementation plan
- [ ] Threat model analysis completed
- [ ] Security testing strategy approved
- [ ] Incident response plan updated

### **Performance Validation** ⚡
- [ ] Performance testing environment configured
- [ ] Benchmark suite implemented
- [ ] Memory profiling tools integrated
- [ ] Performance regression detection active

---

**Document Status**: **ENHANCED - Implementation Ready with Critical Security & Performance Focus**  
**Created**: 2025-08-16  
**Updated**: 2025-08-16 (Critical Analysis Integration)  
**Version**: 2.0 (Major Revision)  
**Total Estimated Effort**: 4 weeks (160 hours) - **High Risk/High Value**  
**Priority**: **CRITICAL** (MVP3 core deliverable with security implications)

**⚠️ CRITICAL NOTICE**: This plan represents a complete architectural overhaul addressing significant security and performance issues in the original design. Implementation requires careful attention to the security mitigations and performance targets outlined above.

This **enhanced** implementation plan provides a production-ready foundation for nx's TUI editor that:
- **Maintains** existing Unix philosophy and user experience
- **Enhances** security with proper input validation and memory safety
- **Optimizes** performance for large files and intensive usage
- **Ensures** maintainability through proper architectural patterns
- **Guarantees** reliability through comprehensive testing strategies