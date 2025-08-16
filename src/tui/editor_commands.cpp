#include "nx/tui/editor_commands.hpp"
#include <algorithm>
#include <sstream>

namespace nx::tui {

// Helper function implementation
bool EditorCommand::isWithinMergeThreshold(
    std::chrono::steady_clock::time_point t1,
    std::chrono::steady_clock::time_point t2,
    std::chrono::milliseconds threshold) {
    
    auto diff = std::abs(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t2).count());
    return diff <= threshold.count();
}

// CursorPosition implementation

bool CursorPosition::isAdjacentTo(const CursorPosition& other) const {
    // Same line, adjacent columns
    if (line == other.line) {
        return std::abs(static_cast<long>(column) - static_cast<long>(other.column)) == 1;
    }
    
    // For this implementation, we only consider positions on the same line as adjacent
    // Cross-line adjacency would require knowing line lengths, which we don't have here
    // This is sufficient for command merging purposes
    
    return false;
}

Result<void> CursorPosition::validate(const EditorBuffer& buffer) const {
    size_t line_count = buffer.getLineCount();
    
    if (line >= line_count) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Line " + std::to_string(line) + " exceeds buffer line count " + std::to_string(line_count));
    }
    
    auto line_result = buffer.getLine(line);
    if (!line_result) {
        return std::unexpected(line_result.error());
    }
    
    if (column > line_result.value().length()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, 
            "Column " + std::to_string(column) + " exceeds line length " + std::to_string(line_result.value().length()));
    }
    
    return {};
}

// InsertTextCommand implementation

InsertTextCommand::InsertTextCommand(CursorPosition position, std::string text)
    : position_(position), text_(std::move(text)), timestamp_(std::chrono::steady_clock::now()) {
}

Result<void> InsertTextCommand::execute(EditorBuffer& buffer) {
    // Validate position
    auto validation = position_.validate(buffer);
    if (!validation) {
        return validation;
    }
    
    // Insert character by character to handle line breaks properly
    CursorPosition current_pos = position_;
    
    for (char ch : text_) {
        if (ch == '\n') {
            // Split line at current position
            auto split_result = buffer.splitLine(current_pos.line, current_pos.column);
            if (!split_result) {
                return split_result;
            }
            current_pos.line++;
            current_pos.column = 0;
        } else {
            // Insert character
            auto insert_result = buffer.insertChar(current_pos.line, current_pos.column, ch);
            if (!insert_result) {
                return insert_result;
            }
            current_pos.column++;
        }
    }
    
    return {};
}

Result<void> InsertTextCommand::undo(EditorBuffer& buffer) {
    // Calculate end position after insertion
    CursorPosition start_pos = position_;
    CursorPosition end_pos = position_;
    
    // Calculate where text ends after insertion
    for (char ch : text_) {
        if (ch == '\n') {
            end_pos.line++;
            end_pos.column = 0;
        } else {
            end_pos.column++;
        }
    }
    
    // Delete the inserted text
    for (size_t i = 0; i < text_.length(); ++i) {
        char last_char = text_[text_.length() - 1 - i];
        
        if (last_char == '\n') {
            // Join lines
            if (end_pos.line > 0) {
                auto join_result = buffer.joinLines(end_pos.line - 1);
                if (!join_result) {
                    return join_result;
                }
                end_pos.line--;
                // Set column to end of previous line
                auto line_result = buffer.getLine(end_pos.line);
                if (line_result) {
                    end_pos.column = line_result.value().length();
                }
            }
        } else {
            // Delete character
            if (end_pos.column > 0) {
                auto delete_result = buffer.deleteChar(end_pos.line, end_pos.column - 1);
                if (!delete_result) {
                    return std::unexpected(delete_result.error());
                }
                end_pos.column--;
            }
        }
    }
    
    return {};
}

bool InsertTextCommand::canMergeWith(const EditorCommand& other) const {
    auto* other_insert = dynamic_cast<const InsertTextCommand*>(&other);
    if (!other_insert) {
        return false;
    }
    
    // Check time threshold
    if (!isWithinMergeThreshold(timestamp_, other_insert->timestamp_, MERGE_TIME_THRESHOLD)) {
        return false;
    }
    
    // Check if combined text would be too long
    if (text_.length() + other_insert->text_.length() > MAX_MERGE_LENGTH) {
        return false;
    }
    
    // Check if positions are adjacent
    CursorPosition my_end = position_;
    for (char ch : text_) {
        if (ch == '\n') {
            my_end.line++;
            my_end.column = 0;
        } else {
            my_end.column++;
        }
    }
    
    return my_end == other_insert->position_;
}

