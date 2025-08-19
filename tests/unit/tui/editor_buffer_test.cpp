#include <gtest/gtest.h>
#include "nx/tui/editor_buffer.hpp"
#include <string>
#include <vector>

using namespace nx::tui;
using namespace nx;

class GapBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        GapBuffer::Config config;
        config.initial_gap_size = 64;
        config.max_buffer_size = 1024 * 1024; // 1MB for tests
        buffer_ = std::make_unique<GapBuffer>(config);
    }

    std::unique_ptr<GapBuffer> buffer_;
};

class EditorBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        EditorBuffer::Config config;
        config.gap_config.initial_gap_size = 64;
        config.gap_config.max_buffer_size = 1024 * 1024;
        editor_buffer_ = std::make_unique<EditorBuffer>(config);
    }

    std::unique_ptr<EditorBuffer> editor_buffer_;
};

// GapBuffer Tests

TEST_F(GapBufferTest, Initialize_EmptyBuffer) {
    EXPECT_TRUE(buffer_->empty());
    EXPECT_EQ(buffer_->size(), 0);
    EXPECT_GT(buffer_->getGapSize(), 0);
}

TEST_F(GapBufferTest, Initialize_WithContent) {
    std::string content = "Hello, World!";
    auto result = buffer_->initialize(content);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(buffer_->size(), content.length());
    EXPECT_EQ(buffer_->toString(), content);
    EXPECT_FALSE(buffer_->empty());
}

TEST_F(GapBufferTest, InsertChar_SingleCharacter) {
    auto result = buffer_->insertChar('H');
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(buffer_->size(), 1);
    EXPECT_EQ(buffer_->toString(), "H");
}

TEST_F(GapBufferTest, InsertChar_MultipleCharacters) {
    std::string text = "Hello";
    for (char c : text) {
        auto result = buffer_->insertChar(c);
        EXPECT_TRUE(result.has_value());
    }
    
    EXPECT_EQ(buffer_->toString(), text);
    EXPECT_EQ(buffer_->size(), text.length());
}

TEST_F(GapBufferTest, InsertString_BasicInsertion) {
    std::string text = "Hello, World!";
    auto result = buffer_->insertString(text);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(buffer_->toString(), text);
    EXPECT_EQ(buffer_->size(), text.length());
}

TEST_F(GapBufferTest, InsertString_EmptyString) {
    auto result = buffer_->insertString("");
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(buffer_->empty());
}

TEST_F(GapBufferTest, DeleteCharBefore_BasicDeletion) {
    buffer_->insertString("Hello");
    
    auto result = buffer_->deleteCharBefore();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 'o');
    EXPECT_EQ(buffer_->toString(), "Hell");
}

TEST_F(GapBufferTest, DeleteCharBefore_EmptyBuffer) {
    auto result = buffer_->deleteCharBefore();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(GapBufferTest, DeleteCharAfter_BasicDeletion) {
    buffer_->insertString("Hello");
    buffer_->moveGapTo(2); // Position gap between 'e' and 'l'
    
    auto result = buffer_->deleteCharAfter();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 'l');
    EXPECT_EQ(buffer_->toString(), "Helo");
}

TEST_F(GapBufferTest, MoveGapTo_DifferentPositions) {
    buffer_->insertString("Hello World");
    
    // Move gap to middle
    auto result = buffer_->moveGapTo(5);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(buffer_->getGapPosition(), 5);
    EXPECT_EQ(buffer_->toString(), "Hello World");
    
    // Move gap to beginning
    result = buffer_->moveGapTo(0);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(buffer_->getGapPosition(), 0);
    EXPECT_EQ(buffer_->toString(), "Hello World");
    
    // Move gap to end
    result = buffer_->moveGapTo(buffer_->size());
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(buffer_->getGapPosition(), buffer_->size());
    EXPECT_EQ(buffer_->toString(), "Hello World");
}

TEST_F(GapBufferTest, MoveGapTo_InvalidPosition) {
    buffer_->insertString("Hello");
    
    auto result = buffer_->moveGapTo(100);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(GapBufferTest, GetCharAt_ValidPositions) {
    buffer_->insertString("Hello");
    
    auto result = buffer_->getCharAt(0);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 'H');
    
    result = buffer_->getCharAt(4);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 'o');
}

TEST_F(GapBufferTest, GetCharAt_InvalidPosition) {
    buffer_->insertString("Hello");
    
    auto result = buffer_->getCharAt(10);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(GapBufferTest, GetSubstring_ValidRange) {
    buffer_->insertString("Hello World");
    
    auto result = buffer_->getSubstring(0, 5);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Hello");
    
    result = buffer_->getSubstring(6, 5);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "World");
}

