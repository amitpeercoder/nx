#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/ubrk.h>
#include <unicode/unorm2.h>
#include <unicode/ucnv.h>
#include "nx/common.hpp"

namespace nx::tui {

/**
 * @brief Unicode text handling with ICU library support
 * 
 * Provides comprehensive Unicode support for the TUI editor including:
 * - Proper character width calculation for CJK and emoji
 * - Word boundary detection for international text
 * - UTF-8 validation and normalization
 * - Cursor positioning with multi-byte character awareness
 */
class UnicodeHandler {
public:
    /**
     * @brief Initialize Unicode handler with locale settings
     * @param locale Locale identifier (e.g., "en_US", "ja_JP", "zh_CN")
     * @return Result indicating success or error
     */
    static Result<void> initialize(const std::string& locale = "");

    /**
     * @brief Cleanup Unicode resources
     */
    static void cleanup();

    /**
     * @brief Calculate display width of UTF-8 string
     * @param text UTF-8 encoded text
     * @return Display width in terminal columns
     */
    static size_t calculateDisplayWidth(const std::string& text);

    /**
     * @brief Calculate display width of single Unicode code point
     * @param codepoint Unicode code point
     * @return Display width (0, 1, or 2)
     */
    static size_t getCodePointWidth(UChar32 codepoint);

    /**
     * @brief Convert UTF-8 string to UTF-16 for ICU processing
     * @param utf8_text UTF-8 encoded text
     * @return Result with UTF-16 string or error
     */
    static Result<std::u16string> utf8ToUtf16(const std::string& utf8_text);

    /**
     * @brief Convert UTF-16 string back to UTF-8
     * @param utf16_text UTF-16 encoded text
     * @return Result with UTF-8 string or error
     */
    static Result<std::string> utf16ToUtf8(const std::u16string& utf16_text);

    /**
     * @brief Find character position from display column position
     * @param text UTF-8 text
     * @param display_col Display column position
     * @return Character index in UTF-8 string
     */
    static Result<size_t> displayColumnToCharIndex(const std::string& text, size_t display_col);

    /**
     * @brief Find display column from character index
     * @param text UTF-8 text
     * @param char_index Character index in UTF-8 string
     * @return Display column position
     */
    static Result<size_t> charIndexToDisplayColumn(const std::string& text, size_t char_index);

    /**
     * @brief Find next word boundary
     * @param text UTF-8 text
     * @param current_pos Current character position
     * @return Position of next word boundary
     */
    static Result<size_t> findNextWordBoundary(const std::string& text, size_t current_pos);

    /**
     * @brief Find previous word boundary
     * @param text UTF-8 text
     * @param current_pos Current character position
     * @return Position of previous word boundary
     */
    static Result<size_t> findPreviousWordBoundary(const std::string& text, size_t current_pos);

    /**
     * @brief Validate UTF-8 string and report any issues
     * @param text Text to validate
     * @return Result indicating validity or specific error
     */
    static Result<void> validateUtf8(const std::string& text);

    /**
     * @brief Normalize UTF-8 text (NFC normalization)
     * @param text Text to normalize
     * @return Result with normalized text or error
     */
    static Result<std::string> normalizeText(const std::string& text);

    /**
     * @brief Check if character is a combining mark
     * @param codepoint Unicode code point to check
     * @return true if character is a combining mark
     */
    static bool isCombiningMark(UChar32 codepoint);

    /**
     * @brief Check if character is a line separator
     * @param codepoint Unicode code point to check
     * @return true if character causes line break
     */
    static bool isLineSeparator(UChar32 codepoint);

    /**
     * @brief Check if character is whitespace
     * @param codepoint Unicode code point to check
     * @return true if character is Unicode whitespace
     */
    static bool isWhitespace(UChar32 codepoint);

    /**
     * @brief Get character category
     * @param codepoint Unicode code point
     * @return Unicode general category
     */
    static int8_t getCharacterCategory(UChar32 codepoint);
    
    /**
     * @brief Get Unicode code point at specific position
     * @param text UTF-8 text
     * @param index Byte index (will be updated to next character)
     * @return Unicode code point or error
     */
    static Result<UChar32> getCodePointAt(const std::string& text, size_t& index);

    /**
     * @brief Safe UTF-8 character iteration
     */
    class Utf8Iterator {
    public:
        explicit Utf8Iterator(const std::string& text);
        
