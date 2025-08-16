#include <gtest/gtest.h>
#include "nx/tui/editor_commands.hpp"
#include "nx/tui/editor_buffer.hpp"
#include <memory>

using namespace nx::tui;
using namespace nx;

class EditorCommandsTest : public ::testing::Test {
protected:
    void SetUp() override {
        EditorBuffer::Config config;
        config.gap_config.initial_gap_size = 64;
        config.gap_config.max_buffer_size = 1024 * 1024;
        buffer_ = std::make_unique<EditorBuffer>(config);
        buffer_->initialize("Hello\nWorld\nTest");
        
        CommandHistory::Config history_config;
        history_config.auto_merge_commands = false; // Disable auto-merge for predictable testing
        history_ = std::make_unique<CommandHistory>(history_config);
    }

    void TearDown() override {
        history_.reset();
        buffer_.reset();
    }

    std::unique_ptr<EditorBuffer> buffer_;
    std::unique_ptr<CommandHistory> history_;
};

// CursorPosition Tests

TEST_F(EditorCommandsTest, CursorPosition_Validation) {
    CursorPosition pos(0, 5);
    auto result = pos.validate(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    // Test out of bounds line
    CursorPosition invalid_line(10, 0);
    result = invalid_line.validate(*buffer_);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
    
    // Test out of bounds column
    CursorPosition invalid_col(0, 100);
    result = invalid_col.validate(*buffer_);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(EditorCommandsTest, CursorPosition_Adjacent) {
    CursorPosition pos1(0, 5);
    CursorPosition pos2(0, 6);
    CursorPosition pos3(1, 0);
    CursorPosition pos4(0, 3);
    
    EXPECT_TRUE(pos1.isAdjacentTo(pos2));
    EXPECT_TRUE(pos2.isAdjacentTo(pos1));
    EXPECT_FALSE(pos1.isAdjacentTo(pos3));
    EXPECT_FALSE(pos1.isAdjacentTo(pos4));
}

// InsertTextCommand Tests

TEST_F(EditorCommandsTest, InsertTextCommand_Execute) {
    CursorPosition pos(0, 5);
    auto command = std::make_unique<InsertTextCommand>(pos, " there");
    
    auto result = command->execute(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello there");
}

TEST_F(EditorCommandsTest, InsertTextCommand_ExecuteUndo) {
    CursorPosition pos(0, 5);
    auto command = std::make_unique<InsertTextCommand>(pos, " there");
    
    // Execute
    auto result = command->execute(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello there");
    
    // Undo
    result = command->undo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello");
}

TEST_F(EditorCommandsTest, InsertTextCommand_WithNewline) {
    CursorPosition pos(0, 5);
    auto command = std::make_unique<InsertTextCommand>(pos, "\nNew line");
    
    auto result = command->execute(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    // Check first line
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello");
    
    // Check new line
    line_result = buffer_->getLine(1);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "New line");
    
    // Undo
    result = command->undo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello");
    
    // Check that we're back to original line count
    EXPECT_EQ(buffer_->getLineCount(), 3);
}

TEST_F(EditorCommandsTest, InsertTextCommand_CanMerge) {
    CursorPosition pos1(0, 5);
    CursorPosition pos2(0, 6);
    
    auto command1 = std::make_unique<InsertTextCommand>(pos1, "a");
    auto command2 = std::make_unique<InsertTextCommand>(pos2, "b");
    
    EXPECT_TRUE(command1->canMergeWith(*command2));
    
    // Test merge
    auto merge_result = command1->mergeWith(std::move(command2));
    EXPECT_TRUE(merge_result.has_value());
    
    auto merged_command = std::move(merge_result.value());
    
    // Execute merged command
    auto result = merged_command->execute(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Helloab");
}

// DeleteTextCommand Tests

TEST_F(EditorCommandsTest, DeleteTextCommand_Execute) {
    CursorPosition start(0, 0);
    CursorPosition end(0, 5);
    auto command = std::make_unique<DeleteTextCommand>(start, end, "Hello");
    
    auto result = command->execute(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "");
}

TEST_F(EditorCommandsTest, DeleteTextCommand_ExecuteUndo) {
    CursorPosition start(0, 0);
    CursorPosition end(0, 5);
    auto command = std::make_unique<DeleteTextCommand>(start, end, "Hello");
    
    // Execute
    auto result = command->execute(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    // Undo
    result = command->undo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello");
}

// CommandHistory Tests

TEST_F(EditorCommandsTest, CommandHistory_ExecuteUndo) {
    CursorPosition pos(0, 5);
    auto command = CommandFactory::createInsertText(pos, " there");
    
    auto result = history_->executeCommand(*buffer_, std::move(command));
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello there");
    
    EXPECT_TRUE(history_->canUndo());
    EXPECT_FALSE(history_->canRedo());
    
    // Undo
    result = history_->undo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello");
    
    EXPECT_FALSE(history_->canUndo());
    EXPECT_TRUE(history_->canRedo());
}

TEST_F(EditorCommandsTest, CommandHistory_ExecuteUndoRedo) {
    CursorPosition pos(0, 5);
    auto command = CommandFactory::createInsertText(pos, " there");
    
    // Execute
    auto result = history_->executeCommand(*buffer_, std::move(command));
    EXPECT_TRUE(result.has_value());
    
    // Undo
    result = history_->undo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello");
    
    // Redo
    result = history_->redo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Hello there");
}

TEST_F(EditorCommandsTest, CommandHistory_MultipleCommands) {
    // Execute multiple commands
    auto cmd1 = CommandFactory::createInsertText(CursorPosition(0, 5), " there");
    auto cmd2 = CommandFactory::createInsertChar(CursorPosition(0, 11), '!');  // "Hello there" = 11 chars, insert at end
    auto cmd3 = CommandFactory::createDeleteChar(CursorPosition(0, 0), 'H');
    
    auto result = history_->executeCommand(*buffer_, std::move(cmd1));
    EXPECT_TRUE(result.has_value());
    
    result = history_->executeCommand(*buffer_, std::move(cmd2));
    EXPECT_TRUE(result.has_value());
    
    result = history_->executeCommand(*buffer_, std::move(cmd3));
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "ello there!");
    
    // Undo all three
    EXPECT_TRUE(history_->canUndo());
    result = history_->undo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_EQ(line_result.value(), "Hello there!");
    
    result = history_->undo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_EQ(line_result.value(), "Hello there");
    
    result = history_->undo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_EQ(line_result.value(), "Hello");
    
    EXPECT_FALSE(history_->canUndo());
}

TEST_F(EditorCommandsTest, CommandHistory_AutoMerge) {
    CommandHistory::Config config;
    config.auto_merge_commands = true;
    auto history_with_merge = std::make_unique<CommandHistory>(config);
    
    // Execute two adjacent insert commands that should merge
    auto cmd1 = CommandFactory::createInsertChar(CursorPosition(0, 5), 'a');
    auto cmd2 = CommandFactory::createInsertChar(CursorPosition(0, 6), 'b');
    
    auto result = history_with_merge->executeCommand(*buffer_, std::move(cmd1));
    EXPECT_TRUE(result.has_value());
    
    result = history_with_merge->executeCommand(*buffer_, std::move(cmd2));
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Helloab");
    
    // Should be able to undo both with one operation due to merging
    EXPECT_TRUE(history_with_merge->canUndo());
    result = history_with_merge->undo(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_EQ(line_result.value(), "Hello");
    
    // Should not be able to undo further
    EXPECT_FALSE(history_with_merge->canUndo());
}

TEST_F(EditorCommandsTest, CommandHistory_MemoryLimit) {
    CommandHistory::Config config;
    config.memory_limit_bytes = 1000; // Very small limit
    config.max_history_size = 50;
    auto memory_limited_history = std::make_unique<CommandHistory>(config);
    
    // Execute many commands to test memory management
    for (int i = 0; i < 20; ++i) {
        std::string text = "text" + std::to_string(i);
        auto cmd = CommandFactory::createInsertText(CursorPosition(0, 5 + i * 5), text);
        auto result = memory_limited_history->executeCommand(*buffer_, std::move(cmd));
        EXPECT_TRUE(result.has_value());
    }
    
    auto stats = memory_limited_history->getStatistics();
    EXPECT_LE(stats.memory_usage, config.memory_limit_bytes * 2); // Allow some overhead
}

// CommandFactory Tests

TEST_F(EditorCommandsTest, CommandFactory_CreateInsertChar) {
    auto command = CommandFactory::createInsertChar(CursorPosition(0, 5), 'X');
    EXPECT_NE(command, nullptr);
    
    auto result = command->execute(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "HelloX");
}

TEST_F(EditorCommandsTest, CommandFactory_CreateDeleteChar) {
    auto command = CommandFactory::createDeleteChar(CursorPosition(0, 0), 'H');
    EXPECT_NE(command, nullptr);
    
    auto result = command->execute(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "ello");
}

TEST_F(EditorCommandsTest, CommandFactory_CreateSplitLine) {
    auto command = CommandFactory::createSplitLine(CursorPosition(0, 2));
    EXPECT_NE(command, nullptr);
    
    auto result = command->execute(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto line1_result = buffer_->getLine(0);
    EXPECT_TRUE(line1_result.has_value());
    EXPECT_EQ(line1_result.value(), "He");
    
    auto line2_result = buffer_->getLine(1);
    EXPECT_TRUE(line2_result.has_value());
    EXPECT_EQ(line2_result.value(), "llo");
}

// Performance Tests

TEST_F(EditorCommandsTest, Performance_LargeHistory) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Execute 1000 commands
    for (int i = 0; i < 1000; ++i) {
        auto cmd = CommandFactory::createInsertChar(CursorPosition(0, 5 + i), 'a');
        auto result = history_->executeCommand(*buffer_, std::move(cmd));
        EXPECT_TRUE(result.has_value());
    }
    
    auto execute_time = std::chrono::steady_clock::now();
    
    // Undo all commands
    while (history_->canUndo()) {
        auto result = history_->undo(*buffer_);
        EXPECT_TRUE(result.has_value());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto execute_duration = std::chrono::duration_cast<std::chrono::milliseconds>(execute_time - start_time);
    auto undo_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - execute_time);
    
    // Performance assertions (these are quite generous, adjust based on requirements)
    EXPECT_LT(total_time.count(), 1000);  // Should complete in under 1 second
    EXPECT_LT(execute_duration.count(), 500);  // Execute should be fast
    EXPECT_LT(undo_duration.count(), 500);     // Undo should be fast
    
    std::cout << "Performance Results:" << std::endl;
    std::cout << "  Total time: " << total_time.count() << "ms" << std::endl;
    std::cout << "  Execute time: " << execute_duration.count() << "ms" << std::endl;
    std::cout << "  Undo time: " << undo_duration.count() << "ms" << std::endl;
}