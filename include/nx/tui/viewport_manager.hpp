#pragma once

#include <cstddef>
#include <chrono>
#include <memory>
#include "nx/common.hpp"

namespace nx::tui {

/**
 * @brief Scrolling behavior configuration
 */
enum class ScrollMode {
    Jump,           // Instant scrolling
    Smooth,         // Smooth animated scrolling
    SmartCenter,    // Centers cursor when scrolling becomes necessary
    Minimal         // Scrolls only the minimum amount needed
};

/**
 * @brief Viewport configuration
 */
struct ViewportConfig {
    size_t top_margin = 3;          // Lines to keep above cursor
    size_t bottom_margin = 3;       // Lines to keep below cursor
    size_t left_margin = 5;         // Columns to keep left of cursor
    size_t right_margin = 10;       // Columns to keep right of cursor
    ScrollMode scroll_mode = ScrollMode::SmartCenter;
    
    // Virtual scrolling config
    bool enable_virtual_scrolling = true;
    size_t virtual_page_size = 100;  // Lines per virtual page
    size_t max_rendered_lines = 1000; // Maximum lines to render at once
    
    // Performance tuning
    std::chrono::milliseconds scroll_animation_duration{150};
    size_t large_file_threshold = 10000; // Lines
};

/**
 * @brief Represents a viewport into a document
 */
struct Viewport {
    size_t start_line = 0;           // First visible line
    size_t end_line = 0;             // Last visible line (exclusive)
    size_t start_column = 0;         // First visible column
    size_t end_column = 0;           // Last visible column (exclusive)
    size_t visible_lines = 0;        // Number of lines that can be displayed
    size_t visible_columns = 0;      // Number of columns that can be displayed
    
    // Cursor position within viewport
    size_t cursor_line = 0;          // Absolute cursor line
    size_t cursor_column = 0;        // Absolute cursor column
    
    // Virtual scrolling state
    size_t virtual_start = 0;        // Virtual start for large files
    size_t virtual_end = 0;          // Virtual end for large files
    bool is_virtual = false;         // Whether virtual scrolling is active
};

/**
 * @brief Statistics for viewport performance monitoring
 */
struct ViewportStatistics {
    size_t scroll_operations = 0;
    size_t virtual_page_loads = 0;
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    std::chrono::milliseconds avg_scroll_time{0};
    size_t memory_usage = 0;
};

/**
 * @brief Manages viewport and scrolling for editor content
 * 
 * This class provides enhanced scrolling capabilities including:
 * - Smart viewport management with configurable margins
 * - Virtual scrolling for large files
 * - Multiple scrolling modes (jump, smooth, smart-center)
 * - Performance optimization and caching
 */
class ViewportManager {
public:
    explicit ViewportManager(ViewportConfig config = {});
    ~ViewportManager() = default;

    // Configuration
    void updateConfig(const ViewportConfig& config);
    const ViewportConfig& getConfig() const { return config_; }

    // Viewport management
    Result<void> setViewportSize(size_t lines, size_t columns);
    Result<void> setCursorPosition(size_t line, size_t column);
    Result<void> setDocumentSize(size_t total_lines, size_t max_line_length);
    
    // Scrolling operations
    Result<void> scrollToLine(size_t line);
    Result<void> scrollToColumn(size_t column);
    Result<void> scrollToPosition(size_t line, size_t column);
    Result<void> scrollBy(int delta_lines, int delta_columns = 0);
    Result<void> scrollByPages(int delta_pages);
    
    // Smart scrolling
    Result<void> ensureCursorVisible();
    Result<void> centerCursor();
    Result<void> scrollToTop();
    Result<void> scrollToBottom();
    
    // Viewport access
    const Viewport& getViewport() const { return viewport_; }
    bool isCursorVisible() const;
    bool isLineVisible(size_t line) const;
    bool isPositionVisible(size_t line, size_t column) const;
    
    // Virtual scrolling
    Result<void> enableVirtualScrolling(bool enable);
    bool isVirtualScrollingActive() const { return viewport_.is_virtual; }
    size_t getVirtualPageSize() const { return config_.virtual_page_size; }
    
    // Performance monitoring
    ViewportStatistics getStatistics() const;
    void resetStatistics();

private:
    ViewportConfig config_;
    Viewport viewport_;
    size_t document_lines_ = 0;
    size_t max_line_length_ = 0;
    
    // Performance tracking
    mutable ViewportStatistics stats_;
    std::chrono::steady_clock::time_point last_scroll_time_;
    
    // Helper methods
    void updateViewportBounds();
    void clampViewport();
    bool needsScroll(size_t target_line, size_t target_column) const;
    void performScroll(size_t target_line, size_t target_column);
    void performSmoothScroll(size_t target_line, size_t target_column);
    void updateVirtualScrolling();
    void recordScrollOperation();
};

/**
 * @brief Factory for creating viewport managers with common configurations
 */
class ViewportManagerFactory {
public:
    static std::unique_ptr<ViewportManager> createForEditor();
    static std::unique_ptr<ViewportManager> createForPreview();
    static std::unique_ptr<ViewportManager> createForLargeFiles();
    static std::unique_ptr<ViewportManager> createMinimal();
};

} // namespace nx::tui