#include <gtest/gtest.h>
#include "nx/tui/enhanced_cursor.hpp"
#include "nx/tui/editor_buffer.hpp"

using namespace nx::tui;
using namespace nx;

class EnhancedCursorTest : public ::testing::Test {
protected:
    void SetUp() override {
        EditorBuffer::Config buffer_config;
        buffer_config.gap_config.initial_gap_size = 64;
        buffer_config.gap_config.max_buffer_size = 1024 * 1024;
        buffer_ = std::make_unique<EditorBuffer>(buffer_config);
        buffer_->initialize("Hello World\nSecond Line\nThird Line");
        
        EnhancedCursor::Config cursor_config;
        cursor_ = std::make_unique<EnhancedCursor>(cursor_config);
        cursor_->initialize(*buffer_);
    }

    std::unique_ptr<EditorBuffer> buffer_;
    std::unique_ptr<EnhancedCursor> cursor_;
};

TEST_F(EnhancedCursorTest, InitialPosition) {
    auto pos = cursor_->getPosition();
    EXPECT_EQ(pos.line, 0);
    EXPECT_EQ(pos.column, 0);
}

TEST_F(EnhancedCursorTest, BasicMovement) {
    // Move right
    auto result = cursor_->move(EnhancedCursor::Direction::Right);
    EXPECT_TRUE(result.has_value());
    
    auto pos = cursor_->getPosition();
    EXPECT_EQ(pos.line, 0);
    EXPECT_EQ(pos.column, 1);
    
    // Move down
    result = cursor_->move(EnhancedCursor::Direction::Down);
    EXPECT_TRUE(result.has_value());
    
    pos = cursor_->getPosition();
    EXPECT_EQ(pos.line, 1);
    EXPECT_EQ(pos.column, 1);
}

TEST_F(EnhancedCursorTest, LineNavigation) {
    // Move to end of line
    auto result = cursor_->move(EnhancedCursor::Direction::End);
    EXPECT_TRUE(result.has_value());
    
    auto pos = cursor_->getPosition();
    EXPECT_EQ(pos.line, 0);
    EXPECT_EQ(pos.column, 11); // "Hello World" is 11 characters
    
    // Move to beginning of line
    result = cursor_->move(EnhancedCursor::Direction::Home);
    EXPECT_TRUE(result.has_value());
    
    pos = cursor_->getPosition();
    EXPECT_EQ(pos.line, 0);
    EXPECT_EQ(pos.column, 0);
}

TEST_F(EnhancedCursorTest, DocumentNavigation) {
    // Move to document end
    auto result = cursor_->move(EnhancedCursor::Direction::DocumentEnd);
    EXPECT_TRUE(result.has_value());
    
    auto pos = cursor_->getPosition();
    EXPECT_EQ(pos.line, 2);
    EXPECT_EQ(pos.column, 10); // "Third Line" is 10 characters
    
    // Move to document start
    result = cursor_->move(EnhancedCursor::Direction::DocumentHome);
    EXPECT_TRUE(result.has_value());
    
    pos = cursor_->getPosition();
    EXPECT_EQ(pos.line, 0);
    EXPECT_EQ(pos.column, 0);
}

TEST_F(EnhancedCursorTest, BoundsChecking) {
    auto bounds = cursor_->getBounds();
    EXPECT_EQ(bounds.total_lines, 3);
    EXPECT_EQ(bounds.max_line, 2);
    
    EXPECT_TRUE(cursor_->isAtDocumentStart());
    EXPECT_FALSE(cursor_->isAtDocumentEnd());
    EXPECT_TRUE(cursor_->isAtLineStart());
    EXPECT_FALSE(cursor_->isAtLineEnd());
}

TEST_F(EnhancedCursorTest, Selection) {
    // Start selection
    auto result = cursor_->startSelection();
    EXPECT_TRUE(result.has_value());
    
    // Move cursor to create selection
    cursor_->move(EnhancedCursor::Direction::Right, true);
    cursor_->move(EnhancedCursor::Direction::Right, true);
    
    auto selection = cursor_->getSelection();
    EXPECT_TRUE(selection.active);
    EXPECT_EQ(selection.start.line, 0);
    EXPECT_EQ(selection.start.column, 0);
    EXPECT_EQ(selection.end.line, 0);
    EXPECT_EQ(selection.end.column, 2);
    
    // Get selected text
    auto selected_text = cursor_->getSelectedText();
    EXPECT_TRUE(selected_text.has_value());
    EXPECT_EQ(selected_text.value(), "He");
}

TEST_F(EnhancedCursorTest, WordSelection) {
    // Move to middle of first word
    cursor_->setPosition(0, 2); // Position in "Hello"
    
    auto result = cursor_->selectWord();
    EXPECT_TRUE(result.has_value());
    
    auto selection = cursor_->getSelection();
    EXPECT_TRUE(selection.active);
    EXPECT_EQ(selection.mode, EnhancedCursor::SelectionMode::Word);
    EXPECT_EQ(selection.start.column, 0);
    EXPECT_EQ(selection.end.column, 5); // "Hello" is 5 characters
}

TEST_F(EnhancedCursorTest, LineSelection) {
    auto result = cursor_->selectLine();
    EXPECT_TRUE(result.has_value());
    
    auto selection = cursor_->getSelection();
    EXPECT_TRUE(selection.active);
    EXPECT_EQ(selection.mode, EnhancedCursor::SelectionMode::Line);
    EXPECT_EQ(selection.start.column, 0);
    EXPECT_EQ(selection.end.column, 11); // "Hello World" length
}

TEST_F(EnhancedCursorTest, CursorManager) {
    CursorManager manager;
    auto result = manager.initialize(*buffer_);
    EXPECT_TRUE(result.has_value());
    
    auto& primary = manager.getPrimaryCursor();
    auto pos = primary.getPosition();
    EXPECT_EQ(pos.line, 0);
    EXPECT_EQ(pos.column, 0);
}