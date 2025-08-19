#include "nx/tui/editor_buffer.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace nx::tui {

// GapBuffer Implementation

GapBuffer::GapBuffer(Config config) 
    : config_(std::move(config)), gap_start_(0), gap_end_(config_.initial_gap_size) {
    buffer_.resize(config_.initial_gap_size);
    recordOperation();
}

GapBuffer::~GapBuffer() {
    // Secure clear of buffer if it contained sensitive data
    if (!buffer_.empty()) {
        nx::util::Security::secureZero(buffer_.data(), buffer_.size());
    }
}

GapBuffer::GapBuffer(GapBuffer&& other) noexcept 
    : config_(std::move(other.config_)),
      buffer_(std::move(other.buffer_)),
      gap_start_(other.gap_start_),
      gap_end_(other.gap_end_),
      insertions_(other.insertions_),
      deletions_(other.deletions_),
      gap_moves_(other.gap_moves_),
      last_operation_(other.last_operation_) {
    
    // Reset other object
    other.gap_start_ = 0;
    other.gap_end_ = 0;
    other.insertions_ = 0;
    other.deletions_ = 0;
    other.gap_moves_ = 0;
}

GapBuffer& GapBuffer::operator=(GapBuffer&& other) noexcept {
    if (this != &other) {
        // Secure clear existing buffer
        if (!buffer_.empty()) {
            nx::util::Security::secureZero(buffer_.data(), buffer_.size());
        }
        
        config_ = std::move(other.config_);
        buffer_ = std::move(other.buffer_);
        gap_start_ = other.gap_start_;
        gap_end_ = other.gap_end_;
        insertions_ = other.insertions_;
        deletions_ = other.deletions_;
        gap_moves_ = other.gap_moves_;
        last_operation_ = other.last_operation_;
        
        // Reset other object
        other.gap_start_ = 0;
        other.gap_end_ = 0;
        other.insertions_ = 0;
        other.deletions_ = 0;
        other.gap_moves_ = 0;
    }
    return *this;
}

Result<void> GapBuffer::initialize(const std::string& content) {
    // Validate content size
    if (content.size() > config_.max_buffer_size) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Content size exceeds maximum buffer size");
    }

    // Clear existing buffer securely
    if (!buffer_.empty()) {
        nx::util::Security::secureZero(buffer_.data(), buffer_.size());
    }

    // Calculate required buffer size
    size_t required_size = content.size() + config_.initial_gap_size;
    buffer_.resize(required_size);

    // Copy content to beginning of buffer
    if (!content.empty()) {
        std::memcpy(buffer_.data(), content.data(), content.size());
    }

    // Set gap position after content
    gap_start_ = content.size();
    gap_end_ = required_size;

    recordOperation();
    return {};
}

Result<void> GapBuffer::insertChar(char ch) {
    // Ensure gap has space
    auto gap_result = ensureGapSize(1);
    if (!gap_result) {
        return gap_result;
    }

    // Insert character at gap start
    buffer_[gap_start_] = ch;
    gap_start_++;
    insertions_++;
    
    recordOperation();
    return {};
}

Result<void> GapBuffer::insertString(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    // Validate total size wouldn't exceed limits
    if (size() + text.size() > config_.max_buffer_size) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Insertion would exceed maximum buffer size");
    }

    // Ensure gap has enough space
    auto gap_result = ensureGapSize(text.size());
    if (!gap_result) {
        return gap_result;
    }

    // Copy text into gap
    std::memcpy(buffer_.data() + gap_start_, text.data(), text.size());
    gap_start_ += text.size();
    insertions_++;
    
    recordOperation();
    return {};
}

Result<char> GapBuffer::deleteCharBefore() {
    if (gap_start_ == 0) {
        return makeErrorResult<char>(ErrorCode::kValidationError, "No character to delete before gap");
    }

    gap_start_--;
    char deleted_char = buffer_[gap_start_];
    deletions_++;
    
    recordOperation();
    return deleted_char;
}

Result<char> GapBuffer::deleteCharAfter() {
    if (gap_end_ >= buffer_.size()) {
        return makeErrorResult<char>(ErrorCode::kValidationError, "No character to delete after gap");
    }

    char deleted_char = buffer_[gap_end_];
    gap_end_++;
    deletions_++;
    
    recordOperation();
    return deleted_char;
}