Result<std::unique_ptr<EditorCommand>> InsertTextCommand::mergeWith(std::unique_ptr<EditorCommand> other) {
    auto other_insert = dynamic_cast<InsertTextCommand*>(other.get());
    if (!other_insert || !canMergeWith(*other_insert)) {
        return makeErrorResult<std::unique_ptr<EditorCommand>>(ErrorCode::kValidationError, 
            "Cannot merge incompatible commands");
    }
    
    // Create merged command
    std::string merged_text = text_ + other_insert->text_;
    auto merged_cmd = std::make_unique<InsertTextCommand>(position_, std::move(merged_text));
    merged_cmd->timestamp_ = std::min(timestamp_, other_insert->timestamp_);
    
    return std::unique_ptr<EditorCommand>(std::move(merged_cmd));
}

std::chrono::steady_clock::time_point InsertTextCommand::getTimestamp() const {
    return timestamp_;
}

std::string InsertTextCommand::getDescription() const {
    std::ostringstream oss;
    oss << "Insert \"" << text_ << "\" at (" << position_.line << ", " << position_.column << ")";
    return oss.str();
}

size_t InsertTextCommand::getMemoryUsage() const {
    return sizeof(*this) + text_.capacity();
}

// DeleteTextCommand implementation

DeleteTextCommand::DeleteTextCommand(CursorPosition start_position, CursorPosition end_position, std::string deleted_text)
    : start_position_(start_position), end_position_(end_position), 
      deleted_text_(std::move(deleted_text)), timestamp_(std::chrono::steady_clock::now()) {
}

Result<void> DeleteTextCommand::execute(EditorBuffer& buffer) {
    // Validate positions
    auto start_validation = start_position_.validate(buffer);
    if (!start_validation) {
        return start_validation;
    }
    
    auto end_validation = end_position_.validate(buffer);
    if (!end_validation) {
        return end_validation;
    }
    
    // Delete character by character from end to start
    CursorPosition current_pos = end_position_;
    
    for (size_t i = 0; i < deleted_text_.length(); ++i) {
        char ch = deleted_text_[deleted_text_.length() - 1 - i];
        
        if (ch == '\n') {
            // Join lines
            if (current_pos.line > 0) {
                auto join_result = buffer.joinLines(current_pos.line - 1);
                if (!join_result) {
                    return join_result;
                }
                current_pos.line--;
                // Update column to end of previous line
                auto line_result = buffer.getLine(current_pos.line);
                if (line_result) {
                    current_pos.column = line_result.value().length();
                }
            }
        } else {
            // Delete character
            if (current_pos.column > 0) {
                auto delete_result = buffer.deleteChar(current_pos.line, current_pos.column - 1);
                if (!delete_result) {
                    return std::unexpected(delete_result.error());
                }
                current_pos.column--;
            }
        }
    }
    
    return {};
}

Result<void> DeleteTextCommand::undo(EditorBuffer& buffer) {
    // Re-insert the deleted text
    auto insert_cmd = std::make_unique<InsertTextCommand>(start_position_, deleted_text_);
    return insert_cmd->execute(buffer);
}

bool DeleteTextCommand::canMergeWith(const EditorCommand& other) const {
    auto* other_delete = dynamic_cast<const DeleteTextCommand*>(&other);
    if (!other_delete) {
        return false;
    }
    
    // Check time threshold
    if (!isWithinMergeThreshold(timestamp_, other_delete->timestamp_, 
                               std::chrono::milliseconds(1000))) {
        return false;
    }
    
    // Check if positions are adjacent
    return start_position_ == other_delete->end_position_ || 
           end_position_ == other_delete->start_position_;
}

