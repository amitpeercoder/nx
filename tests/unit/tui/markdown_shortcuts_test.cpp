#include <gtest/gtest.h>
#include "nx/tui/markdown_shortcuts.hpp"
#include "nx/tui/editor_buffer.hpp"

using namespace nx::tui;

class MarkdownShortcutsTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = MarkdownShortcutConfig{};
        shortcuts_ = std::make_unique<MarkdownShortcuts>(config_);
        
        // Create a test buffer with some content
        buffer_ = std::make_unique<EditorBuffer>();
        std::string content = "This is a test line\nAnother line with some text\nBold and italic text here\n";
        buffer_->initialize(content);
    }
    
    MarkdownShortcutConfig config_;
    std::unique_ptr<MarkdownShortcuts> shortcuts_;
    std::unique_ptr<EditorBuffer> buffer_;
};

// TextSelection Tests

TEST_F(MarkdownShortcutsTest, TextSelectionBasics) {
    TextSelection sel1;
    EXPECT_TRUE(sel1.isEmpty());
    EXPECT_TRUE(sel1.isValid());
    
    TextSelection sel2(CursorPosition(0, 5), CursorPosition(0, 10));
    EXPECT_FALSE(sel2.isEmpty());
    EXPECT_TRUE(sel2.isValid());
    
    TextSelection sel3(CursorPosition(1, 5), CursorPosition(0, 10)); // Invalid
    EXPECT_FALSE(sel3.isValid());
}

TEST_F(MarkdownShortcutsTest, TextSelectionGetText) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7));
    std::string text = selection.getText(*buffer_);
    EXPECT_EQ(text, "is");
    
    TextSelection empty_selection;
    std::string empty_text = empty_selection.getText(*buffer_);
    EXPECT_EQ(empty_text, "");
}

TEST_F(MarkdownShortcutsTest, TextSelectionLength) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7));
    size_t length = selection.getLength(*buffer_);
    EXPECT_EQ(length, 2);
    
    TextSelection empty_selection;
    size_t empty_length = empty_selection.getLength(*buffer_);
    EXPECT_EQ(empty_length, 0);
}

// Delimiter Tests

TEST_F(MarkdownShortcutsTest, GetDelimiters) {
    auto bold_delims = MarkdownShortcuts::getDelimiters(MarkdownFormat::Bold);
    EXPECT_EQ(bold_delims.first, "**");
    EXPECT_EQ(bold_delims.second, "**");
    
    auto italic_delims = MarkdownShortcuts::getDelimiters(MarkdownFormat::Italic);
    EXPECT_EQ(italic_delims.first, "*");
    EXPECT_EQ(italic_delims.second, "*");
    
    auto code_delims = MarkdownShortcuts::getDelimiters(MarkdownFormat::InlineCode);
    EXPECT_EQ(code_delims.first, "`");
    EXPECT_EQ(code_delims.second, "`");
    
    auto wiki_delims = MarkdownShortcuts::getDelimiters(MarkdownFormat::WikiLink);
    EXPECT_EQ(wiki_delims.first, "[[");
    EXPECT_EQ(wiki_delims.second, "]]");
}

// Word Boundary Tests

TEST_F(MarkdownShortcutsTest, IsWordBoundary) {
    EXPECT_TRUE(MarkdownShortcuts::isWordBoundary(' '));
    EXPECT_TRUE(MarkdownShortcuts::isWordBoundary('\t'));
    EXPECT_TRUE(MarkdownShortcuts::isWordBoundary('\n'));
    EXPECT_TRUE(MarkdownShortcuts::isWordBoundary('.'));
    EXPECT_TRUE(MarkdownShortcuts::isWordBoundary(','));
    EXPECT_TRUE(MarkdownShortcuts::isWordBoundary('!'));
    
    EXPECT_FALSE(MarkdownShortcuts::isWordBoundary('a'));
    EXPECT_FALSE(MarkdownShortcuts::isWordBoundary('Z'));
    EXPECT_FALSE(MarkdownShortcuts::isWordBoundary('5'));
    EXPECT_FALSE(MarkdownShortcuts::isWordBoundary('_'));
}