        bool hasNext() const;
        UChar32 next();
        UChar32 previous();
        size_t getIndex() const;
        void setIndex(size_t index);
        size_t length() const;

    private:
        const std::string& text_;
        size_t index_;
        size_t length_;
    };

    /**
     * @brief Text measurement utilities
     */
    struct TextMetrics {
        size_t character_count;     // Number of Unicode characters
        size_t display_width;       // Terminal display width
        size_t byte_length;         // UTF-8 byte length
        size_t line_count;          // Number of lines
        bool contains_rtl;          // Contains right-to-left text
        bool contains_combining;    // Contains combining characters
    };

    /**
     * @brief Analyze text and return detailed metrics
     * @param text UTF-8 text to analyze
     * @return Text metrics
     */
    static TextMetrics analyzeText(const std::string& text);

    /**
     * @brief Truncate text to fit within display width
     * @param text UTF-8 text to truncate
     * @param max_width Maximum display width
     * @param ellipsis Whether to add ellipsis
     * @return Result with truncated text or error
     */
    static Result<std::string> truncateToWidth(const std::string& text, size_t max_width, bool ellipsis = true);

    /**
     * @brief Split text at word boundaries to fit within width
     * @param text UTF-8 text to wrap
     * @param max_width Maximum width per line
     * @return Vector of wrapped lines
     */
    static std::vector<std::string> wrapTextToWidth(const std::string& text, size_t max_width);

private:
    static bool initialized_;
    static UBreakIterator* word_break_iter_;
    static const UNormalizer2* normalizer_;

    static Result<void> initializeBreakIterator();
    static void cleanupBreakIterator();
    
    // Helper functions for ICU operations
    static size_t getUtf8CharLength(uint8_t first_byte);
    static bool isValidUtf8Sequence(const uint8_t* bytes, size_t length);
};

/**
 * @brief Cursor position helper for Unicode-aware text editing
 * 
 * Manages cursor positioning in text with multi-byte characters,
 * combining characters, and variable-width characters.
 */
class UnicodeCursor {
public:
    explicit UnicodeCursor(const std::string& text);

    /**
     * @brief Move cursor to specific character position
     * @param char_index Character index (not byte index)
     * @return Result indicating success or bounds error
     */
    Result<void> setPosition(size_t char_index);

    /**
     * @brief Move cursor forward by one character
     * @return Result indicating success or end-of-text
     */
    Result<void> moveForward();

    /**
     * @brief Move cursor backward by one character
     * @return Result indicating success or start-of-text
     */
    Result<void> moveBackward();

    /**
     * @brief Move cursor to next word boundary
     * @return Result indicating success or end-of-text
     */
    Result<void> moveToNextWord();

    /**
     * @brief Move cursor to previous word boundary
     * @return Result indicating success or start-of-text
     */
    Result<void> moveToPreviousWord();

    /**
     * @brief Get current character index
     * @return Character index position
     */
    size_t getCharacterIndex() const;

    /**
     * @brief Get current byte index in UTF-8 string
     * @return Byte index position
     */
    size_t getByteIndex() const;

    /**
     * @brief Get current display column
     * @return Display column position
     */
    size_t getDisplayColumn() const;
    
    /**
     * @brief Move cursor to specific display column
     * @param column Display column to move to
     * @return Result indicating success or error
     */
    Result<void> moveToDisplayColumn(size_t column);

    /**
     * @brief Move cursor to specific display column
     * @param column Display column to move to
     * @return Result indicating success or bounds error
     */
    Result<void> moveToColumn(size_t column);

    /**
     * @brief Check if cursor is at beginning of text
     * @return true if at start
     */
    bool isAtStart() const;

    /**
     * @brief Check if cursor is at end of text
     * @return true if at end
     */
    bool isAtEnd() const;

    /**
     * @brief Get character at current position
     * @return Unicode code point at cursor
     */
    UChar32 getCurrentCharacter() const;

    /**
     * @brief Update text content (invalidates position cache)
     * @param new_text New text content
     */
    void updateText(const std::string& new_text);

private:
    std::string text_;
    size_t char_index_;
    size_t byte_index_;
    size_t display_column_;
    
    // Cached positions for performance
    std::vector<size_t> char_to_byte_map_;
    std::vector<size_t> char_to_column_map_;
    bool cache_valid_;
    
    void buildPositionCache();

    void rebuildCache();
    void invalidateCache();
    Result<void> validatePosition();
};

} // namespace nx::tui