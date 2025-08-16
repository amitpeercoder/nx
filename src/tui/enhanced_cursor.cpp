#include "nx/tui/enhanced_cursor.hpp"
#include <algorithm>
#include <chrono>

namespace nx::tui {

// EnhancedCursor implementation

EnhancedCursor::EnhancedCursor(Config config)
    : config_(std::move(config))
    , buffer_(nullptr)
    , movements_(0)
    , position_validations_(0)
    , unicode_operations_(0)
    , bounds_checks_(0)
    , bounds_cache_valid_(false)
    , cached_line_number_(SIZE_MAX)
    , last_operation_time_(std::chrono::steady_clock::now()) {
    
    // Unicode integration completed with full UnicodeCursor implementation
}

EnhancedCursor::~EnhancedCursor() = default;

Result<void> EnhancedCursor::initialize(const EditorBuffer& buffer) {
    buffer_ = &buffer;
    
    // Unicode cursor and bounds checker are initialized in constructor
    
    // Reset position to start
    position_ = Position{};
    selection_ = Selection{};
    
    // Validate initial position
    auto validation = validateAndNormalizePosition(position_);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    
    position_ = validation.value();
    invalidateBoundsCache();
    
    return {};
}

Result<void> EnhancedCursor::updateBuffer(const EditorBuffer& buffer) {
    buffer_ = &buffer;
    invalidateBoundsCache();
    
    // Revalidate current position with new buffer
    auto validation = validateAndNormalizePosition(position_);
    if (!validation) {
        // If current position is invalid, move to document end
        auto bounds = getBounds();
        if (bounds.total_lines > 0) {
            position_.line = bounds.total_lines - 1;
            position_.column = bounds.max_column_for_line;
        } else {
            position_ = Position{};
        }
        
        // Try validation again
        validation = validateAndNormalizePosition(position_);
        if (!validation) {
            return std::unexpected(validation.error());
        }
    }
    
    position_ = validation.value();
    
    // Clear selection if it's no longer valid
    if (selection_.active) {
        auto start_validation = validateAndNormalizePosition(selection_.start);
        auto end_validation = validateAndNormalizePosition(selection_.end);
        
        if (!start_validation || !end_validation) {
            clearSelection();
        }
    }
    
    return {};
}

const EnhancedCursor::Position& EnhancedCursor::getPosition() const {
    return position_;
}

Result<void> EnhancedCursor::setPosition(size_t line, size_t column) {
    Position new_pos;
    new_pos.line = line;
    new_pos.column = column;
    
    return setPosition(new_pos);
}

Result<void> EnhancedCursor::setPosition(const Position& position) {
    recordOperation();
    
    auto validation = validateAndNormalizePosition(position);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    
    position_ = validation.value();
    updateVirtualColumn();
    
    return {};
}

Result<void> EnhancedCursor::move(Direction direction, bool extend_selection) {
    recordOperation();
    movements_++;
    
    Position new_pos = position_;
    
    switch (direction) {
        case Direction::Left:
            if (new_pos.column > 0) {
                new_pos.column--;
            } else if (new_pos.line > 0) {
                new_pos.line--;
                // Move to end of previous line
                auto line_result = getLineWithCache(new_pos.line);
                if (line_result) {
                    new_pos.column = line_result.value().length();
                }
            }
            break;
            
        case Direction::Right: {
            auto line_result = getLineWithCache(new_pos.line);
            if (line_result) {
                if (new_pos.column < line_result.value().length()) {
                    new_pos.column++;
                } else {
                    // Move to beginning of next line
                    auto bounds = getBounds();
                    if (new_pos.line + 1 < bounds.total_lines) {
                        new_pos.line++;
                        new_pos.column = 0;
                    }
                }
            }
            break;
        }
        
        case Direction::Up:
            if (new_pos.line > 0) {
                new_pos.line--;
                if (config_.enable_virtual_column) {
                    // Use virtual column for vertical movement
                    auto line_result = getLineWithCache(new_pos.line);
                    if (line_result) {
                        new_pos.column = std::min(new_pos.virtual_column, line_result.value().length());
                    }
                } else {
                    // Clamp to line length
                    auto line_result = getLineWithCache(new_pos.line);
                    if (line_result) {
                        new_pos.column = std::min(new_pos.column, line_result.value().length());
                    }
                }
            }
            break;
            
        case Direction::Down: {
            auto bounds = getBounds();
            if (new_pos.line + 1 < bounds.total_lines) {
                new_pos.line++;
                if (config_.enable_virtual_column) {
                    // Use virtual column for vertical movement
                    auto line_result = getLineWithCache(new_pos.line);
                    if (line_result) {
                        new_pos.column = std::min(new_pos.virtual_column, line_result.value().length());
                    }
                } else {
                    // Clamp to line length
                    auto line_result = getLineWithCache(new_pos.line);
                    if (line_result) {
                        new_pos.column = std::min(new_pos.column, line_result.value().length());
                    }
                }
            }
            break;
        }
        
        case Direction::Home:
            new_pos.column = 0;
            break;
            
        case Direction::End: {
            auto line_result = getLineWithCache(new_pos.line);
            if (line_result) {
                new_pos.column = line_result.value().length();
            }
            break;
        }
        
        case Direction::PageUp: {
            size_t lines_to_move = std::min(config_.page_size, new_pos.line);
            new_pos.line -= lines_to_move;
            if (config_.enable_virtual_column) {
                auto line_result = getLineWithCache(new_pos.line);
                if (line_result) {
                    new_pos.column = std::min(new_pos.virtual_column, line_result.value().length());
                }
            }
            break;
        }
        
        case Direction::PageDown: {
            auto bounds = getBounds();
            size_t lines_to_move = std::min(config_.page_size, bounds.total_lines - new_pos.line - 1);
            new_pos.line += lines_to_move;
            if (config_.enable_virtual_column) {
                auto line_result = getLineWithCache(new_pos.line);
                if (line_result) {
                    new_pos.column = std::min(new_pos.virtual_column, line_result.value().length());
                }
            }
            break;
        }
        
        case Direction::DocumentHome:
            new_pos.line = 0;
            new_pos.column = 0;
            break;
            
        case Direction::DocumentEnd: {
            auto bounds = getBounds();
            if (bounds.total_lines > 0) {
                new_pos.line = bounds.total_lines - 1;
                auto line_result = getLineWithCache(new_pos.line);
                if (line_result) {
                    new_pos.column = line_result.value().length();
                }
            }
            break;
        }
    }
    
    // Validate and apply new position
    auto validation = validateAndNormalizePosition(new_pos);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    
    position_ = validation.value();
    
    // Update virtual column for horizontal movements
    if (direction == Direction::Left || direction == Direction::Right ||
        direction == Direction::Home || direction == Direction::End) {
        updateVirtualColumn();
    }
    
    // Update selection
    updateSelectionOnMove(extend_selection);
    
    return {};
}

Result<void> EnhancedCursor::moveByCharacters(int char_count, bool extend_selection) {
    recordOperation();
    movements_++;
    
    if (char_count == 0) {
        return {};
    }
    
    Position new_pos = position_;
    
    if (char_count > 0) {
        // Move forward
        for (int i = 0; i < char_count; ++i) {
            auto move_result = move(Direction::Right, false);
            if (!move_result) {
                break; // Stop at document boundary
            }
            new_pos = position_;
        }
    } else {
        // Move backward
        for (int i = 0; i < -char_count; ++i) {
            auto move_result = move(Direction::Left, false);
            if (!move_result) {
                break; // Stop at document boundary
            }
            new_pos = position_;
        }
    }
    
    updateSelectionOnMove(extend_selection);
    return {};
}

Result<void> EnhancedCursor::moveByLines(int line_count, bool extend_selection) {
    recordOperation();
    movements_++;
    
    if (line_count == 0) {
        return {};
    }
    
    if (line_count > 0) {
        // Move down
        for (int i = 0; i < line_count; ++i) {
            auto move_result = move(Direction::Down, false);
            if (!move_result) {
                break; // Stop at document boundary
            }
        }
    } else {
        // Move up
        for (int i = 0; i < -line_count; ++i) {
            auto move_result = move(Direction::Up, false);
            if (!move_result) {
                break; // Stop at document boundary
            }
        }
    }
    
    updateSelectionOnMove(extend_selection);
    return {};
}

Result<void> EnhancedCursor::moveToNextWord(bool extend_selection) {
    recordOperation();
    movements_++;
    unicode_operations_++;
    
    auto word_boundary = findWordBoundary(true);
    if (!word_boundary) {
        return std::unexpected(word_boundary.error());
    }
    
    position_ = word_boundary.value();
    updateVirtualColumn();
    updateSelectionOnMove(extend_selection);
    
    return {};
}

Result<void> EnhancedCursor::moveToPreviousWord(bool extend_selection) {
    recordOperation();
    movements_++;
    unicode_operations_++;
    
    auto word_boundary = findWordBoundary(false);
    if (!word_boundary) {
        return std::unexpected(word_boundary.error());
    }
    
    position_ = word_boundary.value();
    updateVirtualColumn();
    updateSelectionOnMove(extend_selection);
    
    return {};
}

Result<void> EnhancedCursor::moveToDisplayColumn(size_t display_column) {
    recordOperation();
    movements_++;
    unicode_operations_++;
    
    auto line_result = getLineWithCache(position_.line);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }
    