TEST_F(MarkdownShortcutsTest, ExtendToWordBoundaries) {
    // Test with cursor in middle of word "test"
    CursorPosition pos(0, 12); // In "test"
    TextSelection empty_selection;
    
    auto extended = shortcuts_->extendToWordBoundaries(*buffer_, empty_selection, pos);
    EXPECT_EQ(extended.start.line, 0);
    EXPECT_EQ(extended.start.column, 10); // Start of "test"
    EXPECT_EQ(extended.end.line, 0);
    EXPECT_EQ(extended.end.column, 14);   // End of "test"
    
    // Test with existing selection - should remain unchanged
    TextSelection existing(CursorPosition(0, 5), CursorPosition(0, 7));
    auto unchanged = shortcuts_->extendToWordBoundaries(*buffer_, existing, pos);
    EXPECT_EQ(unchanged.start.line, 0);
    EXPECT_EQ(unchanged.start.column, 5);
    EXPECT_EQ(unchanged.end.line, 0);
    EXPECT_EQ(unchanged.end.column, 7);
}

// Format Selection Tests

TEST_F(MarkdownShortcutsTest, FormatSelectionBold) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7)); // "is"
    
    auto command_result = shortcuts_->formatSelection(
        *buffer_, MarkdownFormat::Bold, selection, CursorPosition(0, 6));
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    EXPECT_NE(command, nullptr);
    
    // Execute the command
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    // Check that text was wrapped with **
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    EXPECT_TRUE(line.find("**is**") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, FormatSelectionItalic) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7)); // "is"
    
    auto command_result = shortcuts_->formatSelection(
        *buffer_, MarkdownFormat::Italic, selection, CursorPosition(0, 6));
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    EXPECT_TRUE(line.find("*is*") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, FormatSelectionInlineCode) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7)); // "is"
    
    auto command_result = shortcuts_->formatSelection(
        *buffer_, MarkdownFormat::InlineCode, selection, CursorPosition(0, 6));
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    EXPECT_TRUE(line.find("`is`") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, FormatSelectionWikiLink) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7)); // "is"
    
    auto command_result = shortcuts_->formatSelection(
        *buffer_, MarkdownFormat::WikiLink, selection, CursorPosition(0, 6));
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    EXPECT_TRUE(line.find("[[is]]") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, FormatEmptySelectionWithWordExtension) {
    config_.extend_word_boundaries = true;
    shortcuts_->setConfig(config_);
    
    TextSelection empty_selection;
    CursorPosition cursor(0, 12); // In middle of "test"
    
    auto command_result = shortcuts_->formatSelection(
        *buffer_, MarkdownFormat::Bold, empty_selection, cursor);
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    EXPECT_TRUE(line.find("**test**") != std::string::npos);
}

// Toggle Format Tests

TEST_F(MarkdownShortcutsTest, ToggleFormatAddBold) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7)); // "is"
    
    auto command_result = shortcuts_->toggleFormat(
        *buffer_, MarkdownFormat::Bold, selection, CursorPosition(0, 6));
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    EXPECT_TRUE(line.find("**is**") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, ToggleFormatRemoveBold) {
    // First add bold formatting manually
    auto line_result = buffer_->getLine(0);
    ASSERT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    std::string new_line = line.substr(0, 5) + "**is**" + line.substr(7);
    buffer_->setLine(0, new_line);
    
    // Now try to toggle (remove) it
    TextSelection selection(CursorPosition(0, 7), CursorPosition(0, 9)); // The "is" part
    
    auto command_result = shortcuts_->toggleFormat(
        *buffer_, MarkdownFormat::Bold, selection, CursorPosition(0, 8));
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result_after = buffer_->getLine(0);
    EXPECT_TRUE(line_result_after.has_value());
    std::string line_after = line_result_after.value();
    // Should have removed the ** delimiters
    EXPECT_TRUE(line_after.find("is") != std::string::npos);
    EXPECT_TRUE(line_after.find("**is**") == std::string::npos);
}

// Create Link Tests