Result<std::unique_ptr<EditorCommand>> DeleteTextCommand::mergeWith(std::unique_ptr<EditorCommand> other) {
    auto other_delete = dynamic_cast<DeleteTextCommand*>(other.get());
    if (!other_delete || !canMergeWith(*other_delete)) {
        return makeErrorResult<std::unique_ptr<EditorCommand>>(ErrorCode::kValidationError, 
            "Cannot merge incompatible commands");
    }
    
    // Determine merge order
    CursorPosition new_start, new_end;
    std::string merged_text;
    
    if (start_position_ == other_delete->end_position_) {
        // Other command comes before this one
        new_start = other_delete->start_position_;
        new_end = end_position_;
        merged_text = other_delete->deleted_text_ + deleted_text_;
    } else {
        // This command comes before other one
        new_start = start_position_;
        new_end = other_delete->end_position_;
        merged_text = deleted_text_ + other_delete->deleted_text_;
    }
    
    auto merged_cmd = std::make_unique<DeleteTextCommand>(new_start, new_end, std::move(merged_text));
    merged_cmd->timestamp_ = std::min(timestamp_, other_delete->timestamp_);
    
    return std::unique_ptr<EditorCommand>(std::move(merged_cmd));
}

std::chrono::steady_clock::time_point DeleteTextCommand::getTimestamp() const {
    return timestamp_;
}

std::string DeleteTextCommand::getDescription() const {
    std::ostringstream oss;
    oss << "Delete \"" << deleted_text_ << "\" from (" 
        << start_position_.line << ", " << start_position_.column << ") to ("
        << end_position_.line << ", " << end_position_.column << ")";
    return oss.str();
}

size_t DeleteTextCommand::getMemoryUsage() const {
    return sizeof(*this) + deleted_text_.capacity();
}

// ReplaceTextCommand implementation

ReplaceTextCommand::ReplaceTextCommand(CursorPosition start_position, CursorPosition end_position, 
                                     std::string old_text, std::string new_text)
    : start_position_(start_position), end_position_(end_position),
      old_text_(std::move(old_text)), new_text_(std::move(new_text)),
      timestamp_(std::chrono::steady_clock::now()) {
}

Result<void> ReplaceTextCommand::execute(EditorBuffer& buffer) {
    // First delete the old text
    auto delete_cmd = std::make_unique<DeleteTextCommand>(start_position_, end_position_, old_text_);
    auto delete_result = delete_cmd->execute(buffer);
    if (!delete_result) {
        return delete_result;
    }
    
    // Then insert the new text
    auto insert_cmd = std::make_unique<InsertTextCommand>(start_position_, new_text_);
    return insert_cmd->execute(buffer);
}

Result<void> ReplaceTextCommand::undo(EditorBuffer& buffer) {
    // Calculate where new text ends
    CursorPosition new_end = start_position_;
    for (char ch : new_text_) {
        if (ch == '\n') {
            new_end.line++;
            new_end.column = 0;
        } else {
            new_end.column++;
        }
    }
    
    // Delete the new text
    auto delete_cmd = std::make_unique<DeleteTextCommand>(start_position_, new_end, new_text_);
    auto delete_result = delete_cmd->execute(buffer);
    if (!delete_result) {
        return delete_result;
    }
    
    // Insert the old text
    auto insert_cmd = std::make_unique<InsertTextCommand>(start_position_, old_text_);
    return insert_cmd->execute(buffer);
}

bool ReplaceTextCommand::canMergeWith(const EditorCommand& other) const {
    // Replace commands are generally not merged for simplicity
    return false;
}

Result<std::unique_ptr<EditorCommand>> ReplaceTextCommand::mergeWith(std::unique_ptr<EditorCommand> other) {
    return makeErrorResult<std::unique_ptr<EditorCommand>>(ErrorCode::kValidationError, 
        "Replace commands cannot be merged");
}

std::chrono::steady_clock::time_point ReplaceTextCommand::getTimestamp() const {
    return timestamp_;
}

std::string ReplaceTextCommand::getDescription() const {
    std::ostringstream oss;
    oss << "Replace \"" << old_text_ << "\" with \"" << new_text_ << "\" at ("
        << start_position_.line << ", " << start_position_.column << ")";
    return oss.str();
}

size_t ReplaceTextCommand::getMemoryUsage() const {
    return sizeof(*this) + old_text_.capacity() + new_text_.capacity();
}

// MergedCommand implementation