Result<std::string> GapBuffer::deleteRange(size_t start_pos, size_t end_pos) {
    if (start_pos > end_pos) {
        return makeErrorResult<std::string>(ErrorCode::kValidationError, "Invalid range: start > end");
    }

    if (end_pos > size()) {
        return makeErrorResult<std::string>(ErrorCode::kValidationError, "Range end exceeds buffer size");
    }

    // Get the text that will be deleted
    auto deleted_text_result = getSubstring(start_pos, end_pos - start_pos);
    if (!deleted_text_result) {
        return std::unexpected(deleted_text_result.error());
    }

    // Move gap to start of deletion range
    auto move_result = moveGapTo(start_pos);
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    // Expand gap to include deleted range
    size_t delete_size = end_pos - start_pos;
    gap_end_ += delete_size;
    
    // Clamp gap_end to buffer size
    gap_end_ = std::min(gap_end_, buffer_.size());
    
    deletions_++;
    recordOperation();
    
    return deleted_text_result.value();
}

Result<void> GapBuffer::moveGapTo(size_t position) {
    if (position > size()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "Position exceeds buffer size");
    }

    size_t gap_position = getGapPosition();
    
    if (position == gap_position) {
        return {}; // Already at correct position
    }

    if (position < gap_position) {
        // Move gap left
        size_t move_size = gap_position - position;
        size_t src_start = position;
        size_t dst_start = gap_end_ - move_size;
        
        // Move text from before gap to after gap
        std::memmove(buffer_.data() + dst_start, 
                     buffer_.data() + src_start, 
                     move_size);
        
        gap_start_ -= move_size;
        gap_end_ -= move_size;
    } else {
        // Move gap right
        size_t move_size = position - gap_position;
        size_t src_start = gap_end_;
        size_t dst_start = gap_start_;
        
        // Move text from after gap to before gap
        std::memmove(buffer_.data() + dst_start,
                     buffer_.data() + src_start,
                     move_size);
        
        gap_start_ += move_size;
        gap_end_ += move_size;
    }

    gap_moves_++;
    recordOperation();
    return {};
}

Result<char> GapBuffer::getCharAt(size_t position) const {
    auto validation_result = validatePosition(position);
    if (!validation_result) {
        return std::unexpected(validation_result.error());
    }

    size_t physical_pos = logicalToPhysical(position);
    return buffer_[physical_pos];
}

Result<std::string> GapBuffer::getSubstring(size_t start, size_t length) const {
    if (start > size()) {
        return makeErrorResult<std::string>(ErrorCode::kValidationError, "Start position exceeds buffer size");
    }

    // Clamp length to available characters
    length = std::min(length, size() - start);
    
    if (length == 0) {
        return std::string();
    }

    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        auto char_result = getCharAt(start + i);
        if (!char_result) {
            return std::unexpected(char_result.error());
        }
        result += char_result.value();
    }

    return result;
}

std::string GapBuffer::toString() const {
    std::string result;
    result.reserve(size());

    // Copy text before gap
    if (gap_start_ > 0) {
        result.append(buffer_.data(), gap_start_);
    }

    // Copy text after gap
    if (gap_end_ < buffer_.size()) {
        result.append(buffer_.data() + gap_end_, buffer_.size() - gap_end_);
    }

    return result;
}

std::vector<std::string> GapBuffer::toLines() const {
    std::vector<std::string> lines;
    std::string content = toString();
    
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    // Handle case where content doesn't end with newline
    if (!content.empty() && content.back() != '\n') {
        // Last line already added by getline
    } else if (!lines.empty() || content.empty()) {
        // Add empty line if content ends with newline or is empty
        if (content.empty()) {
            lines.push_back("");
        }
    }

    return lines;
}

size_t GapBuffer::size() const {
    return buffer_.size() - (gap_end_ - gap_start_);
}

bool GapBuffer::empty() const {
    return size() == 0;
}

size_t GapBuffer::getGapPosition() const {
    return gap_start_;
}

size_t GapBuffer::getGapSize() const {
    return gap_end_ - gap_start_;
}

size_t GapBuffer::getPhysicalSize() const {
    return buffer_.size();
}

double GapBuffer::getUtilization() const {
    if (buffer_.empty()) return 0.0;
    return static_cast<double>(size()) / static_cast<double>(buffer_.size());
}

Result<void> GapBuffer::compact() {
    size_t logical_size = size();
    size_t new_gap_size = std::max(config_.min_gap_size, 
                                   static_cast<size_t>(logical_size * 0.1));
    size_t new_physical_size = logical_size + new_gap_size;

    // Create new buffer
    std::vector<char> new_buffer(new_physical_size);

    // Copy content to new buffer
    std::string content = toString();
    if (!content.empty()) {
        std::memcpy(new_buffer.data(), content.data(), content.size());
    }

    // Secure clear old buffer
    nx::util::Security::secureZero(buffer_.data(), buffer_.size());

    // Update buffer and gap positions
    buffer_ = std::move(new_buffer);
    gap_start_ = logical_size;
    gap_end_ = new_physical_size;

    recordOperation();
    return {};
}