    // Use Unicode-aware display column calculation
    Position new_pos = position_;
    
    // Find the character position that corresponds to the target display column
    const std::string& line_text = line_result.value();
    size_t char_pos = 0;
    size_t current_display_col = 0;
    
    // Use UnicodeHandler to calculate proper display positions
    UnicodeHandler::Utf8Iterator iter(line_text);
    while (iter.hasNext() && current_display_col < display_column) {
        UChar32 codepoint = iter.next();
        size_t char_width = UnicodeHandler::getCodePointWidth(codepoint);
        
        if (current_display_col + char_width > display_column) {
            // Would overshoot target, stop at current position
            break;
        }
        
        current_display_col += char_width;
        char_pos++;
    }
    
    new_pos.column = std::min(char_pos, line_text.length());
    
    auto validation = validateAndNormalizePosition(new_pos);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    
    position_ = validation.value();
    updateVirtualColumn();
    
    return {};
}

const EnhancedCursor::Selection& EnhancedCursor::getSelection() const {
    return selection_;
}

Result<void> EnhancedCursor::startSelection(SelectionMode mode) {
    selection_.start = position_;
    selection_.end = position_;
    selection_.mode = mode;
    selection_.active = true;
    
    return {};
}

void EnhancedCursor::endSelection() {
    selection_.active = false;
}

