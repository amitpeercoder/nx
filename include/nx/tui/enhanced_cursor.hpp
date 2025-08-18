#pragma once

#include <memory>
#include <vector>
#include <chrono>
#include "nx/common.hpp"
#include "nx/tui/editor_buffer.hpp"
#include "nx/tui/unicode_handler.hpp"
#include "nx/tui/editor_security.hpp"

namespace nx::tui {

/**
 * @brief Enhanced cursor position with Unicode and security awareness
 * 
 * Provides sophisticated cursor management with:
 * - Unicode-aware positioning (handles multi-byte characters correctly)
 * - Bounds checking for all operations
 * - Selection state management
 * - Virtual column tracking for vertical movement
 * - Performance optimization for large documents
 */
class EnhancedCursor {
public:
    /**
     * @brief Cursor movement direction
     */
    enum class Direction {
        Up,
        Down,
        Left,
        Right,
        Home,        // Beginning of line
        End,         // End of line
        PageUp,
        PageDown,
        DocumentHome, // Beginning of document
        DocumentEnd   // End of document
    };

    /**
     * @brief Word boundary type for word navigation
     */
    enum class WordBoundary {
        Normal,      // Standard word boundaries (whitespace, punctuation)
        Programming, // Programming-aware (camelCase, snake_case)
        Unicode      // Unicode-aware word boundaries using ICU
    };

    /**
     * @brief Selection mode
     */
    enum class SelectionMode {
        None,        // No selection
        Character,   // Character-by-character selection
        Word,        // Word selection
        Line,        // Line selection
        Block        // Block/column selection
    };

    /**
     * @brief Configuration for cursor behavior
     */
    struct Config {
        bool enable_virtual_column;     // Maintain virtual column for up/down movement
        bool enable_word_wrap;          // Handle word-wrapped lines
        WordBoundary word_boundary_type; // Type of word boundary detection
        size_t page_size;               // Lines per page for page up/down
        bool clamp_to_content;          // Clamp cursor to actual content bounds
        bool allow_past_eol;            // Allow cursor past end of line

        Config()
            : enable_virtual_column(true)
            , enable_word_wrap(false)
            , word_boundary_type(WordBoundary::Unicode)
            , page_size(20)
            , clamp_to_content(true)
            , allow_past_eol(false) {}
    };

    /**
     * @brief Current cursor state
     */
    struct Position {
        size_t line;                    // Line number (0-based)
        size_t column;                  // Column number (0-based, Unicode code points)
        size_t byte_offset;             // Byte offset within line (for UTF-8)
        size_t display_column;          // Display column (accounts for wide characters)
        size_t virtual_column;          // Virtual column for vertical movement
        
        Position() : line(0), column(0), byte_offset(0), display_column(0), virtual_column(0) {}
        
        bool operator==(const Position& other) const {
            return line == other.line && column == other.column;
        }
        
