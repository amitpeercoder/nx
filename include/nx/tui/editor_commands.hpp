#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <array>
#include "nx/common.hpp"
#include "nx/tui/editor_buffer.hpp"

namespace nx::tui {

/**
 * @brief Abstract base class for editor commands implementing Command Pattern
 * 
 * Provides a memory-efficient, type-safe undo/redo system using the Command Pattern.
 * All editor operations are implemented as commands that can be executed, undone,
 * and merged for optimal memory usage.
 */
class EditorCommand {
public:
    virtual ~EditorCommand() = default;

    /**
     * @brief Execute the command
     * @param buffer Editor buffer to operate on
     * @return Result indicating success or error
     */
    virtual Result<void> execute(EditorBuffer& buffer) = 0;

    /**
     * @brief Undo the command
     * @param buffer Editor buffer to operate on
     * @return Result indicating success or error
     */
    virtual Result<void> undo(EditorBuffer& buffer) = 0;

    /**
     * @brief Check if this command can be merged with another
     * @param other Command to potentially merge with
     * @return true if commands can be merged
     */
    virtual bool canMergeWith(const EditorCommand& other) const = 0;

    /**
     * @brief Merge this command with another compatible command
     * @param other Command to merge with
     * @return Result with merged command or error
     */
    virtual Result<std::unique_ptr<EditorCommand>> mergeWith(std::unique_ptr<EditorCommand> other) = 0;

    /**
     * @brief Get command timestamp
     * @return Timestamp when command was created
     */
    virtual std::chrono::steady_clock::time_point getTimestamp() const = 0;

    /**
     * @brief Get command description for debugging
     * @return Human-readable command description
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief Get memory usage of this command
     * @return Approximate memory usage in bytes
     */
    virtual size_t getMemoryUsage() const = 0;

protected:
    /**
     * @brief Check if two timestamps are within merge threshold
     * @param t1 First timestamp
     * @param t2 Second timestamp
     * @param threshold Maximum time difference for merging
     * @return true if timestamps are within threshold
     */
    static bool isWithinMergeThreshold(
        std::chrono::steady_clock::time_point t1,
        std::chrono::steady_clock::time_point t2,
        std::chrono::milliseconds threshold = std::chrono::milliseconds(1000));
};

/**
 * @brief Cursor position for command operations
 */
struct CursorPosition {
    size_t line;
    size_t column;

    CursorPosition() : line(0), column(0) {}
    CursorPosition(size_t l, size_t c) : line(l), column(c) {}

    bool operator==(const CursorPosition& other) const {
        return line == other.line && column == other.column;
    }

    bool operator!=(const CursorPosition& other) const {
        return !(*this == other);
    }

    /**
     * @brief Check if position is adjacent to another
     * @param other Position to compare with
     * @return true if positions are adjacent
     */
    bool isAdjacentTo(const CursorPosition& other) const;

    /**
     * @brief Validate position against buffer bounds
     * @param buffer Buffer to validate against
     * @return Result indicating validity
     */
    Result<void> validate(const EditorBuffer& buffer) const;
};

/**
 * @brief Insert text command
 */
class InsertTextCommand : public EditorCommand {
public:
    InsertTextCommand(CursorPosition position, std::string text);

    Result<void> execute(EditorBuffer& buffer) override;
    Result<void> undo(EditorBuffer& buffer) override;
    bool canMergeWith(const EditorCommand& other) const override;
    Result<std::unique_ptr<EditorCommand>> mergeWith(std::unique_ptr<EditorCommand> other) override;
    std::chrono::steady_clock::time_point getTimestamp() const override;
    std::string getDescription() const override;
    size_t getMemoryUsage() const override;

private:
    CursorPosition position_;
    std::string text_;
    std::chrono::steady_clock::time_point timestamp_;
    
    // For command merging optimization
    static constexpr size_t MAX_MERGE_LENGTH = 100;
    static constexpr auto MERGE_TIME_THRESHOLD = std::chrono::milliseconds(1000);
};

/**
 * @brief Delete text command
 */
class DeleteTextCommand : public EditorCommand {
public:
    DeleteTextCommand(CursorPosition start_position, CursorPosition end_position, std::string deleted_text);