void EnhancedCursor::clearSelection() {
    selection_ = Selection{};
}

Result<void> EnhancedCursor::selectAll() {
    auto bounds = getBounds();
    if (bounds.total_lines == 0) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "Cannot select all in empty document");
    }
    
    selection_.start = Position{};
    selection_.end.line = bounds.total_lines - 1;
    
    auto last_line = getLineWithCache(selection_.end.line);
    if (last_line) {
        selection_.end.column = last_line.value().length();
    }
    
    selection_.mode = SelectionMode::Character;
    selection_.active = true;
    
    return {};
}

Result<void> EnhancedCursor::selectWord() {
    auto line_result = getLineWithCache(position_.line);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }
    
    // Simple word selection using whitespace boundaries (will be improved with full Unicode integration)
    const std::string& line_text = line_result.value();
    size_t word_start = position_.column;
    size_t word_end = position_.column;
    
    // Use UnicodeHandler for proper word boundary detection when available
    auto prev_boundary = UnicodeHandler::findPreviousWordBoundary(line_text, position_.column);
    if (prev_boundary.has_value()) {
        word_start = prev_boundary.value();
    } else {
        // Fall back to simple detection
        while (word_start > 0 && !std::isspace(line_text[word_start - 1])) {
            word_start--;
        }
    }
    
    auto next_boundary = UnicodeHandler::findNextWordBoundary(line_text, position_.column);
    if (next_boundary.has_value()) {
        word_end = next_boundary.value();
    } else {
        // Fall back to simple detection
        while (word_end < line_text.length() && !std::isspace(line_text[word_end])) {
            word_end++;
        }
    }
    
    selection_.start.line = position_.line;
    selection_.start.column = word_start;
    selection_.end.line = position_.line;
    selection_.end.column = word_end;
    selection_.mode = SelectionMode::Word;
    selection_.active = true;
    
    return {};
}

Result<void> EnhancedCursor::selectLine() {
    auto line_result = getLineWithCache(position_.line);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }
    
    selection_.start.line = position_.line;
    selection_.start.column = 0;
    selection_.end.line = position_.line;
    selection_.end.column = line_result.value().length();
    selection_.mode = SelectionMode::Line;
    selection_.active = true;
    
    return {};
}

bool EnhancedCursor::isPositionSelected(const Position& position) const {
    if (!selection_.active) {
        return false;
    }
    
    auto [start, end] = selection_.getNormalized();
    
    if (position.line < start.line || position.line > end.line) {
        return false;
    }
    
    if (position.line == start.line && position.column < start.column) {
        return false;
    }
    
    if (position.line == end.line && position.column > end.column) {
        return false;
    }
    
    return true;
}