TEST_F(MarkdownShortcutsTest, CreateLink) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7)); // "is"
    
    auto command_result = shortcuts_->createLink(
        *buffer_, selection, CursorPosition(0, 6), "https://example.com");
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result3 = buffer_->getLine(0);
    EXPECT_TRUE(line_result3.has_value());
    std::string line3 = line_result3.value();
    EXPECT_TRUE(line3.find("[is](https://example.com)") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, CreateLinkWithoutURL) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7)); // "is"
    
    auto command_result = shortcuts_->createLink(
        *buffer_, selection, CursorPosition(0, 6), "");
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result4 = buffer_->getLine(0);
    EXPECT_TRUE(line_result4.has_value());
    std::string line4 = line_result4.value();
    EXPECT_TRUE(line4.find("[is](url)") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, CreateLinkEmptySelection) {
    TextSelection empty_selection;
    CursorPosition cursor(0, 5);
    
    auto command_result = shortcuts_->createLink(
        *buffer_, empty_selection, cursor, "https://example.com");
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    EXPECT_TRUE(line.find("[link text](https://example.com)") != std::string::npos);
}

// Create Wiki Link Tests

TEST_F(MarkdownShortcutsTest, CreateWikiLink) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7)); // "is"
    
    auto command_result = shortcuts_->createWikiLink(*buffer_, selection, CursorPosition(0, 6));
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    EXPECT_TRUE(line.find("[[is]]") != std::string::npos);
}

// Detect Existing Formats Tests

TEST_F(MarkdownShortcutsTest, DetectExistingFormats) {
    // Set up buffer with various markdown formats
    buffer_->setLine(0, "This **is** a *test* with `code` and [[wiki]]");
    
    // Test detection within bold text
    auto formats_bold = shortcuts_->detectExistingFormats(*buffer_, CursorPosition(0, 7));
    EXPECT_TRUE(std::find(formats_bold.begin(), formats_bold.end(), MarkdownFormat::Bold) != formats_bold.end());
    
    // Test detection within italic text
    auto formats_italic = shortcuts_->detectExistingFormats(*buffer_, CursorPosition(0, 15));
    EXPECT_TRUE(std::find(formats_italic.begin(), formats_italic.end(), MarkdownFormat::Italic) != formats_italic.end());
    
    // Test detection within code
    auto formats_code = shortcuts_->detectExistingFormats(*buffer_, CursorPosition(0, 27));
    EXPECT_TRUE(std::find(formats_code.begin(), formats_code.end(), MarkdownFormat::InlineCode) != formats_code.end());
    
    // Test detection within wiki link
    auto formats_wiki = shortcuts_->detectExistingFormats(*buffer_, CursorPosition(0, 40));
    EXPECT_TRUE(std::find(formats_wiki.begin(), formats_wiki.end(), MarkdownFormat::WikiLink) != formats_wiki.end());
}

// Configuration Tests

TEST_F(MarkdownShortcutsTest, ConfigurationDisabledShortcuts) {
    config_.enable_bold_shortcut = false;
    shortcuts_->setConfig(config_);
    
    const auto& retrieved_config = shortcuts_->getConfig();
    EXPECT_FALSE(retrieved_config.enable_bold_shortcut);
    EXPECT_TRUE(retrieved_config.enable_italic_shortcut); // Should still be enabled
}

TEST_F(MarkdownShortcutsTest, ConfigurationWordBoundaries) {
    config_.extend_word_boundaries = false;
    shortcuts_->setConfig(config_);
    
    TextSelection empty_selection;
    CursorPosition cursor(0, 12); // In middle of "test"
    
    // Should not extend to word boundaries
    auto extended = shortcuts_->extendToWordBoundaries(*buffer_, empty_selection, cursor);
    EXPECT_TRUE(extended.isEmpty()); // Should remain empty
}

// Command Tests