MergedCommand::MergedCommand(std::unique_ptr<EditorCommand> first, std::unique_ptr<EditorCommand> second) {
    commands_.push_back(std::move(first));
    commands_.push_back(std::move(second));
    
    earliest_timestamp_ = commands_[0]->getTimestamp();
    latest_timestamp_ = commands_[1]->getTimestamp();
    
    if (earliest_timestamp_ > latest_timestamp_) {
        std::swap(earliest_timestamp_, latest_timestamp_);
    }
}

Result<void> MergedCommand::execute(EditorBuffer& buffer) {
    for (auto& command : commands_) {
        auto result = command->execute(buffer);
        if (!result) {
            return result;
        }
    }
    return {};
}

Result<void> MergedCommand::undo(EditorBuffer& buffer) {
    // Undo in reverse order
    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
        auto result = (*it)->undo(buffer);
        if (!result) {
            return result;
        }
    }
    return {};
}

bool MergedCommand::canMergeWith(const EditorCommand& other) const {
    // Check if the last command in our sequence can merge with the other command
    if (commands_.empty()) {
        return false;
    }
    
    return commands_.back()->canMergeWith(other);
}

Result<std::unique_ptr<EditorCommand>> MergedCommand::mergeWith(std::unique_ptr<EditorCommand> other) {
    if (!canMergeWith(*other)) {
        return makeErrorResult<std::unique_ptr<EditorCommand>>(ErrorCode::kValidationError, 
            "Cannot merge incompatible commands");
    }
    
    // Try to merge the last command with the other command
    auto merge_result = commands_.back()->mergeWith(std::move(other));
    if (!merge_result) {
        return merge_result;
    }
    
    // Create new merged command with the merged result replacing the last command
    auto new_merged = std::make_unique<MergedCommand>(std::move(commands_[0]), std::move(merge_result.value()));
    
    // Add remaining commands if any
    for (size_t i = 1; i < commands_.size() - 1; ++i) {
        new_merged->commands_.push_back(std::move(commands_[i]));
    }
    
    return std::unique_ptr<EditorCommand>(std::move(new_merged));
}

std::chrono::steady_clock::time_point MergedCommand::getTimestamp() const {
    return earliest_timestamp_;
}

std::string MergedCommand::getDescription() const {
    std::ostringstream oss;
    oss << "Merged(" << commands_.size() << " commands)";
    return oss.str();
}

size_t MergedCommand::getMemoryUsage() const {
    size_t total = sizeof(*this);
    for (const auto& command : commands_) {
        total += command->getMemoryUsage();
    }
    return total;
}

// CommandHistory implementation

CommandHistory::CommandHistory(Config config) 
    : config_(std::move(config)), head_(0), tail_(0), current_(0), count_(0), memory_usage_(0) {
}

CommandHistory::~CommandHistory() {
    clear();
}

Result<void> CommandHistory::executeCommand(EditorBuffer& buffer, std::unique_ptr<EditorCommand> command) {
    // Execute the command first
    auto execute_result = command->execute(buffer);
    if (!execute_result) {
        return execute_result;
    }
    
    // Try to merge with last command if auto-merge is enabled
    if (config_.auto_merge_commands && count_ > 0) {
        if (tryMergeWithLast(command)) {
            return {}; // Command was merged, nothing more to do
        }
    }
    
    // Clear any redo history when executing new command
    if (current_ < head_) {
        // Remove commands from current to head
        while (current_ < head_) {
            if (history_[current_]) {
                memory_usage_ -= history_[current_]->getMemoryUsage();
                history_[current_].reset();
            }
            current_ = nextIndex(current_);
            count_--;
        }
        head_ = current_;
    }
    
    // Add command to history
    addToHistory(std::move(command));
    
    // Enforce memory limits
    if (isOverMemoryLimit()) {
        enforceMemoryLimit();
    }
    
    return {};
}

Result<void> CommandHistory::undo(EditorBuffer& buffer) {
    if (!canUndo()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "No commands to undo");
    }
    
    // Move to previous command
    current_ = prevIndex(current_);
    
    // Undo the command
    auto command = getCommandAt(current_);
    if (!command) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "Invalid command in history");
    }
    
    return command->undo(buffer);
}