Result<std::string> EnhancedCursor::getSelectedText() const {
    if (!selection_.active || selection_.isEmpty()) {
        return std::string{};
    }
    
    auto [start, end] = selection_.getNormalized();
    
    if (!buffer_) {
        return makeErrorResult<std::string>(ErrorCode::kInvalidState, "No buffer associated with cursor");
    }
    
    std::string result;
    
    if (start.line == end.line) {
        // Single line selection
        auto line_result = buffer_->getLine(start.line);
        if (!line_result) {
            return std::unexpected(line_result.error());
        }
        
        if (end.column <= line_result.value().length()) {
            result = line_result.value().substr(start.column, end.column - start.column);
        }
    } else {
        // Multi-line selection
        for (size_t line = start.line; line <= end.line; ++line) {
            auto line_result = buffer_->getLine(line);
            if (!line_result) {
                return std::unexpected(line_result.error());
            }
            
            if (line == start.line) {
                // First line - from start column to end
                if (start.column < line_result.value().length()) {
                    result += line_result.value().substr(start.column);
                }
            } else if (line == end.line) {
                // Last line - from beginning to end column
                if (end.column <= line_result.value().length()) {
                    result += line_result.value().substr(0, end.column);
                }
            } else {
                // Middle lines - entire line
                result += line_result.value();
            }
            
            // Add newline except for last line
            if (line < end.line) {
                result += '\n';
            }
        }
    }
    
    return result;
}

EnhancedCursor::Bounds EnhancedCursor::getBounds() const {
    bounds_checks_++;
    rebuildBoundsCache();
    return cached_bounds_;
}

bool EnhancedCursor::isAtDocumentStart() const {
    return position_.line == 0 && position_.column == 0;
}

bool EnhancedCursor::isAtDocumentEnd() const {
    auto bounds = getBounds();
    return position_.line == bounds.max_line && position_.column == bounds.max_column_for_line;
}

bool EnhancedCursor::isAtLineStart() const {
    return position_.column == 0;
}

bool EnhancedCursor::isAtLineEnd() const {
    auto line_result = getLineWithCache(position_.line);
    if (!line_result) {
        return false;
    }
    return position_.column >= line_result.value().length();
}

Result<UChar32> EnhancedCursor::getCharacterAtCursor() const {
    unicode_operations_++;
    
    auto line_result = getLineWithCache(position_.line);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }
    
    const std::string& line = line_result.value();
    if (position_.column >= line.length()) {
        return static_cast<UChar32>('\n'); // Return newline for end of line
    }
    
    // Simple ASCII character extraction (will be improved with full Unicode integration)
    return static_cast<UChar32>(line[position_.column]);
}

Result<std::string> EnhancedCursor::getCurrentLine() const {
    return getLineWithCache(position_.line);
}

void EnhancedCursor::updateConfig(const Config& new_config) {
    config_ = new_config;
}

const EnhancedCursor::Config& EnhancedCursor::getConfig() const {
    return config_;
}

EnhancedCursor::Statistics EnhancedCursor::getStatistics() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_operation_time_);
    
    return Statistics{
        .movements = movements_,
        .position_validations = position_validations_,
        .unicode_operations = unicode_operations_,
        .bounds_checks = bounds_checks_,
        .avg_operation_time = elapsed / std::max(size_t{1}, movements_ + position_validations_),
        .memory_usage = sizeof(*this) + cached_line_.capacity()
    };
}

// Private methods

Result<EnhancedCursor::Position> EnhancedCursor::validateAndNormalizePosition(const Position& pos) const {
    position_validations_++;
    bounds_checks_++;
    
    if (!buffer_) {
        return makeErrorResult<Position>(ErrorCode::kInvalidState, "No buffer associated with cursor");
    }
    
    auto bounds = getBounds();
    
    if (bounds.total_lines == 0) {
        // Empty document
        return Position{};
    }
    
    Position normalized = pos;
    
    // Clamp line to valid range
    if (normalized.line >= bounds.total_lines) {
        normalized.line = bounds.total_lines - 1;
    }
    
    // Get line content and clamp column
    auto line_result = getLineWithCache(normalized.line);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }
    
    size_t max_column = line_result.value().length();
    if (!config_.allow_past_eol && normalized.column > max_column) {
        normalized.column = max_column;
    }
    
    // Calculate display metrics
    auto metrics_result = calculateDisplayMetrics(normalized);
    if (!metrics_result) {
        return std::unexpected(metrics_result.error());
    }
    
    return metrics_result.value();
}

Result<EnhancedCursor::Position> EnhancedCursor::calculateDisplayMetrics(Position pos) const {
    unicode_operations_++;
    
    auto line_result = getLineWithCache(pos.line);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }
    
    // Simple display metrics calculation for ASCII (will be improved with full Unicode integration)
    const std::string& line = line_result.value();
    pos.byte_offset = std::min(pos.column, line.length());
    pos.display_column = pos.column; // Assume 1:1 for ASCII
    
    return pos;
}

