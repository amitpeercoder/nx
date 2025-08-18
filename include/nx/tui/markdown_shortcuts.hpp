#pragma once

#include <memory>
#include <string>
#include <functional>
#include "nx/common.hpp"
#include "nx/tui/editor_buffer.hpp"
#include "nx/tui/editor_commands.hpp"

namespace nx::tui {

/**
 * @brief Text selection for markdown formatting operations
 */
struct TextSelection {
    CursorPosition start;
    CursorPosition end;
    
    TextSelection() = default;
    TextSelection(CursorPosition s, CursorPosition e) : start(s), end(e) {}
    
    bool isEmpty() const {
        return start == end;
    }
    
    bool isValid() const {
        return start.line <= end.line && 
               (start.line != end.line || start.column <= end.column);
    }
    
    size_t getLength(const EditorBuffer& buffer) const;
    std::string getText(const EditorBuffer& buffer) const;
};

/**
 * @brief Markdown formatting types
 */
enum class MarkdownFormat {
    Bold,           // **text**
    Italic,         // *text*
    BoldItalic,     // ***text***
    InlineCode,     // `text`
    Strikethrough,  // ~~text~~
    Underline,      // __text__
    Link,           // [text](url)
    WikiLink        // [[text]]
};

/**
 * @brief Configuration for markdown shortcuts
 */
struct MarkdownShortcutConfig {
    bool enable_bold_shortcut = true;           // Ctrl+B
    bool enable_italic_shortcut = true;         // Ctrl+I
    bool enable_code_shortcut = true;           // Ctrl+`
    bool enable_link_shortcut = true;           // Ctrl+K
    bool enable_strikethrough_shortcut = true;  // Ctrl+Shift+X
    bool enable_underline_shortcut = true;      // Ctrl+U
    bool enable_wiki_link_shortcut = true;      // Ctrl+Shift+K
    
    // Auto-completion settings
    bool auto_close_brackets = true;
    bool auto_close_parentheses = true;
    bool auto_close_backticks = true;
    
    // Smart formatting settings
    bool smart_emphasis_detection = true;       // Detect existing emphasis and toggle
    bool preserve_selection_after_format = true;
    bool extend_word_boundaries = true;         // Extend to word boundaries if no selection
};

/**
 * @brief Result of markdown formatting operation
 */
struct MarkdownFormatResult {
    bool success = false;
    std::string error_message;
    CursorPosition new_cursor_position;
    TextSelection new_selection;
    bool selection_changed = false;
    
    // For undo operations
    std::string original_text;
    TextSelection original_selection;
    CursorPosition original_cursor;
};

/**
 * @brief Markdown shortcuts manager for TUI editor
 * 
 * Provides keyboard shortcuts and smart formatting for markdown text.
 * Supports wrapping selected text with markdown syntax and intelligent
 * detection of existing formatting for toggle behavior.
 */
class MarkdownShortcuts {
public:
    explicit MarkdownShortcuts(MarkdownShortcutConfig config = MarkdownShortcutConfig{});
    
    /**
     * @brief Apply markdown formatting to selection or current word
     * @param buffer Editor buffer to modify
     * @param format Type of markdown formatting to apply
     * @param selection Current text selection (can be empty)
     * @param cursor_position Current cursor position
     * @return Command to execute the formatting operation
     */
    Result<std::unique_ptr<EditorCommand>> formatSelection(
        const EditorBuffer& buffer,
        MarkdownFormat format,
        const TextSelection& selection,
        CursorPosition cursor_position) const;
    
    /**
     * @brief Toggle markdown formatting (add if not present, remove if present)
     * @param buffer Editor buffer to check/modify
     * @param format Type of markdown formatting to toggle
     * @param selection Current text selection
     * @param cursor_position Current cursor position
     * @return Command to execute the toggle operation
     */
    Result<std::unique_ptr<EditorCommand>> toggleFormat(
        const EditorBuffer& buffer,
        MarkdownFormat format,
        const TextSelection& selection,
        CursorPosition cursor_position) const;
    
    /**
     * @brief Create a markdown link with optional URL input
     * @param buffer Editor buffer to modify
     * @param selection Current text selection (will be link text)
     * @param cursor_position Current cursor position
     * @param url Optional URL (if empty, will insert placeholder)
     * @return Command to create the link
     */
    Result<std::unique_ptr<EditorCommand>> createLink(
        const EditorBuffer& buffer,
        const TextSelection& selection,
        CursorPosition cursor_position,
        const std::string& url = "") const;
    
    /**
     * @brief Create a wiki-style link
     * @param buffer Editor buffer to modify
     * @param selection Current text selection (will be link text)
     * @param cursor_position Current cursor position
     * @return Command to create the wiki link
     */
    Result<std::unique_ptr<EditorCommand>> createWikiLink(
        const EditorBuffer& buffer,
        const TextSelection& selection,
        CursorPosition cursor_position) const;
    
    /**
     * @brief Detect existing markdown formatting around cursor/selection
     * @param buffer Editor buffer to analyze
     * @param position Position to check around
     * @return Vector of detected formats
     */
    std::vector<MarkdownFormat> detectExistingFormats(
        const EditorBuffer& buffer,
        CursorPosition position) const;
    
