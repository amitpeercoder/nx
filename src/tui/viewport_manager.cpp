#include "nx/tui/viewport_manager.hpp"
#include <algorithm>
#include <cmath>

namespace nx::tui {

ViewportManager::ViewportManager(ViewportConfig config)
    : config_(std::move(config))
    , last_scroll_time_(std::chrono::steady_clock::now()) {
}

void ViewportManager::updateConfig(const ViewportConfig& config) {
    config_ = config;
    updateViewportBounds();
}

Result<void> ViewportManager::setViewportSize(size_t lines, size_t columns) {
    if (lines == 0 || columns == 0) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Viewport size must be greater than zero");
    }
    
    viewport_.visible_lines = lines;
    viewport_.visible_columns = columns;
    
    // Update virtual scrolling if necessary
    updateVirtualScrolling();
    updateViewportBounds();
    clampViewport();
    
    return {};
}

Result<void> ViewportManager::setCursorPosition(size_t line, size_t column) {
    viewport_.cursor_line = line;
    viewport_.cursor_column = column;
    
    // Ensure cursor is within document bounds
    if (line >= document_lines_ && document_lines_ > 0) {
        viewport_.cursor_line = document_lines_ - 1;
    }
    if (column > max_line_length_) {
        viewport_.cursor_column = max_line_length_;
    }
    
    // Update viewport to ensure cursor visibility
    if (config_.scroll_mode == ScrollMode::SmartCenter || 
        config_.scroll_mode == ScrollMode::Minimal) {
        auto result = ensureCursorVisible();
        if (!result) {
            return result;
        }
    }
    
    return {};
}

Result<void> ViewportManager::setDocumentSize(size_t total_lines, size_t max_line_length) {
    document_lines_ = total_lines;
    max_line_length_ = max_line_length;
    
    // Enable virtual scrolling for large files
    if (config_.enable_virtual_scrolling && 
        total_lines > config_.large_file_threshold) {
        viewport_.is_virtual = true;
    }
    
    updateVirtualScrolling();
    clampViewport();
    
    return {};
}

Result<void> ViewportManager::scrollToLine(size_t line) {
    recordScrollOperation();
    
    switch (config_.scroll_mode) {
        case ScrollMode::Jump:
            viewport_.start_line = line;
            break;
            
        case ScrollMode::SmartCenter:
            if (viewport_.visible_lines > 0) {
                // Center the target line in viewport
                if (line >= viewport_.visible_lines / 2) {
                    viewport_.start_line = line - viewport_.visible_lines / 2;
                } else {
                    viewport_.start_line = 0;
                }
            }
            break;
            
        case ScrollMode::Smooth:
            // Implement smooth scrolling with gradual movement
            performSmoothScroll(line, viewport_.start_column);
            break;
            
        case ScrollMode::Minimal:
            // Minimal scrolling - only scroll if absolutely necessary
            viewport_.start_line = line;
            break;
    }
    
    clampViewport();
    updateViewportBounds();
    
    return {};
}

Result<void> ViewportManager::scrollToColumn(size_t column) {
    recordScrollOperation();
    
    switch (config_.scroll_mode) {
        case ScrollMode::Jump:
            viewport_.start_column = column;
            break;
            
        case ScrollMode::SmartCenter:
            if (viewport_.visible_columns > 0) {
                // Center the target column in viewport
                if (column >= viewport_.visible_columns / 2) {
                    viewport_.start_column = column - viewport_.visible_columns / 2;
                } else {
                    viewport_.start_column = 0;
                }
            }
            break;
            
        case ScrollMode::Smooth:
        case ScrollMode::Minimal:
            viewport_.start_column = column;
            break;
    }
    
    clampViewport();
    updateViewportBounds();
    
    return {};
}

Result<void> ViewportManager::scrollToPosition(size_t line, size_t column) {
    auto line_result = scrollToLine(line);
    if (!line_result) {
        return line_result;
    }
    
    return scrollToColumn(column);
}