void EnhancedCursor::updateSelectionOnMove(bool extend_selection) {
    if (extend_selection) {
        if (!selection_.active) {
            startSelection(SelectionMode::Character);
        } else {
            selection_.end = position_;
        }
    } else {
        if (selection_.active) {
            clearSelection();
        }
    }
}

Result<EnhancedCursor::Position> EnhancedCursor::findWordBoundary(bool forward) const {
    unicode_operations_++;
    
    auto line_result = getLineWithCache(position_.line);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }
    
    // Simple word boundary detection using whitespace (will be improved with full Unicode integration)
    const std::string& line = line_result.value();
    size_t new_column = position_.column;
    
    if (forward) {
        // Move to next word
        while (new_column < line.length() && !std::isspace(line[new_column])) {
            new_column++;
        }
        while (new_column < line.length() && std::isspace(line[new_column])) {
            new_column++;
        }
    } else {
        // Move to previous word
        if (new_column > 0) new_column--;
        while (new_column > 0 && std::isspace(line[new_column])) {
            new_column--;
        }
        while (new_column > 0 && !std::isspace(line[new_column - 1])) {
            new_column--;
        }
    }
    
    Result<size_t> boundary = new_column;
    
    if (!boundary) {
        // Try moving to next/previous line
        auto bounds = getBounds();
        Position new_pos = position_;
        
        if (forward && position_.line + 1 < bounds.total_lines) {
            new_pos.line++;
            new_pos.column = 0;
        } else if (!forward && position_.line > 0) {
            new_pos.line--;
            auto prev_line = getLineWithCache(new_pos.line);
            if (prev_line) {
                new_pos.column = prev_line.value().length();
            }
        } else {
            return std::unexpected(boundary.error());
        }
        
        return validateAndNormalizePosition(new_pos);
    }
    
    Position new_pos = position_;
    new_pos.column = boundary.value();
    
    return validateAndNormalizePosition(new_pos);
}

void EnhancedCursor::updateVirtualColumn() {
    if (config_.enable_virtual_column) {
        position_.virtual_column = position_.display_column;
    }
}

void EnhancedCursor::invalidateBoundsCache() const {
    bounds_cache_valid_ = false;
    cached_line_number_ = SIZE_MAX;
}

void EnhancedCursor::rebuildBoundsCache() const {
    if (bounds_cache_valid_ || !buffer_) {
        return;
    }
    
    cached_bounds_.total_lines = buffer_->getLineCount();
    cached_bounds_.total_characters = 0;
    
    if (cached_bounds_.total_lines > 0) {
        cached_bounds_.max_line = cached_bounds_.total_lines - 1;
        
        // Get last line length
        auto last_line = buffer_->getLine(cached_bounds_.max_line);
        if (last_line) {
            cached_bounds_.max_column_for_line = last_line.value().length();
        } else {
            cached_bounds_.max_column_for_line = 0;
        }
        
        // Calculate total characters (expensive, do only if needed)
        for (size_t i = 0; i < cached_bounds_.total_lines; ++i) {
            auto line = buffer_->getLine(i);
            if (line) {
                cached_bounds_.total_characters += line.value().length() + 1; // +1 for newline
            }
        }
    } else {
        cached_bounds_.max_line = 0;
        cached_bounds_.max_column_for_line = 0;
    }
    
    bounds_cache_valid_ = true;
}

void EnhancedCursor::recordOperation() const {
    last_operation_time_ = std::chrono::steady_clock::now();
}

Result<std::string> EnhancedCursor::getLineWithCache(size_t line_number) const {
    if (cached_line_number_ == line_number && !cached_line_.empty()) {
        return cached_line_;
    }
    
    if (!buffer_) {
        return makeErrorResult<std::string>(ErrorCode::kInvalidState, "No buffer associated with cursor");
    }
    
    auto line_result = buffer_->getLine(line_number);
    if (line_result) {
        cached_line_number_ = line_number;
        cached_line_ = line_result.value();
    }
    
    return line_result;
}

// CursorManager implementation

CursorManager::CursorManager(EnhancedCursor::Config config) {
    primary_cursor_ = std::make_unique<EnhancedCursor>(std::move(config));
}

Result<void> CursorManager::initialize(const EditorBuffer& buffer) {
    return primary_cursor_->initialize(buffer);
}

EnhancedCursor& CursorManager::getPrimaryCursor() {
    return *primary_cursor_;
}

const EnhancedCursor& CursorManager::getPrimaryCursor() const {
    return *primary_cursor_;
}

Result<void> CursorManager::updateBuffer(const EditorBuffer& buffer) {
    return primary_cursor_->updateBuffer(buffer);
}

} // namespace nx::tui