    /**
     * @brief Extend selection to word boundaries if selection is empty
     * @param buffer Editor buffer to analyze
     * @param selection Current selection
     * @param cursor_position Current cursor position
     * @return Extended selection
     */
    TextSelection extendToWordBoundaries(
        const EditorBuffer& buffer,
        const TextSelection& selection,
        CursorPosition cursor_position) const;
    
    /**
     * @brief Check if character is a word boundary
     * @param ch Character to check
     * @return true if character is word boundary
     */
    static bool isWordBoundary(char ch);
    
    /**
     * @brief Get markdown delimiters for format type
     * @param format Format type
     * @return Pair of opening and closing delimiters
     */
    static std::pair<std::string, std::string> getDelimiters(MarkdownFormat format);
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void setConfig(const MarkdownShortcutConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const MarkdownShortcutConfig& getConfig() const;

private:
    MarkdownShortcutConfig config_;
    
    /**
     * @brief Apply formatting delimiters around text
     * @param buffer Editor buffer
     * @param selection Text selection to wrap
     * @param opening_delimiter Opening delimiter (e.g., "**")
     * @param closing_delimiter Closing delimiter (e.g., "**")
     * @return Formatting result
     */
    MarkdownFormatResult applyDelimiters(
        const EditorBuffer& buffer,
        const TextSelection& selection,
        const std::string& opening_delimiter,
        const std::string& closing_delimiter) const;
    
    /**
     * @brief Remove formatting delimiters from around text
     * @param buffer Editor buffer
     * @param selection Text selection to unwrap
     * @param opening_delimiter Opening delimiter to remove
     * @param closing_delimiter Closing delimiter to remove
     * @return Formatting result
     */
    MarkdownFormatResult removeDelimiters(
        const EditorBuffer& buffer,
        const TextSelection& selection,
        const std::string& opening_delimiter,
        const std::string& closing_delimiter) const;
    
    /**
     * @brief Check if text has specific delimiters around it
     * @param buffer Editor buffer
     * @param selection Text selection to check
     * @param opening_delimiter Opening delimiter
     * @param closing_delimiter Closing delimiter
     * @return true if delimiters are present
     */
    bool hasDelimitersAround(
        const EditorBuffer& buffer,
        const TextSelection& selection,
        const std::string& opening_delimiter,
        const std::string& closing_delimiter) const;
    
    /**
     * @brief Find word boundaries around position
     * @param buffer Editor buffer
     * @param position Starting position
     * @return Selection encompassing the word
     */
    TextSelection findWordAt(const EditorBuffer& buffer, CursorPosition position) const;
    
    /**
     * @brief Validate and normalize selection
     * @param buffer Editor buffer
     * @param selection Selection to validate
     * @return Normalized selection or error
     */
    Result<TextSelection> validateSelection(
        const EditorBuffer& buffer, 
        const TextSelection& selection) const;
    
    /**
     * @brief Calculate new cursor position after formatting
     * @param original_position Original cursor position
     * @param selection Selection that was formatted
     * @param delimiters_added Length of delimiters added
     * @param preserve_selection Whether to preserve selection
     * @return New cursor position and selection
     */
    std::pair<CursorPosition, TextSelection> calculateNewPosition(
        CursorPosition original_position,
        const TextSelection& selection,
        size_t delimiters_added,
        bool preserve_selection) const;
};

/**
 * @brief Markdown wrap command for undo/redo support
 */
class MarkdownWrapCommand : public EditorCommand {
public:
    MarkdownWrapCommand(
        TextSelection selection,
        std::string opening_delimiter,
        std::string closing_delimiter,
        std::string original_text);
    
    Result<void> execute(EditorBuffer& buffer) override;
    Result<void> undo(EditorBuffer& buffer) override;
    bool canMergeWith(const EditorCommand& other) const override;
    Result<std::unique_ptr<EditorCommand>> mergeWith(std::unique_ptr<EditorCommand> other) override;
    std::chrono::steady_clock::time_point getTimestamp() const override;
    std::string getDescription() const override;
    size_t getMemoryUsage() const override;

private:
    TextSelection selection_;
    std::string opening_delimiter_;
    std::string closing_delimiter_;
    std::string original_text_;
    std::chrono::steady_clock::time_point timestamp_;
    bool executed_;
};

/**
 * @brief Markdown unwrap command for removing formatting
 */
class MarkdownUnwrapCommand : public EditorCommand {
public:
    MarkdownUnwrapCommand(
        TextSelection selection,
        std::string opening_delimiter,
        std::string closing_delimiter,
        std::string formatted_text);
    
    Result<void> execute(EditorBuffer& buffer) override;
    Result<void> undo(EditorBuffer& buffer) override;
    bool canMergeWith(const EditorCommand& other) const override;
    Result<std::unique_ptr<EditorCommand>> mergeWith(std::unique_ptr<EditorCommand> other) override;
    std::chrono::steady_clock::time_point getTimestamp() const override;
    std::string getDescription() const override;
    size_t getMemoryUsage() const override;

private:
    TextSelection selection_;
    std::string opening_delimiter_;
    std::string closing_delimiter_;
    std::string formatted_text_;
    std::chrono::steady_clock::time_point timestamp_;
    bool executed_;
};

} // namespace nx::tui