Result<void> CommandHistory::redo(EditorBuffer& buffer) {
    if (!canRedo()) {
        return makeErrorResult<void>(ErrorCode::kValidationError, "No commands to redo");
    }
    
    // Get command to redo
    auto command = getCommandAt(current_);
    if (!command) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "Invalid command in history");
    }
    
    // Execute the command
    auto result = command->execute(buffer);
    if (!result) {
        return result;
    }
    
    // Move current forward
    current_ = nextIndex(current_);
    
    return {};
}

bool CommandHistory::canUndo() const {
    return count_ > 0 && current_ != tail_;
}

bool CommandHistory::canRedo() const {
    return current_ != head_;
}

void CommandHistory::clear() {
    for (auto& command : history_) {
        command.reset();
    }
    head_ = tail_ = current_ = count_ = memory_usage_ = 0;
}

size_t CommandHistory::getMemoryUsage() const {
    return memory_usage_;
}

CommandHistory::Statistics CommandHistory::getStatistics() const {
    size_t undo_count = 0;
    size_t redo_count = 0;
    size_t merged_count = 0;
    
    // Count undo commands (from tail to current)
    size_t pos = tail_;
    while (pos != current_ && count_ > 0) {
        if (history_[pos]) {
            undo_count++;
            if (dynamic_cast<const MergedCommand*>(history_[pos].get())) {
                merged_count++;
            }
        }
        pos = nextIndex(pos);
    }
    
    // Count redo commands (from current to head)
    pos = current_;
    while (pos != head_ && count_ > 0) {
        if (history_[pos]) {
            redo_count++;
            if (dynamic_cast<const MergedCommand*>(history_[pos].get())) {
                merged_count++;
            }
        }
        pos = nextIndex(pos);
    }
    
    return Statistics{
        .total_commands = count_,
        .undo_commands = undo_count,
        .redo_commands = redo_count,
        .merged_commands = merged_count,
        .memory_usage = memory_usage_,
        .memory_limit = config_.memory_limit_bytes,
        .memory_utilization = config_.memory_limit_bytes > 0 ? 
            static_cast<double>(memory_usage_) / static_cast<double>(config_.memory_limit_bytes) : 0.0
    };
}

size_t CommandHistory::compactHistory() {
    size_t original_count = count_;
    
    if (count_ < 2) {
        return 0; // Nothing to compact
    }
    
    // Compact by merging adjacent compatible commands
    size_t write_pos = tail_;
    size_t read_pos = tail_;
    size_t new_count = 0;
    
    while (read_pos != head_ && new_count < count_) {
        auto& current_cmd = history_[read_pos];
        
        // Try to merge with next command if possible
        size_t next_pos = nextIndex(read_pos);
        bool merged = false;
        
        if (next_pos != head_ && current_cmd && history_[next_pos]) {
            if (current_cmd->canMergeWith(*history_[next_pos])) {
                auto merge_result = current_cmd->mergeWith(std::move(history_[next_pos]));
                if (merge_result.has_value()) {
                    // Update memory usage
                    memory_usage_ -= current_cmd->getMemoryUsage();
                    memory_usage_ -= history_[next_pos]->getMemoryUsage();
                    
                    // Replace current with merged command
                    current_cmd = std::move(merge_result.value());
                    memory_usage_ += current_cmd->getMemoryUsage();
                    
                    // Clear the merged command
                    history_[next_pos].reset();
                    
                    // Skip the merged command in read position
                    read_pos = nextIndex(next_pos);
                    merged = true;
                }
            }
        }
        
        // Move command to write position if different
        if (write_pos != read_pos && current_cmd) {
            history_[write_pos] = std::move(current_cmd);
        }
        
        if (!merged) {
            read_pos = nextIndex(read_pos);
        }
        
        write_pos = nextIndex(write_pos);
        new_count++;
    }
    
    // Clear any remaining commands
    while (write_pos != head_) {
        if (history_[write_pos]) {
            memory_usage_ -= history_[write_pos]->getMemoryUsage();
            history_[write_pos].reset();
        }
        write_pos = nextIndex(write_pos);
    }
    
    // Update head and count
    head_ = write_pos;
    count_ = new_count;
    
    return original_count - count_;
}