void GapBuffer::clear() {
    // Secure clear existing content
    if (!buffer_.empty()) {
        nx::util::Security::secureZero(buffer_.data(), buffer_.size());
    }

    // Reset to initial state
    buffer_.resize(config_.initial_gap_size);
    gap_start_ = 0;
    gap_end_ = config_.initial_gap_size;
    
    // Reset statistics
    insertions_ = 0;
    deletions_ = 0;
    gap_moves_ = 0;
    
    recordOperation();
}

GapBuffer::Statistics GapBuffer::getStatistics() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_operation_);
    
    return Statistics{
        .logical_size = size(),
        .physical_size = getPhysicalSize(),
        .gap_size = getGapSize(),
        .gap_position = getGapPosition(),
        .utilization = getUtilization(),
        .insertions = insertions_,
        .deletions = deletions_,
        .gap_moves = gap_moves_,
        .last_operation_time = duration
    };
}

Result<void> GapBuffer::ensureGapSize(size_t required_size) {
    size_t current_gap_size = getGapSize();
    
    if (current_gap_size >= required_size) {
        return {}; // Gap is already large enough
    }

    // Calculate new buffer size
    size_t current_logical_size = size();
    size_t new_gap_size = std::max(required_size, 
                                   static_cast<size_t>(current_gap_size * config_.gap_growth_factor));
    new_gap_size = std::min(new_gap_size, config_.max_gap_size);
    
    size_t new_physical_size = current_logical_size + new_gap_size;
    
    if (new_physical_size > config_.max_buffer_size) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Buffer expansion would exceed maximum size");
    }

    return growBuffer(new_physical_size);
}

Result<void> GapBuffer::growBuffer(size_t new_capacity) {
    if (new_capacity <= buffer_.size()) {
        return {}; // No growth needed
    }

    try {
        std::vector<char> new_buffer(new_capacity);
        
        // Copy text before gap
        if (gap_start_ > 0) {
            std::memcpy(new_buffer.data(), buffer_.data(), gap_start_);
        }
        
        // Copy text after gap to end of new buffer
        size_t after_gap_size = buffer_.size() - gap_end_;
        if (after_gap_size > 0) {
            size_t new_after_gap_start = new_capacity - after_gap_size;
            std::memcpy(new_buffer.data() + new_after_gap_start,
                       buffer_.data() + gap_end_,
                       after_gap_size);
            gap_end_ = new_after_gap_start;
        } else {
            gap_end_ = new_capacity;
        }
        
        // Secure clear old buffer
        nx::util::Security::secureZero(buffer_.data(), buffer_.size());
        
        buffer_ = std::move(new_buffer);
        
        return {};
    } catch (const std::exception& e) {
        return makeErrorResult<void>(ErrorCode::kSystemError, 
            "Failed to grow buffer: " + std::string(e.what()));
    }
}

size_t GapBuffer::logicalToPhysical(size_t logical_pos) const {
    if (logical_pos < gap_start_) {
        return logical_pos;
    } else {
        return logical_pos + (gap_end_ - gap_start_);
    }
}

size_t GapBuffer::physicalToLogical(size_t physical_pos) const {
    if (physical_pos < gap_start_) {
        return physical_pos;
    } else if (physical_pos >= gap_end_) {
        return physical_pos - (gap_end_ - gap_start_);
    } else {
        // Position is within gap - invalid
        return size(); // Return end position
    }
}

Result<void> GapBuffer::validatePosition(size_t position) const {
    if (position > size()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Position " + std::to_string(position) + " exceeds buffer size " + std::to_string(size()));
    }
    return {};
}

void GapBuffer::recordOperation() const {
    last_operation_ = std::chrono::steady_clock::now();
}

// EditorBuffer Implementation

EditorBuffer::EditorBuffer(Config config) 
    : config_(std::move(config)) {
    gap_buffer_ = std::make_unique<GapBuffer>(config_.gap_config);
}

Result<void> EditorBuffer::initialize(const std::string& content) {
    auto result = gap_buffer_->initialize(content);
    if (result) {
        invalidateLineCache();
    }
    return result;
}

size_t EditorBuffer::getLineCount() const {
    rebuildLineCache();
    return line_starts_.size();
}