    Result<void> execute(EditorBuffer& buffer) override;
    Result<void> undo(EditorBuffer& buffer) override;
    bool canMergeWith(const EditorCommand& other) const override;
    Result<std::unique_ptr<EditorCommand>> mergeWith(std::unique_ptr<EditorCommand> other) override;
    std::chrono::steady_clock::time_point getTimestamp() const override;
    std::string getDescription() const override;
    size_t getMemoryUsage() const override;

private:
    CursorPosition start_position_;
    CursorPosition end_position_;
    std::string deleted_text_;
    std::chrono::steady_clock::time_point timestamp_;
};

/**
 * @brief Replace text command (combination of delete + insert)
 */
class ReplaceTextCommand : public EditorCommand {
public:
    ReplaceTextCommand(CursorPosition start_position, CursorPosition end_position, 
                      std::string old_text, std::string new_text);

    Result<void> execute(EditorBuffer& buffer) override;
    Result<void> undo(EditorBuffer& buffer) override;
    bool canMergeWith(const EditorCommand& other) const override;
    Result<std::unique_ptr<EditorCommand>> mergeWith(std::unique_ptr<EditorCommand> other) override;
    std::chrono::steady_clock::time_point getTimestamp() const override;
    std::string getDescription() const override;
    size_t getMemoryUsage() const override;

private:
    CursorPosition start_position_;
    CursorPosition end_position_;
    std::string old_text_;
    std::string new_text_;
    std::chrono::steady_clock::time_point timestamp_;
};

/**
 * @brief Merged command that combines multiple commands for memory efficiency
 */
class MergedCommand : public EditorCommand {
public:
    MergedCommand(std::unique_ptr<EditorCommand> first, std::unique_ptr<EditorCommand> second);

    Result<void> execute(EditorBuffer& buffer) override;
    Result<void> undo(EditorBuffer& buffer) override;
    bool canMergeWith(const EditorCommand& other) const override;
    Result<std::unique_ptr<EditorCommand>> mergeWith(std::unique_ptr<EditorCommand> other) override;
    std::chrono::steady_clock::time_point getTimestamp() const override;
    std::string getDescription() const override;
    size_t getMemoryUsage() const override;

private:
    std::vector<std::unique_ptr<EditorCommand>> commands_;
    std::chrono::steady_clock::time_point earliest_timestamp_;
    std::chrono::steady_clock::time_point latest_timestamp_;
};

/**
 * @brief Memory-efficient command history manager
 * 
 * Manages undo/redo history with circular buffer to prevent memory exhaustion.
 * Automatically merges compatible commands to reduce memory usage.
 */
class CommandHistory {
public:
    /**
     * @brief Configuration for command history
     */
    struct Config {
        size_t max_history_size;        // Maximum number of commands to keep
        size_t memory_limit_bytes;      // Maximum memory usage for history
        bool auto_merge_commands;       // Automatically merge compatible commands
        std::chrono::milliseconds merge_timeout; // Timeout for command merging

        Config() 
            : max_history_size(100)
            , memory_limit_bytes(10 * 1024 * 1024) // 10MB
            , auto_merge_commands(true)
            , merge_timeout(1000) {}
    };

    explicit CommandHistory(Config config = Config{});
    ~CommandHistory();

    /**
     * @brief Execute and add command to history
     * @param buffer Buffer to execute command on
     * @param command Command to execute
     * @return Result indicating success or error
     */
    Result<void> executeCommand(EditorBuffer& buffer, std::unique_ptr<EditorCommand> command);

    /**
     * @brief Undo last command
     * @param buffer Buffer to undo command on
     * @return Result indicating success or error
     */
    Result<void> undo(EditorBuffer& buffer);

    /**
     * @brief Redo last undone command
     * @param buffer Buffer to redo command on
     * @return Result indicating success or error
     */
    Result<void> redo(EditorBuffer& buffer);

    /**
     * @brief Check if undo is available
     * @return true if undo operation is possible
     */
    bool canUndo() const;

    /**
     * @brief Check if redo is available
     * @return true if redo operation is possible
     */
    bool canRedo() const;

    /**
     * @brief Clear all command history
     */
    void clear();