void CommandHistory::addToHistory(std::unique_ptr<EditorCommand> command) {
    // If buffer is full, remove oldest
    if (count_ >= history_.size()) {
        removeOldest();
    }
    
    // Add new command
    memory_usage_ += command->getMemoryUsage();
    history_[head_] = std::move(command);
    head_ = nextIndex(head_);
    current_ = head_;
    count_++;
}

void CommandHistory::removeOldest() {
    if (count_ == 0) {
        return;
    }
    
    if (history_[tail_]) {
        memory_usage_ -= history_[tail_]->getMemoryUsage();
        history_[tail_].reset();
    }
    
    tail_ = nextIndex(tail_);
    count_--;
}

bool CommandHistory::tryMergeWithLast(std::unique_ptr<EditorCommand>& command) {
    if (count_ == 0) {
        return false;
    }
    
    size_t last_index = prevIndex(head_);
    auto& last_command = history_[last_index];
    
    if (!last_command || !last_command->canMergeWith(*command)) {
        return false;
    }
    
    // Attempt to merge
    auto merge_result = last_command->mergeWith(std::move(command));
    if (!merge_result) {
        return false;
    }
    
    // Update memory usage
    memory_usage_ -= last_command->getMemoryUsage();
    memory_usage_ += merge_result.value()->getMemoryUsage();
    
    // Replace last command with merged command
    last_command = std::move(merge_result.value());
    
    return true;
}

bool CommandHistory::isOverMemoryLimit() const {
    return memory_usage_ > config_.memory_limit_bytes;
}

void CommandHistory::enforceMemoryLimit() {
    while (isOverMemoryLimit() && count_ > 1) {
        removeOldest();
    }
}

EditorCommand* CommandHistory::getCommandAt(size_t index) const {
    if (index >= history_.size()) {
        return nullptr;
    }
    return history_[index].get();
}

size_t CommandHistory::nextIndex(size_t current) const {
    return (current + 1) % history_.size();
}

size_t CommandHistory::prevIndex(size_t current) const {
    return current == 0 ? history_.size() - 1 : current - 1;
}

// CommandFactory implementation

std::unique_ptr<EditorCommand> CommandFactory::createInsertChar(CursorPosition position, char ch) {
    return std::make_unique<InsertTextCommand>(position, std::string(1, ch));
}

std::unique_ptr<EditorCommand> CommandFactory::createInsertText(CursorPosition position, const std::string& text) {
    return std::make_unique<InsertTextCommand>(position, text);
}

std::unique_ptr<EditorCommand> CommandFactory::createDeleteChar(CursorPosition position, char deleted_char) {
    CursorPosition end_pos = position;
    end_pos.column++;
    return std::make_unique<DeleteTextCommand>(position, end_pos, std::string(1, deleted_char));
}

std::unique_ptr<EditorCommand> CommandFactory::createDeleteRange(
    CursorPosition start_position, CursorPosition end_position, const std::string& deleted_text) {
    return std::make_unique<DeleteTextCommand>(start_position, end_position, deleted_text);
}

std::unique_ptr<EditorCommand> CommandFactory::createReplaceText(
    CursorPosition start_position, CursorPosition end_position, 
    const std::string& old_text, const std::string& new_text) {
    return std::make_unique<ReplaceTextCommand>(start_position, end_position, old_text, new_text);
}

std::unique_ptr<EditorCommand> CommandFactory::createSplitLine(CursorPosition position) {
    return std::make_unique<InsertTextCommand>(position, "\n");
}

std::unique_ptr<EditorCommand> CommandFactory::createJoinLines(CursorPosition position, const std::string& separator) {
    // Create a join lines command that will properly handle line joining
    // This is a simple implementation that assumes the position is at the end of the line to join
    
    // Calculate the position of the newline character
    // The position parameter should point to the end of the current line
    CursorPosition newline_start = position;
    CursorPosition newline_end = position;
    newline_end.line += 1;
    newline_end.column = 0;
    
    if (separator.empty()) {
        // Standard join: just remove the newline (including any leading whitespace on next line)
        return std::make_unique<DeleteTextCommand>(newline_start, newline_end, "\n");
    } else {
        // Join with separator: replace newline with separator
        return std::make_unique<ReplaceTextCommand>(newline_start, newline_end, "\n", separator);
    }
}

} // namespace nx::tui