Result<std::string> EditorBuffer::getLine(size_t line_index) const {
    rebuildLineCache();
    
    if (line_index >= line_starts_.size()) {
        return makeErrorResult<std::string>(ErrorCode::kValidationError, "Line index out of bounds");
    }

    size_t start_pos = line_starts_[line_index];
    size_t end_pos;
    
    if (line_index + 1 < line_starts_.size()) {
        end_pos = line_starts_[line_index + 1] - 1; // Exclude newline
    } else {
        end_pos = gap_buffer_->size();
    }

    if (end_pos <= start_pos) {
        return std::string(); // Empty line
    }

    return gap_buffer_->getSubstring(start_pos, end_pos - start_pos);
}

Result<void> EditorBuffer::setLine(size_t line_index, const std::string& content) {
    if (content.size() > config_.max_line_length) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "Line exceeds maximum length");
    }

    rebuildLineCache();
    
    if (line_index >= line_starts_.size()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "Line index out of bounds");
    }

    // Calculate line boundaries
    size_t start_pos = line_starts_[line_index];
    size_t end_pos;
    
    if (line_index + 1 < line_starts_.size()) {
        end_pos = line_starts_[line_index + 1];
    } else {
        end_pos = gap_buffer_->size();
    }

    // Delete existing line content
    if (end_pos > start_pos) {
        auto delete_result = gap_buffer_->deleteRange(start_pos, end_pos);
        if (!delete_result) {
            return std::unexpected(delete_result.error());
        }
    }

    // Insert new content
    auto move_result = gap_buffer_->moveGapTo(start_pos);
    if (!move_result) {
        return move_result;
    }

    auto insert_result = gap_buffer_->insertString(content);
    if (!insert_result) {
        return insert_result;
    }

    // Add newline if not last line
    if (line_index + 1 < line_starts_.size()) {
        auto newline_result = gap_buffer_->insertChar('\n');
        if (!newline_result) {
            return newline_result;
        }
    }

    invalidateLineCache();
    line_operations_++;
    return {};
}

std::string EditorBuffer::toString() const {
    return gap_buffer_->toString();
}

std::vector<std::string> EditorBuffer::toLines() const {
    return gap_buffer_->toLines();
}

void EditorBuffer::clear() {
    gap_buffer_->clear();
    invalidateLineCache();
}

EditorBuffer::Statistics EditorBuffer::getStatistics() const {
    auto gap_stats = gap_buffer_->getStatistics();
    
    return Statistics{
        .gap_stats = gap_stats,
        .line_count = getLineCount(),
        .total_characters = gap_stats.logical_size,
        .line_operations = line_operations_,
        .char_operations = char_operations_
    };
}

void EditorBuffer::rebuildLineCache() const {
    if (!line_cache_dirty_) {
        return;
    }

    line_starts_.clear();
    
    // Always have at least one line, even for empty buffers
    line_starts_.push_back(0); // First line starts at 0
    
    // Find all newline positions
    for (size_t i = 0; i < gap_buffer_->size(); ++i) {
        auto char_result = gap_buffer_->getCharAt(i);
        if (char_result && char_result.value() == '\n') {
            line_starts_.push_back(i + 1);
        }
    }

    line_cache_dirty_ = false;
}

void EditorBuffer::invalidateLineCache() {
    line_cache_dirty_ = true;
}

Result<size_t> EditorBuffer::lineColToPosition(size_t line_index, size_t col_index) const {
    rebuildLineCache();
    
    if (line_index >= line_starts_.size()) {
        return makeErrorResult<size_t>(ErrorCode::kValidationError, "Line index out of bounds");
    }

    size_t line_start = line_starts_[line_index];
    
    // Get line length to validate column
    auto line_result = getLine(line_index);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }
    
    if (col_index > line_result.value().length()) {
        return makeErrorResult<size_t>(ErrorCode::kValidationError, "Column index exceeds line length");
    }

    return line_start + col_index;
}

Result<void> EditorBuffer::insertLine(size_t line_index, const std::string& content) {
    if (content.size() > config_.max_line_length) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "Line exceeds maximum length");
    }

    rebuildLineCache();
    
    if (line_index > line_starts_.size()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "Line index out of bounds for insertion");
    }

    size_t insert_pos;
    if (line_index == 0) {
        insert_pos = 0;
    } else if (line_index >= line_starts_.size()) {
        insert_pos = gap_buffer_->size();
    } else {
        insert_pos = line_starts_[line_index];
    }

    // Move gap to insertion position
    auto move_result = gap_buffer_->moveGapTo(insert_pos);
    if (!move_result) {
        return move_result;
    }

    // Insert content
    auto insert_result = gap_buffer_->insertString(content);
    if (!insert_result) {
        return insert_result;
    }

    // Add newline
    auto newline_result = gap_buffer_->insertChar('\n');
    if (!newline_result) {
        return newline_result;
    }

    invalidateLineCache();
    line_operations_++;
    return {};
}