    /**
     * @brief Get current memory usage
     * @return Memory usage in bytes
     */
    size_t getMemoryUsage() const;

    /**
     * @brief Get history statistics
     */
    struct Statistics {
        size_t total_commands;
        size_t undo_commands;
        size_t redo_commands;
        size_t merged_commands;
        size_t memory_usage;
        size_t memory_limit;
        double memory_utilization;
    };

    Statistics getStatistics() const;

    /**
     * @brief Compact history to reduce memory usage
     * @return Number of commands merged/removed
     */
    size_t compactHistory();

private:
    Config config_;
    std::array<std::unique_ptr<EditorCommand>, 200> history_;  // Circular buffer
    size_t head_;           // Points to next insertion position
    size_t tail_;           // Points to oldest command
    size_t current_;        // Current position for undo/redo
    size_t count_;          // Number of commands in history
    size_t memory_usage_;   // Current memory usage

    /**
     * @brief Add command to circular buffer
     * @param command Command to add
     */
    void addToHistory(std::unique_ptr<EditorCommand> command);

    /**
     * @brief Remove oldest command from history
     */
    void removeOldest();

    /**
     * @brief Try to merge command with last command in history
     * @param command Command to potentially merge
     * @return true if command was merged
     */
    bool tryMergeWithLast(std::unique_ptr<EditorCommand>& command);

    /**
     * @brief Check if memory limit is exceeded
     * @return true if over memory limit
     */
    bool isOverMemoryLimit() const;

    /**
     * @brief Reduce memory usage by merging or removing commands
     */
    void enforceMemoryLimit();

    /**
     * @brief Get command at specific index in circular buffer
     * @param index Index in circular buffer
     * @return Pointer to command or nullptr
     */
    EditorCommand* getCommandAt(size_t index) const;

    /**
     * @brief Calculate next index in circular buffer
     * @param current Current index
     * @return Next index
     */
    size_t nextIndex(size_t current) const;

    /**
     * @brief Calculate previous index in circular buffer
     * @param current Current index
     * @return Previous index
     */
    size_t prevIndex(size_t current) const;
};

/**
 * @brief Factory for creating editor commands
 */
class CommandFactory {
public:
    /**
     * @brief Create insert character command
     * @param position Position to insert at
     * @param ch Character to insert
     * @return Unique pointer to command
     */
    static std::unique_ptr<EditorCommand> createInsertChar(CursorPosition position, char ch);

    /**
     * @brief Create insert text command
     * @param position Position to insert at
     * @param text Text to insert
     * @return Unique pointer to command
     */
    static std::unique_ptr<EditorCommand> createInsertText(CursorPosition position, const std::string& text);

    /**
     * @brief Create delete character command
     * @param position Position to delete from
     * @param deleted_char Character that was deleted
     * @return Unique pointer to command
     */
    static std::unique_ptr<EditorCommand> createDeleteChar(CursorPosition position, char deleted_char);

    /**
     * @brief Create delete range command
     * @param start_position Start of range to delete
     * @param end_position End of range to delete
     * @param deleted_text Text that was deleted
     * @return Unique pointer to command
     */
    static std::unique_ptr<EditorCommand> createDeleteRange(
        CursorPosition start_position, CursorPosition end_position, const std::string& deleted_text);

    /**
     * @brief Create replace text command
     * @param start_position Start of range to replace
     * @param end_position End of range to replace
     * @param old_text Text being replaced
     * @param new_text New text
     * @return Unique pointer to command
     */
    static std::unique_ptr<EditorCommand> createReplaceText(
        CursorPosition start_position, CursorPosition end_position, 
        const std::string& old_text, const std::string& new_text);

    /**
     * @brief Create split line command
     * @param position Position to split line at
     * @return Unique pointer to command
     */
    static std::unique_ptr<EditorCommand> createSplitLine(CursorPosition position);

    /**
     * @brief Create join lines command
     * @param position Position of first line to join
     * @param separator Separator text that was removed
     * @return Unique pointer to command
     */
    static std::unique_ptr<EditorCommand> createJoinLines(CursorPosition position, const std::string& separator);
};

} // namespace nx::tui