TEST_F(MarkdownShortcutsTest, MarkdownWrapCommandUndo) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7)); // "is"
    std::string original_text = selection.getText(*buffer_);
    
    MarkdownWrapCommand command(selection, "**", "**", original_text);
    
    // Execute
    auto exec_result = command.execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line_after = line_result.value();
    EXPECT_TRUE(line_after.find("**is**") != std::string::npos);
    
    // Undo
    auto undo_result = command.undo(*buffer_);
    EXPECT_TRUE(undo_result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line_undone = line_result.value();
    EXPECT_TRUE(line_undone.find("**is**") == std::string::npos);
    EXPECT_TRUE(line_undone.find("is") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, MarkdownUnwrapCommandUndo) {
    // First add bold formatting
    auto line_result_unwrap = buffer_->getLine(0);
    ASSERT_TRUE(line_result_unwrap.has_value());
    std::string line_unwrap = line_result_unwrap.value();
    std::string new_line_unwrap = line_unwrap.substr(0, 5) + "**is**" + line_unwrap.substr(7);
    buffer_->setLine(0, new_line_unwrap);
    
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 11)); // "**is**"
    std::string formatted_text = selection.getText(*buffer_);
    
    MarkdownUnwrapCommand command(selection, "**", "**", formatted_text);
    
    // Execute (remove formatting)
    auto exec_result = command.execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result_after_unwrap = buffer_->getLine(0);
    EXPECT_TRUE(line_result_after_unwrap.has_value());
    std::string line_after_unwrap = line_result_after_unwrap.value();
    EXPECT_TRUE(line_after_unwrap.find("**is**") == std::string::npos);
    EXPECT_TRUE(line_after_unwrap.find("is") != std::string::npos);
    
    // Undo (restore formatting)
    auto undo_result = command.undo(*buffer_);
    EXPECT_TRUE(undo_result.has_value());
    
    auto line_result_undo_unwrap = buffer_->getLine(0);
    EXPECT_TRUE(line_result_undo_unwrap.has_value());
    std::string line_undone_unwrap = line_result_undo_unwrap.value();
    EXPECT_TRUE(line_undone_unwrap.find("**is**") != std::string::npos);
}

// Edge Cases Tests

TEST_F(MarkdownShortcutsTest, FormatAtEndOfLine) {
    TextSelection selection(CursorPosition(0, 14), CursorPosition(0, 18)); // "line"
    
    auto command_result = shortcuts_->formatSelection(
        *buffer_, MarkdownFormat::Bold, selection, CursorPosition(0, 16));
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line = line_result.value();
    EXPECT_TRUE(line.find("**line**") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, FormatEntireLine) {
    auto line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string original_line = line_result.value();
    
    TextSelection selection(CursorPosition(0, 0), CursorPosition(0, original_line.length() - 1)); // Exclude newline
    
    auto command_result = shortcuts_->formatSelection(
        *buffer_, MarkdownFormat::Italic, selection, CursorPosition(0, 10));
    
    EXPECT_TRUE(command_result.has_value());
    auto command = std::move(command_result.value());
    
    auto exec_result = command->execute(*buffer_);
    EXPECT_TRUE(exec_result.has_value());
    
    line_result = buffer_->getLine(0);
    EXPECT_TRUE(line_result.has_value());
    std::string line_after = line_result.value();
    EXPECT_TRUE(line_after.find("*This is a test line*") != std::string::npos);
}

TEST_F(MarkdownShortcutsTest, InvalidSelection) {
    TextSelection invalid_selection(CursorPosition(0, 20), CursorPosition(0, 10)); // End before start
    
    auto command_result = shortcuts_->formatSelection(
        *buffer_, MarkdownFormat::Bold, invalid_selection, CursorPosition(0, 15));
    
    EXPECT_FALSE(command_result.has_value());
}

TEST_F(MarkdownShortcutsTest, SelectionBeyondBuffer) {
    TextSelection out_of_bounds(CursorPosition(10, 0), CursorPosition(10, 5)); // Line doesn't exist
    
    auto command_result = shortcuts_->formatSelection(
        *buffer_, MarkdownFormat::Bold, out_of_bounds, CursorPosition(10, 2));
    
    EXPECT_FALSE(command_result.has_value());
}

// Memory and Performance Tests

TEST_F(MarkdownShortcutsTest, CommandMemoryUsage) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7));
    MarkdownWrapCommand command(selection, "**", "**", "is");
    
    size_t memory_usage = command.getMemoryUsage();
    EXPECT_GT(memory_usage, 0);
    EXPECT_LT(memory_usage, 1000); // Should be reasonable
}

TEST_F(MarkdownShortcutsTest, CommandDescription) {
    TextSelection selection(CursorPosition(0, 5), CursorPosition(0, 7));
    MarkdownWrapCommand wrap_command(selection, "**", "**", "is");
    MarkdownUnwrapCommand unwrap_command(selection, "**", "**", "**is**");
    
    std::string wrap_desc = wrap_command.getDescription();
    std::string unwrap_desc = unwrap_command.getDescription();
    
    EXPECT_FALSE(wrap_desc.empty());
    EXPECT_FALSE(unwrap_desc.empty());
    EXPECT_TRUE(wrap_desc.find("**") != std::string::npos);
    EXPECT_TRUE(unwrap_desc.find("**") != std::string::npos);
}