Result<std::string> EditorBuffer::deleteLine(size_t line_index) {
    rebuildLineCache();
    
    if (line_index >= line_starts_.size()) {
        return makeErrorResult<std::string>(ErrorCode::kValidationError, "Line index out of bounds");
    }

    // Get line content before deletion
    auto line_result = getLine(line_index);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }

    size_t start_pos = line_starts_[line_index];
    size_t end_pos;
    
    if (line_index + 1 < line_starts_.size()) {
        end_pos = line_starts_[line_index + 1]; // Include newline
    } else {
        end_pos = gap_buffer_->size();
    }

    // Delete the line range
    auto delete_result = gap_buffer_->deleteRange(start_pos, end_pos);
    if (!delete_result) {
        return std::unexpected(delete_result.error());
    }

    invalidateLineCache();
    line_operations_++;
    return line_result.value();
}

Result<void> EditorBuffer::insertChar(size_t line_index, size_t col_index, char ch) {
    auto pos_result = lineColToPosition(line_index, col_index);
    if (!pos_result) {
        return std::unexpected(pos_result.error());
    }

    auto move_result = gap_buffer_->moveGapTo(pos_result.value());
    if (!move_result) {
        return move_result;
    }

    auto insert_result = gap_buffer_->insertChar(ch);
    if (!insert_result) {
        return insert_result;
    }

    // Any character insertion invalidates the line cache since positions change
    invalidateLineCache();

    char_operations_++;
    return {};
}

Result<char> EditorBuffer::deleteChar(size_t line_index, size_t col_index) {
    auto pos_result = lineColToPosition(line_index, col_index);
    if (!pos_result) {
        return std::unexpected(pos_result.error());
    }

    auto move_result = gap_buffer_->moveGapTo(pos_result.value());
    if (!move_result) {
        return std::unexpected(move_result.error());
    }

    auto delete_result = gap_buffer_->deleteCharAfter();
    if (!delete_result) {
        return delete_result;
    }

    // Any character deletion invalidates the line cache since positions change
    invalidateLineCache();

    char_operations_++;
    return delete_result.value();
}

Result<void> EditorBuffer::splitLine(size_t line_index, size_t col_index) {
    auto pos_result = lineColToPosition(line_index, col_index);
    if (!pos_result) {
        return std::unexpected(pos_result.error());
    }

    auto move_result = gap_buffer_->moveGapTo(pos_result.value());
    if (!move_result) {
        return move_result;
    }

    auto insert_result = gap_buffer_->insertChar('\n');
    if (!insert_result) {
        return insert_result;
    }

    invalidateLineCache();
    line_operations_++;
    return {};
}

Result<void> EditorBuffer::joinLines(size_t line_index) {
    rebuildLineCache();
    
    if (line_index >= line_starts_.size() - 1) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "Cannot join last line or line index out of bounds");
    }

    // Find the newline at the end of the current line
    size_t newline_pos;
    if (line_index + 1 < line_starts_.size()) {
        newline_pos = line_starts_[line_index + 1] - 1;
    } else {
        return makeErrorResult<void>(ErrorCode::kValidationError, "No next line to join");
    }

    // Move gap to newline position and delete it
    auto move_result = gap_buffer_->moveGapTo(newline_pos);
    if (!move_result) {
        return move_result;
    }

    auto delete_result = gap_buffer_->deleteCharAfter();
    if (!delete_result) {
        return std::unexpected(delete_result.error());
    }

    invalidateLineCache();
    line_operations_++;
    return {};
}

Result<std::pair<size_t, size_t>> EditorBuffer::positionToLineCol(size_t position) const {
    rebuildLineCache();
    
    if (position > gap_buffer_->size()) {
        return makeErrorResult<std::pair<size_t, size_t>>(ErrorCode::kValidationError, "Position out of bounds");
    }

    // Binary search through line starts to find the line
    auto it = std::upper_bound(line_starts_.begin(), line_starts_.end(), position);
    if (it == line_starts_.begin()) {
        return std::make_pair(0, position);
    }

    --it;
    size_t line_index = std::distance(line_starts_.begin(), it);
    size_t col_index = position - *it;
    
    return std::make_pair(line_index, col_index);
}

} // namespace nx::tui