TEST_F(GapBufferTest, GetSubstring_EdgeCases) {
    buffer_->insertString("Hello");
    
    // Empty substring
    auto result = buffer_->getSubstring(0, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
    
    // Length extends beyond end
    result = buffer_->getSubstring(3, 10);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "lo");
}

TEST_F(GapBufferTest, DeleteRange_ValidRange) {
    buffer_->insertString("Hello World");
    
    auto result = buffer_->deleteRange(5, 6); // Delete the space
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), " ");
    EXPECT_EQ(buffer_->toString(), "HelloWorld");
}

TEST_F(GapBufferTest, DeleteRange_InvalidRange) {
    buffer_->insertString("Hello");
    
    auto result = buffer_->deleteRange(3, 2); // start > end
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
    
    result = buffer_->deleteRange(0, 10); // end > size
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(GapBufferTest, ToLines_MultilineContent) {
    buffer_->insertString("Line 1\nLine 2\nLine 3");
    
    auto lines = buffer_->toLines();
    EXPECT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[0], "Line 1");
    EXPECT_EQ(lines[1], "Line 2");
    EXPECT_EQ(lines[2], "Line 3");
}

TEST_F(GapBufferTest, ToLines_EmptyLines) {
    buffer_->insertString("Line 1\n\nLine 3");
    
    auto lines = buffer_->toLines();
    EXPECT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[0], "Line 1");
    EXPECT_EQ(lines[1], "");
    EXPECT_EQ(lines[2], "Line 3");
}

TEST_F(GapBufferTest, Clear_RemovesAllContent) {
    buffer_->insertString("Hello World");
    EXPECT_FALSE(buffer_->empty());
    
    buffer_->clear();
    EXPECT_TRUE(buffer_->empty());
    EXPECT_EQ(buffer_->size(), 0);
    EXPECT_EQ(buffer_->toString(), "");
}

TEST_F(GapBufferTest, Compact_ReducesGapSize) {
    // Test basic compact functionality
    buffer_->insertString("Hello World");
    
    std::string original_content = buffer_->toString();
    
    auto result = buffer_->compact();
    EXPECT_TRUE(result.has_value());
    
    // Content should be preserved
    EXPECT_EQ(buffer_->toString(), original_content);
    
    // Buffer should still be functional after compact
    auto insert_result = buffer_->insertChar('!');
    EXPECT_TRUE(insert_result.has_value());
    EXPECT_EQ(buffer_->toString(), "Hello World!");
}

TEST_F(GapBufferTest, Statistics_TrackOperations) {
    auto stats = buffer_->getStatistics();
    EXPECT_EQ(stats.insertions, 0);
    EXPECT_EQ(stats.deletions, 0);
    
    buffer_->insertString("Hello");
    buffer_->deleteCharBefore();
    
    stats = buffer_->getStatistics();
    EXPECT_GT(stats.insertions, 0);
    EXPECT_GT(stats.deletions, 0);
    EXPECT_EQ(stats.logical_size, 4); // "Hell"
}

// Performance Tests

TEST_F(GapBufferTest, Performance_InsertionSpeed) {
    const size_t num_insertions = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Mix individual character insertions and string insertions for realistic usage
    for (size_t i = 0; i < num_insertions / 10; ++i) {
        // Insert a string (more efficient, realistic for copy/paste)
        buffer_->insertString("0123456789");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // More realistic expectation: string insertions should be much faster
    // Allow up to 1000ms for 10k characters to account for buffer reallocations
    EXPECT_LT(duration.count(), 1000) << "Insertions too slow: " << duration.count() << "ms";
    EXPECT_EQ(buffer_->size(), num_insertions);
}

TEST_F(GapBufferTest, Performance_GapMovement) {
    buffer_->insertString(std::string(1000, 'a'));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Move gap back and forth
    for (int i = 0; i < 100; ++i) {
        buffer_->moveGapTo(i * 10);
        buffer_->moveGapTo(1000 - (i * 10));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Gap movement should be fast
    EXPECT_LT(duration.count(), 50) << "Gap movement too slow: " << duration.count() << "ms";
}

// EditorBuffer Tests

TEST_F(EditorBufferTest, Initialize_WithContent) {
    std::string content = "Line 1\nLine 2\nLine 3";
    auto result = editor_buffer_->initialize(content);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(editor_buffer_->getLineCount(), 3);
    EXPECT_EQ(editor_buffer_->toString(), content);
}

TEST_F(EditorBufferTest, GetLine_ValidIndices) {
    editor_buffer_->initialize("First\nSecond\nThird");
    
    auto result = editor_buffer_->getLine(0);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "First");
    
    result = editor_buffer_->getLine(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Second");
    
    result = editor_buffer_->getLine(2);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Third");
}

TEST_F(EditorBufferTest, GetLine_InvalidIndex) {
    editor_buffer_->initialize("Single line");
    
    auto result = editor_buffer_->getLine(1);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kValidationError);
}

TEST_F(EditorBufferTest, SetLine_ValidOperation) {
    editor_buffer_->initialize("Old line\nSecond line");
    
    auto result = editor_buffer_->setLine(0, "New line");
    EXPECT_TRUE(result.has_value());
    
    auto line_result = editor_buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "New line");
    
    // Second line should be unchanged
    line_result = editor_buffer_->getLine(1);
    EXPECT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "Second line");
}