Result<void> ViewportManager::scrollBy(int delta_lines, int delta_columns) {
    recordScrollOperation();
    
    // Apply line delta
    if (delta_lines != 0) {
        int new_start_line = static_cast<int>(viewport_.start_line) + delta_lines;
        viewport_.start_line = std::max(0, new_start_line);
    }
    
    // Apply column delta
    if (delta_columns != 0) {
        int new_start_column = static_cast<int>(viewport_.start_column) + delta_columns;
        viewport_.start_column = std::max(0, new_start_column);
    }
    
    clampViewport();
    updateViewportBounds();
    
    return {};
}

Result<void> ViewportManager::scrollByPages(int delta_pages) {
    size_t page_size = viewport_.visible_lines > 0 ? viewport_.visible_lines - 1 : 1;
    int delta_lines = delta_pages * static_cast<int>(page_size);
    
    return scrollBy(delta_lines, 0);
}

Result<void> ViewportManager::ensureCursorVisible() {
    if (isCursorVisible()) {
        return {}; // Already visible
    }
    
    recordScrollOperation();
    
    size_t target_line = viewport_.cursor_line;
    size_t target_column = viewport_.cursor_column;
    
    // Check if we need to scroll vertically
    if (needsScroll(target_line, target_column)) {
        performScroll(target_line, target_column);
    }
    
    return {};
}

Result<void> ViewportManager::centerCursor() {
    recordScrollOperation();
    
    // Center cursor in viewport
    if (viewport_.visible_lines > 0) {
        if (viewport_.cursor_line >= viewport_.visible_lines / 2) {
            viewport_.start_line = viewport_.cursor_line - viewport_.visible_lines / 2;
        } else {
            viewport_.start_line = 0;
        }
    }
    
    if (viewport_.visible_columns > 0) {
        if (viewport_.cursor_column >= viewport_.visible_columns / 2) {
            viewport_.start_column = viewport_.cursor_column - viewport_.visible_columns / 2;
        } else {
            viewport_.start_column = 0;
        }
    }
    
    clampViewport();
    updateViewportBounds();
    
    return {};
}

Result<void> ViewportManager::scrollToTop() {
    recordScrollOperation();
    
    viewport_.start_line = 0;
    viewport_.start_column = 0;
    updateViewportBounds();
    
    return {};
}

Result<void> ViewportManager::scrollToBottom() {
    recordScrollOperation();
    
    if (document_lines_ > viewport_.visible_lines) {
        viewport_.start_line = document_lines_ - viewport_.visible_lines;
    } else {
        viewport_.start_line = 0;
    }
    
    clampViewport();
    updateViewportBounds();
    
    return {};
}

bool ViewportManager::isCursorVisible() const {
    return isPositionVisible(viewport_.cursor_line, viewport_.cursor_column);
}

bool ViewportManager::isLineVisible(size_t line) const {
    return line >= viewport_.start_line && line < viewport_.end_line;
}

bool ViewportManager::isPositionVisible(size_t line, size_t column) const {
    return isLineVisible(line) && 
           column >= viewport_.start_column && 
           column < viewport_.end_column;
}

Result<void> ViewportManager::enableVirtualScrolling(bool enable) {
    if (enable && document_lines_ <= config_.large_file_threshold) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Document too small for virtual scrolling");
    }
    
    viewport_.is_virtual = enable;
    updateVirtualScrolling();
    
    return {};
}

ViewportStatistics ViewportManager::getStatistics() const {
    return stats_;
}

void ViewportManager::resetStatistics() {
    stats_ = ViewportStatistics{};
}

// Private helper methods

void ViewportManager::updateViewportBounds() {
    // Calculate end positions
    viewport_.end_line = viewport_.start_line + viewport_.visible_lines;
    viewport_.end_column = viewport_.start_column + viewport_.visible_columns;
    
    // Clamp to document bounds
    if (viewport_.end_line > document_lines_) {
        viewport_.end_line = document_lines_;
    }
    
    if (viewport_.end_column > max_line_length_) {
        viewport_.end_column = max_line_length_;
    }
}

void ViewportManager::clampViewport() {
    // Clamp start positions to valid ranges
    if (document_lines_ > viewport_.visible_lines) {
        viewport_.start_line = std::min(viewport_.start_line, 
                                      document_lines_ - viewport_.visible_lines);
    } else {
        viewport_.start_line = 0;
    }
    
    if (max_line_length_ > viewport_.visible_columns) {
        viewport_.start_column = std::min(viewport_.start_column, 
                                        max_line_length_ - viewport_.visible_columns);
    } else {
        viewport_.start_column = 0;
    }
    
    // Update bounds after clamping
    updateViewportBounds();
}

