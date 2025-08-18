#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include "nx/common.hpp"
#include "nx/util/security.hpp"

namespace nx::tui {

/**
 * @brief High-performance gap buffer for editor text storage
 * 
 * Implements a gap buffer data structure that provides O(1) insertion/deletion
 * at the cursor position, dramatically improving editor performance compared
 * to string-based approaches.
 */
class GapBuffer {
public:
    /**
     * @brief Configuration for gap buffer behavior
     */
    struct Config {
        size_t initial_gap_size;     // Initial gap size in characters
        size_t min_gap_size;          // Minimum gap size to maintain
        size_t max_gap_size;         // Maximum gap size before compaction
        double gap_growth_factor;     // Factor to grow gap by when needed
        size_t max_buffer_size; // 100MB max buffer
        
        Config() 
            : initial_gap_size(1024)
            , min_gap_size(256)
            , max_gap_size(8192)
            , gap_growth_factor(1.5)
            , max_buffer_size(100 * 1024 * 1024) {}
    };

    explicit GapBuffer(Config config = Config{});
    ~GapBuffer();

    // Disable copy constructor and assignment (use move semantics)
    GapBuffer(const GapBuffer&) = delete;
    GapBuffer& operator=(const GapBuffer&) = delete;
    GapBuffer(GapBuffer&& other) noexcept;
    GapBuffer& operator=(GapBuffer&& other) noexcept;

    /**
     * @brief Initialize buffer with content
     * @param content Initial content to load
     * @return Result indicating success or error
     */
    Result<void> initialize(const std::string& content);

    /**
     * @brief Insert character at current gap position
     * @param ch Character to insert
     * @return Result indicating success or error
     */
    Result<void> insertChar(char ch);

    /**
     * @brief Insert string at current gap position
     * @param text String to insert
     * @return Result indicating success or error
     */
    Result<void> insertString(const std::string& text);

    /**
     * @brief Delete character before gap (backspace)
     * @return Result with deleted character or error
     */
    Result<char> deleteCharBefore();

    /**
     * @brief Delete character after gap (delete)
     * @return Result with deleted character or error
     */
    Result<char> deleteCharAfter();

    /**
     * @brief Delete range of characters
     * @param start_pos Start position (inclusive)
     * @param end_pos End position (exclusive)
     * @return Result with deleted text or error
     */
    Result<std::string> deleteRange(size_t start_pos, size_t end_pos);

    /**
     * @brief Move gap to specified position
     * @param position New position for gap
     * @return Result indicating success or error
     */
    Result<void> moveGapTo(size_t position);

    /**
     * @brief Get character at position
     * @param position Position to query
     * @return Result with character or error
     */
    Result<char> getCharAt(size_t position) const;

    /**
     * @brief Get substring from buffer
     * @param start Start position
     * @param length Length of substring
     * @return Result with substring or error
     */
    Result<std::string> getSubstring(size_t start, size_t length) const;

    /**
     * @brief Get entire buffer content as string
     * @return Complete buffer content
     */
    std::string toString() const;

    /**
     * @brief Get buffer content as vector of lines
     * @return Vector of lines
     */
    std::vector<std::string> toLines() const;

    /**
     * @brief Get current logical size (excluding gap)
     * @return Number of characters in buffer
     */
    size_t size() const;

    /**
     * @brief Check if buffer is empty
     * @return true if buffer contains no characters
     */
    bool empty() const;

    /**
     * @brief Get current gap position
     * @return Position of gap in logical buffer
     */
    size_t getGapPosition() const;

    /**
     * @brief Get current gap size
     * @return Size of gap in characters
     */
    size_t getGapSize() const;

    /**
     * @brief Get physical buffer size
     * @return Total allocated buffer size
     */
    size_t getPhysicalSize() const;

    /**
     * @brief Get buffer utilization ratio
     * @return Ratio of used space to total space (0.0-1.0)
     */
    double getUtilization() const;

    /**
     * @brief Compact buffer to remove excess gap space
     * @return Result indicating success or error
     */
    Result<void> compact();

    /**
     * @brief Clear all buffer content
     */
    void clear();

    /**
     * @brief Get buffer statistics for debugging/monitoring
     */
    struct Statistics {
        size_t logical_size;
        size_t physical_size;
        size_t gap_size;
        size_t gap_position;
        double utilization;
        size_t insertions;
        size_t deletions;
        size_t gap_moves;
        std::chrono::milliseconds last_operation_time;
    };

    Statistics getStatistics() const;

private:
    Config config_;
    std::vector<char> buffer_;     // Physical buffer storage
    size_t gap_start_;             // Start of gap in physical buffer
    size_t gap_end_;               // End of gap in physical buffer (exclusive)
    
    // Performance statistics
    mutable size_t insertions_ = 0;
    mutable size_t deletions_ = 0;
    mutable size_t gap_moves_ = 0;
    mutable std::chrono::steady_clock::time_point last_operation_;