        bool operator!=(const Position& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief Selection state
     */
    struct Selection {
        Position start;
        Position end;
        SelectionMode mode;
        bool active;
        
        Selection() : mode(SelectionMode::None), active(false) {}
        
        /**
         * @brief Check if selection is empty
         */
        bool isEmpty() const {
            return !active || start == end;
        }
        
        /**
         * @brief Get normalized selection (start <= end)
         */
        std::pair<Position, Position> getNormalized() const {
            if (start.line < end.line || (start.line == end.line && start.column <= end.column)) {
                return {start, end};
            } else {
                return {end, start};
            }
        }
    };

    explicit EnhancedCursor(Config config = Config{});
    ~EnhancedCursor();

    /**
     * @brief Initialize cursor with buffer
     * @param buffer Buffer to operate on
     * @return Result indicating success or error
     */
    Result<void> initialize(const EditorBuffer& buffer);

    /**
     * @brief Update buffer reference (when buffer changes)
     * @param buffer New buffer to operate on
     * @return Result indicating success or error
     */
    Result<void> updateBuffer(const EditorBuffer& buffer);

    /**
     * @brief Get current cursor position
     * @return Current position
     */
    const Position& getPosition() const;

    /**
     * @brief Set cursor position with bounds checking
     * @param line Line number
     * @param column Column number
     * @return Result indicating success or bounds error
     */
    Result<void> setPosition(size_t line, size_t column);

    /**
     * @brief Set cursor position with full position info
     * @param position Position to set
     * @return Result indicating success or bounds error
     */
    Result<void> setPosition(const Position& position);

    /**
     * @brief Move cursor in specified direction
     * @param direction Direction to move
     * @param extend_selection Whether to extend current selection
     * @return Result indicating success or bounds error
     */
    Result<void> move(Direction direction, bool extend_selection = false);

    /**
     * @brief Move cursor by specified number of characters
     * @param char_count Number of characters to move (negative for backward)
     * @param extend_selection Whether to extend current selection
     * @return Result indicating success or bounds error
     */
    Result<void> moveByCharacters(int char_count, bool extend_selection = false);

    /**
     * @brief Move cursor by specified number of lines
     * @param line_count Number of lines to move (negative for up)
     * @param extend_selection Whether to extend current selection
     * @return Result indicating success or bounds error
     */
    Result<void> moveByLines(int line_count, bool extend_selection = false);

    /**
     * @brief Move cursor to next word boundary
     * @param extend_selection Whether to extend current selection
     * @return Result indicating success or bounds error
     */
    Result<void> moveToNextWord(bool extend_selection = false);

    /**
     * @brief Move cursor to previous word boundary
     * @param extend_selection Whether to extend current selection
     * @return Result indicating success or bounds error
     */
    Result<void> moveToPreviousWord(bool extend_selection = false);

    /**
     * @brief Move cursor to specific display column
     * @param display_column Target display column
     * @return Result indicating success or bounds error
     */
    Result<void> moveToDisplayColumn(size_t display_column);

    /**
     * @brief Get current selection state
     * @return Current selection
     */
    const Selection& getSelection() const;

    /**
     * @brief Start selection at current position
     * @param mode Selection mode to use
     * @return Result indicating success or error
     */
    Result<void> startSelection(SelectionMode mode = SelectionMode::Character);

    /**
     * @brief End selection
     */
    void endSelection();

    /**
     * @brief Clear selection
     */
    void clearSelection();

    /**
     * @brief Select all content
     * @return Result indicating success or error
     */
    Result<void> selectAll();

    /**
     * @brief Select current word
     * @return Result indicating success or error
     */
    Result<void> selectWord();

    /**
     * @brief Select current line
     * @return Result indicating success or error
     */
    Result<void> selectLine();

    /**
     * @brief Check if position is within selection
     * @param position Position to check
     * @return true if position is selected
     */
    bool isPositionSelected(const Position& position) const;

    /**
     * @brief Get text content of current selection
     * @return Result with selected text or error
     */
    Result<std::string> getSelectedText() const;

    /**
     * @brief Get cursor position bounds information
     */
    struct Bounds {
        size_t max_line;
        size_t max_column_for_line;
        size_t total_lines;
        size_t total_characters;
    };

    /**
     * @brief Get current bounds information
     * @return Bounds information
     */
    Bounds getBounds() const;

    /**
     * @brief Check if cursor is at beginning of document
     * @return true if at document start
     */
    bool isAtDocumentStart() const;

    /**
     * @brief Check if cursor is at end of document
     * @return true if at document end
     */
    bool isAtDocumentEnd() const;

    /**
     * @brief Check if cursor is at beginning of line
     * @return true if at line start
     */
    bool isAtLineStart() const;

    /**
     * @brief Check if cursor is at end of line
     * @return true if at line end
     */
    bool isAtLineEnd() const;

    /**
     * @brief Get character at current cursor position
     * @return Result with character or bounds error
     */
    Result<UChar32> getCharacterAtCursor() const;

    /**
     * @brief Get line content at current cursor position
     * @return Result with line content or error
     */
    Result<std::string> getCurrentLine() const;

    /**
     * @brief Update configuration
     * @param new_config New configuration
     */
    void updateConfig(const Config& new_config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const Config& getConfig() const;

    /**
     * @brief Performance statistics
     */
    struct Statistics {
        size_t movements;
        size_t position_validations;
        size_t unicode_operations;
        size_t bounds_checks;
        std::chrono::milliseconds avg_operation_time;
        size_t memory_usage;
    };

    /**
     * @brief Get performance statistics
     * @return Current statistics
     */
    Statistics getStatistics() const;

private:
    Config config_;
    Position position_;
    Selection selection_;
    
    // Buffer reference (non-owning)
    const EditorBuffer* buffer_;
    
    // Note: Unicode handling and security validation will be added when all dependencies are complete
    // For now, using simplified implementations
    
    // Performance tracking
    mutable size_t movements_;
    mutable size_t position_validations_;
    mutable size_t unicode_operations_;
    mutable size_t bounds_checks_;
    mutable std::chrono::steady_clock::time_point last_operation_time_;
    
    // Cached information for performance
    mutable bool bounds_cache_valid_;
    mutable Bounds cached_bounds_;
    mutable std::string cached_line_;
    mutable size_t cached_line_number_;

    /**
     * @brief Validate and normalize position
     * @param pos Position to validate
     * @return Result with normalized position or bounds error
     */
    Result<Position> validateAndNormalizePosition(const Position& pos) const;

    /**
     * @brief Calculate display metrics for position
     * @param pos Position to calculate for
     * @return Result with updated position including display info
     */
    Result<Position> calculateDisplayMetrics(Position pos) const;

    /**
     * @brief Update selection when cursor moves
     * @param extend_selection Whether to extend selection
     */
    void updateSelectionOnMove(bool extend_selection);

    /**
     * @brief Find word boundary in specified direction
     * @param forward True for forward, false for backward
     * @return Result with new position or bounds error
     */
    Result<Position> findWordBoundary(bool forward) const;

    /**
     * @brief Update virtual column for vertical movement
     */
    void updateVirtualColumn();

    /**
     * @brief Invalidate cached bounds information
     */
    void invalidateBoundsCache() const;

    /**
     * @brief Rebuild bounds cache if needed
     */
    void rebuildBoundsCache() const;

    /**
     * @brief Record operation for performance tracking
     */
    void recordOperation() const;

    /**
     * @brief Safe line access with caching
     * @param line_number Line number to access
     * @return Result with line content or error
     */
    Result<std::string> getLineWithCache(size_t line_number) const;
};

/**
 * @brief Cursor manager for multiple cursors (future extension)
 * 
 * Manages multiple cursor positions for advanced editing features.
 * Currently supports single cursor but designed for future multi-cursor support.
 */
class CursorManager {
public:
    explicit CursorManager(EnhancedCursor::Config config = EnhancedCursor::Config{});

    /**
     * @brief Initialize with buffer
     * @param buffer Buffer to operate on
     * @return Result indicating success or error
     */
    Result<void> initialize(const EditorBuffer& buffer);

    /**
     * @brief Get primary cursor
     * @return Reference to primary cursor
     */
    EnhancedCursor& getPrimaryCursor();

    /**
     * @brief Get primary cursor (const version)
     * @return Const reference to primary cursor
     */
    const EnhancedCursor& getPrimaryCursor() const;

    /**
     * @brief Update buffer for all cursors
     * @param buffer New buffer to operate on
     * @return Result indicating success or error
     */
    Result<void> updateBuffer(const EditorBuffer& buffer);

private:
    std::unique_ptr<EnhancedCursor> primary_cursor_;
    // Future: std::vector<std::unique_ptr<EnhancedCursor>> additional_cursors_;
};

} // namespace nx::tui