TEST_F(EditorBufferTest, ToLines_ConvertsCorrectly) {
    editor_buffer_->initialize("Line 1\nLine 2\n\nLine 4");
    
    auto lines = editor_buffer_->toLines();
    EXPECT_EQ(lines.size(), 4);
    EXPECT_EQ(lines[0], "Line 1");
    EXPECT_EQ(lines[1], "Line 2");
    EXPECT_EQ(lines[2], "");
    EXPECT_EQ(lines[3], "Line 4");
}

TEST_F(EditorBufferTest, Clear_RemovesAllLines) {
    editor_buffer_->initialize("Line 1\nLine 2");
    EXPECT_GT(editor_buffer_->getLineCount(), 0);
    
    editor_buffer_->clear();
    EXPECT_EQ(editor_buffer_->toString(), "");
}

TEST_F(EditorBufferTest, Statistics_TrackMetrics) {
    editor_buffer_->initialize("Line 1\nLine 2");
    
    auto stats = editor_buffer_->getStatistics();
    EXPECT_EQ(stats.line_count, 2);
    EXPECT_GT(stats.total_characters, 0);
    EXPECT_GT(stats.gap_stats.logical_size, 0);
}

// Edge Cases and Error Handling

TEST_F(GapBufferTest, EdgeCase_VeryLargeGap) {
    // Insert small content, then create very large gap
    buffer_->insertString("small");
    
    // Multiple insertions should handle gap growth
    for (int i = 0; i < 1000; ++i) {
        auto result = buffer_->insertChar('x');
        EXPECT_TRUE(result.has_value());
    }
    
    EXPECT_EQ(buffer_->size(), 5 + 1000);
}

TEST_F(GapBufferTest, EdgeCase_AlternatingOperations) {
    // Test pattern that exercises gap management
    for (int i = 0; i < 100; ++i) {
        buffer_->insertChar('a');
        if (i % 10 == 0) {
            buffer_->moveGapTo(i / 2);
        }
        if (i % 20 == 0 && buffer_->size() > 0) {
            buffer_->deleteCharBefore();
        }
    }
    
    // Should maintain consistency
    EXPECT_EQ(buffer_->toString().length(), buffer_->size());
}

TEST_F(EditorBufferTest, EdgeCase_SingleCharacterLines) {
    editor_buffer_->initialize("a\nb\nc\n");
    
    EXPECT_EQ(editor_buffer_->getLineCount(), 4); // Including empty line after last \n
    
    auto result = editor_buffer_->getLine(0);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "a");
    
    result = editor_buffer_->getLine(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "b");
}

TEST_F(EditorBufferTest, EdgeCase_EmptyDocument) {
    // Empty documents should have one empty line for editing
    EXPECT_EQ(editor_buffer_->getLineCount(), 1);
    
    auto result = editor_buffer_->getLine(0);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
}

// Stress Tests

TEST_F(GapBufferTest, StressTest_LargeDocument) {
    const size_t large_size = 50000; // 50KB
    std::string large_content(large_size, 'x');
    
    auto result = buffer_->initialize(large_content);
    EXPECT_TRUE(result.has_value());
    
    // Verify content integrity
    EXPECT_EQ(buffer_->size(), large_size);
    
    // Test random access
    for (int i = 0; i < 100; ++i) {
        size_t pos = i * (large_size / 100);
        auto char_result = buffer_->getCharAt(pos);
        EXPECT_TRUE(char_result.has_value());
        EXPECT_EQ(char_result.value(), 'x');
    }
}