bool ViewportManager::needsScroll(size_t target_line, size_t target_column) const {
    // Check vertical scrolling need
    bool needs_vertical_scroll = false;
    
    if (target_line < viewport_.start_line + config_.top_margin) {
        needs_vertical_scroll = true;
    } else if (target_line >= viewport_.end_line - config_.bottom_margin) {
        needs_vertical_scroll = true;
    }
    
    // Check horizontal scrolling need
    bool needs_horizontal_scroll = false;
    
    if (target_column < viewport_.start_column + config_.left_margin) {
        needs_horizontal_scroll = true;
    } else if (target_column >= viewport_.end_column - config_.right_margin) {
        needs_horizontal_scroll = true;
    }
    
    return needs_vertical_scroll || needs_horizontal_scroll;
}

void ViewportManager::performScroll(size_t target_line, size_t target_column) {
    switch (config_.scroll_mode) {
        case ScrollMode::Jump:
            viewport_.start_line = target_line;
            viewport_.start_column = target_column;
            break;
            
        case ScrollMode::SmartCenter:
            // Center the target position
            if (viewport_.visible_lines > 0 && target_line >= viewport_.visible_lines / 2) {
                viewport_.start_line = target_line - viewport_.visible_lines / 2;
            } else {
                viewport_.start_line = 0;
            }
            
            if (viewport_.visible_columns > 0 && target_column >= viewport_.visible_columns / 2) {
                viewport_.start_column = target_column - viewport_.visible_columns / 2;
            } else {
                viewport_.start_column = 0;
            }
            break;
            
        case ScrollMode::Minimal:
            // Scroll only the minimum amount needed
            if (target_line < viewport_.start_line + config_.top_margin) {
                viewport_.start_line = target_line >= config_.top_margin ? 
                    target_line - config_.top_margin : 0;
            } else if (target_line >= viewport_.end_line - config_.bottom_margin) {
                viewport_.start_line = target_line + config_.bottom_margin + 1 - viewport_.visible_lines;
            }
            
            if (target_column < viewport_.start_column + config_.left_margin) {
                viewport_.start_column = target_column >= config_.left_margin ? 
                    target_column - config_.left_margin : 0;
            } else if (target_column >= viewport_.end_column - config_.right_margin) {
                viewport_.start_column = target_column + config_.right_margin + 1 - viewport_.visible_columns;
            }
            break;
            
        case ScrollMode::Smooth:
            // Implement smooth scrolling animation
            performSmoothScroll(target_line, target_column);
            break;
    }
    
    clampViewport();
    updateViewportBounds();
}

void ViewportManager::updateVirtualScrolling() {
    if (!viewport_.is_virtual || !config_.enable_virtual_scrolling) {
        viewport_.virtual_start = 0;
        viewport_.virtual_end = document_lines_;
        return;
    }
    
    // Calculate virtual window around current viewport
    size_t virtual_margin = config_.virtual_page_size;
    
    if (viewport_.start_line >= virtual_margin) {
        viewport_.virtual_start = viewport_.start_line - virtual_margin;
    } else {
        viewport_.virtual_start = 0;
    }
    
    viewport_.virtual_end = std::min(document_lines_, 
        viewport_.end_line + virtual_margin);
    
    // Ensure we don't exceed max rendered lines
    if (viewport_.virtual_end - viewport_.virtual_start > config_.max_rendered_lines) {
        viewport_.virtual_end = viewport_.virtual_start + config_.max_rendered_lines;
    }
    
    stats_.virtual_page_loads++;
}

void ViewportManager::recordScrollOperation() {
    stats_.scroll_operations++;
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_scroll_time_);
    
    // Update average scroll time (simple moving average)
    if (stats_.scroll_operations > 1) {
        auto total_time = stats_.avg_scroll_time.count() * (stats_.scroll_operations - 1) + 
                         duration.count();
        stats_.avg_scroll_time = std::chrono::milliseconds(total_time / stats_.scroll_operations);
    } else {
        stats_.avg_scroll_time = duration;
    }
    
    last_scroll_time_ = now;
}