    /**
     * @brief Ensure gap has minimum required size
     * @param required_size Minimum size needed
     * @return Result indicating success or error
     */
    Result<void> ensureGapSize(size_t required_size);

    /**
     * @brief Grow buffer capacity
     * @param new_capacity New capacity to allocate
     * @return Result indicating success or error
     */
    Result<void> growBuffer(size_t new_capacity);

    /**
     * @brief Convert logical position to physical position
     * @param logical_pos Position in logical buffer
     * @return Physical position in buffer_
     */
    size_t logicalToPhysical(size_t logical_pos) const;

    /**
     * @brief Convert physical position to logical position
     * @param physical_pos Position in buffer_
     * @return Logical position
     */
    size_t physicalToLogical(size_t physical_pos) const;

    /**
     * @brief Validate position is within bounds
     * @param position Position to validate
     * @return Result indicating validity
     */
    Result<void> validatePosition(size_t position) const;

    /**
     * @brief Record operation timing for performance monitoring
     */
    void recordOperation() const;
};

/**
 * @brief Line-based editor buffer that uses GapBuffer internally
 * 
 * Provides line-oriented operations while maintaining the performance
 * benefits of the gap buffer for character-level operations.
 */
class EditorBuffer {
public:
    /**
     * @brief Configuration for editor buffer
     */
    struct Config {
        GapBuffer::Config gap_config;
        size_t max_line_length;
        size_t max_lines;
        bool track_line_endings;
        
        Config() 
            : max_line_length(10000)
            , max_lines(1000000)
            , track_line_endings(true) {}
    };

    explicit EditorBuffer(Config config = Config{});

    /**
     * @brief Initialize with content
     * @param content Initial content
     * @return Result indicating success or error
     */
    Result<void> initialize(const std::string& content);

    /**
     * @brief Get line count
     * @return Number of lines in buffer
     */
    size_t getLineCount() const;

    /**
     * @brief Get line content
     * @param line_index Line index (0-based)
     * @return Result with line content or error
     */
    Result<std::string> getLine(size_t line_index) const;

    /**
     * @brief Set line content
     * @param line_index Line index
     * @param content New line content
     * @return Result indicating success or error
     */
    Result<void> setLine(size_t line_index, const std::string& content);

    /**
     * @brief Insert line at position
     * @param line_index Position to insert at
     * @param content Line content
     * @return Result indicating success or error
     */
    Result<void> insertLine(size_t line_index, const std::string& content);

    /**
     * @brief Delete line
     * @param line_index Line to delete
     * @return Result with deleted line content or error
     */
    Result<std::string> deleteLine(size_t line_index);

    /**
     * @brief Insert character at position
     * @param line_index Line index
     * @param col_index Column index
     * @param ch Character to insert
     * @return Result indicating success or error
     */
    Result<void> insertChar(size_t line_index, size_t col_index, char ch);

    /**
     * @brief Delete character at position
     * @param line_index Line index
     * @param col_index Column index
     * @return Result with deleted character or error
     */
    Result<char> deleteChar(size_t line_index, size_t col_index);

    /**
     * @brief Split line at position
     * @param line_index Line to split
     * @param col_index Position to split at
     * @return Result indicating success or error
     */
    Result<void> splitLine(size_t line_index, size_t col_index);

    /**
     * @brief Join line with next line
     * @param line_index Line to join with next
     * @return Result indicating success or error
     */
    Result<void> joinLines(size_t line_index);

    /**
     * @brief Get buffer content as string
     * @return Complete buffer content
     */
    std::string toString() const;

    /**
     * @brief Get buffer content as vector of lines
     * @return Vector of lines
     */
    std::vector<std::string> toLines() const;

    /**
     * @brief Clear all content
     */
    void clear();

    /**
     * @brief Get buffer statistics
     */
    struct Statistics {
        GapBuffer::Statistics gap_stats;
        size_t line_count;
        size_t total_characters;
        size_t line_operations;
        size_t char_operations;
    };

    Statistics getStatistics() const;

private:
    Config config_;
    std::unique_ptr<GapBuffer> gap_buffer_;
    mutable std::vector<size_t> line_starts_;  // Cache of line start positions
    mutable bool line_cache_dirty_ = true;
    
    // Performance counters
    mutable size_t line_operations_ = 0;
    mutable size_t char_operations_ = 0;

    /**
     * @brief Rebuild line start cache if dirty
     */
    void rebuildLineCache() const;

    /**
     * @brief Convert line/column to buffer position
     * @param line_index Line index
     * @param col_index Column index  
     * @return Result with buffer position or error
     */
    Result<size_t> lineColToPosition(size_t line_index, size_t col_index) const;

    /**
     * @brief Convert buffer position to line/column
     * @param position Buffer position
     * @return Result with line/column pair or error
     */
    Result<std::pair<size_t, size_t>> positionToLineCol(size_t position) const;

    /**
     * @brief Mark line cache as dirty
     */
    void invalidateLineCache();
};

} // namespace nx::tui