void ViewportManager::performSmoothScroll(size_t target_line, size_t target_column) {
    // For CLI applications, "smooth scrolling" means intelligently interpolating 
    // the scroll position to reduce jarring jumps for large distances
    
    size_t current_line = viewport_.start_line;
    size_t current_column = viewport_.start_column;
    
    // Calculate the distance to scroll
    int line_delta = static_cast<int>(target_line) - static_cast<int>(current_line);
    int column_delta = static_cast<int>(target_column) - static_cast<int>(current_column);
    
    // For large distances, use exponential easing to reduce jarring jumps
    // For distances larger than one screen, scroll in multiple steps
    if (std::abs(line_delta) > static_cast<int>(viewport_.visible_lines) || 
        std::abs(column_delta) > static_cast<int>(viewport_.visible_columns)) {
        
        // Calculate intermediate position (exponential easing)
        double easing_factor = 0.6; // Scroll 60% of the distance
        int intermediate_line_delta = static_cast<int>(line_delta * easing_factor);
        int intermediate_column_delta = static_cast<int>(column_delta * easing_factor);
        
        viewport_.start_line = current_line + intermediate_line_delta;
        viewport_.start_column = current_column + intermediate_column_delta;
    } else {
        // For small distances, scroll directly but with slight easing
        double easing_factor = 0.8; // Scroll 80% of the distance
        int eased_line_delta = static_cast<int>(line_delta * easing_factor);
        int eased_column_delta = static_cast<int>(column_delta * easing_factor);
        
        if (std::abs(eased_line_delta) < 1 && std::abs(eased_column_delta) < 1) {
            // Very small movement - just snap to target
            viewport_.start_line = target_line;
            viewport_.start_column = target_column;
        } else {
            viewport_.start_line = current_line + eased_line_delta;
            viewport_.start_column = current_column + eased_column_delta;
        }
    }
    
    // Ensure we don't go past the target in the wrong direction
    if ((line_delta > 0 && viewport_.start_line > target_line) ||
        (line_delta < 0 && viewport_.start_line < target_line)) {
        viewport_.start_line = target_line;
    }
    
    if ((column_delta > 0 && viewport_.start_column > target_column) ||
        (column_delta < 0 && viewport_.start_column < target_column)) {
        viewport_.start_column = target_column;
    }
}

// ViewportManagerFactory implementation

std::unique_ptr<ViewportManager> ViewportManagerFactory::createForEditor() {
    ViewportConfig config;
    config.scroll_mode = ScrollMode::SmartCenter;
    config.top_margin = 3;
    config.bottom_margin = 3;
    config.left_margin = 5;
    config.right_margin = 10;
    config.enable_virtual_scrolling = true;
    config.large_file_threshold = 1000;
    
    return std::make_unique<ViewportManager>(config);
}

std::unique_ptr<ViewportManager> ViewportManagerFactory::createForPreview() {
    ViewportConfig config;
    config.scroll_mode = ScrollMode::Jump;
    config.top_margin = 1;
    config.bottom_margin = 1;
    config.left_margin = 2;
    config.right_margin = 2;
    config.enable_virtual_scrolling = true;
    config.large_file_threshold = 5000;
    
    return std::make_unique<ViewportManager>(config);
}

std::unique_ptr<ViewportManager> ViewportManagerFactory::createForLargeFiles() {
    ViewportConfig config;
    config.scroll_mode = ScrollMode::Minimal;
    config.top_margin = 5;
    config.bottom_margin = 5;
    config.left_margin = 10;
    config.right_margin = 20;
    config.enable_virtual_scrolling = true;
    config.virtual_page_size = 200;
    config.max_rendered_lines = 500;
    config.large_file_threshold = 10000;
    
    return std::make_unique<ViewportManager>(config);
}

std::unique_ptr<ViewportManager> ViewportManagerFactory::createMinimal() {
    ViewportConfig config;
    config.scroll_mode = ScrollMode::Jump;
    config.top_margin = 0;
    config.bottom_margin = 0;
    config.left_margin = 0;
    config.right_margin = 0;
    config.enable_virtual_scrolling = false;
    
    return std::make_unique<ViewportManager>(config);
}

} // namespace nx::tui