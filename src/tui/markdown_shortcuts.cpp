#include "nx/tui/markdown_shortcuts.hpp"
#include <algorithm>
#include <regex>
#include <cctype>

namespace nx::tui {

// TextSelection implementation

size_t TextSelection::getLength(const EditorBuffer& buffer) const {
    if (!isValid() || isEmpty()) return 0;
    
    if (start.line == end.line) {
        return end.column - start.column;
    }
    
    // Multi-line selection - calculate total length
    size_t total_length = 0;
    for (size_t line = start.line; line <= end.line; ++line) {
        auto line_result = buffer.getLine(line);
        if (!line_result.has_value()) continue;
        
        const std::string& line_text = line_result.value();
        
        if (line == start.line) {
            total_length += line_text.length() - start.column + 1; // +1 for newline
        } else if (line == end.line) {
            total_length += end.column;
        } else {
            total_length += line_text.length() + 1; // +1 for newline
        }
    }
    
    return total_length;
}

std::string TextSelection::getText(const EditorBuffer& buffer) const {
    if (!isValid() || isEmpty()) return "";
    
    if (start.line == end.line) {
        auto line_result = buffer.getLine(start.line);
        if (!line_result.has_value()) return "";
        
        const std::string& line = line_result.value();
        if (start.column >= line.length() || end.column > line.length()) return "";
        
        return line.substr(start.column, end.column - start.column);
    }
    
    // Multi-line selection
    std::string result;
    for (size_t line = start.line; line <= end.line; ++line) {
        auto line_result = buffer.getLine(line);
        if (!line_result.has_value()) continue;
        
        const std::string& line_text = line_result.value();
        
        if (line == start.line) {
            if (start.column < line_text.length()) {
                result += line_text.substr(start.column);
            }
            result += "\n";
        } else if (line == end.line) {
            if (end.column <= line_text.length()) {
                result += line_text.substr(0, end.column);
            }
        } else {
            result += line_text + "\n";
        }
    }
    
    return result;
}

// MarkdownShortcuts implementation

MarkdownShortcuts::MarkdownShortcuts(MarkdownShortcutConfig config)
    : config_(std::move(config)) {}

Result<std::unique_ptr<EditorCommand>> MarkdownShortcuts::formatSelection(
    const EditorBuffer& buffer,
    MarkdownFormat format,
    const TextSelection& selection,
    CursorPosition cursor_position) const {
    
    auto delimiters = getDelimiters(format);
    if (delimiters.first.empty()) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidArgument, "Invalid markdown format"));
    }
    
    TextSelection working_selection = selection;
    
    // If no selection and extend_word_boundaries is enabled, select current word
    if (working_selection.isEmpty() && config_.extend_word_boundaries) {
        working_selection = findWordAt(buffer, cursor_position);
    }
    
    // If still no selection, just insert delimiters at cursor
    if (working_selection.isEmpty()) {
        working_selection = TextSelection(cursor_position, cursor_position);
    }
    
    auto validation_result = validateSelection(buffer, working_selection);
    if (!validation_result.has_value()) {
        return std::unexpected(validation_result.error());
    }
    working_selection = validation_result.value();
    
    std::string selected_text = working_selection.getText(buffer);
    
    return std::make_unique<MarkdownWrapCommand>(
        working_selection,
        delimiters.first,
        delimiters.second,
        selected_text
    );
}

Result<std::unique_ptr<EditorCommand>> MarkdownShortcuts::toggleFormat(
    const EditorBuffer& buffer,
    MarkdownFormat format,
    const TextSelection& selection,
    CursorPosition cursor_position) const {
    
    auto delimiters = getDelimiters(format);
    if (delimiters.first.empty()) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidArgument, "Invalid markdown format"));
    }
    
    TextSelection working_selection = selection;
    
    // If no selection, try to find word or detect existing formatting
    if (working_selection.isEmpty()) {
        if (config_.extend_word_boundaries) {
            working_selection = findWordAt(buffer, cursor_position);
        } else {
            working_selection = TextSelection(cursor_position, cursor_position);
        }
    }
    
    // Check if text already has the formatting
    if (hasDelimitersAround(buffer, working_selection, delimiters.first, delimiters.second)) {
        // Remove existing formatting - expand selection to include delimiters
        TextSelection expanded_selection(
            CursorPosition(working_selection.start.line, working_selection.start.column - delimiters.first.length()),
            CursorPosition(working_selection.end.line, working_selection.end.column + delimiters.second.length())
        );
        std::string formatted_text = expanded_selection.getText(buffer);
        return std::make_unique<MarkdownUnwrapCommand>(
            expanded_selection,
            delimiters.first,
            delimiters.second,
            formatted_text
        );
    } else {
        // Add formatting
        return formatSelection(buffer, format, working_selection, cursor_position);
    }
}

Result<std::unique_ptr<EditorCommand>> MarkdownShortcuts::createLink(
    const EditorBuffer& buffer,
    const TextSelection& selection,
    CursorPosition cursor_position,
    const std::string& url) const {
    
    if (!config_.enable_link_shortcut) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidState, "Link shortcuts are disabled"));
    }
    
    TextSelection working_selection = selection;
    
    // If no selection, select current word
    if (working_selection.isEmpty() && config_.extend_word_boundaries) {
        working_selection = findWordAt(buffer, cursor_position);
    }
    
    std::string link_text = working_selection.getText(buffer);
    if (link_text.empty()) {
        link_text = "link text";
    }
    
    std::string link_url = url;
    if (link_url.empty()) {
        link_url = "url";
    }
    
    std::string formatted_link = "[" + link_text + "](" + link_url + ")";
    
    return std::make_unique<ReplaceTextCommand>(
        working_selection.start,
        working_selection.end,
        working_selection.getText(buffer),
        formatted_link
    );
}

Result<std::unique_ptr<EditorCommand>> MarkdownShortcuts::createWikiLink(
    const EditorBuffer& buffer,
    const TextSelection& selection,
    CursorPosition cursor_position) const {
    
    if (!config_.enable_wiki_link_shortcut) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidState, "Wiki link shortcuts are disabled"));
    }
    
    return formatSelection(buffer, MarkdownFormat::WikiLink, selection, cursor_position);
}

std::vector<MarkdownFormat> MarkdownShortcuts::detectExistingFormats(
    const EditorBuffer& buffer,
    CursorPosition position) const {
    
    std::vector<MarkdownFormat> detected_formats;
    
    auto line_result = buffer.getLine(position.line);
    if (!line_result.has_value()) return detected_formats;
    
    const std::string& line = line_result.value();
    if (position.column >= line.length()) return detected_formats;
    
    // Check for common markdown patterns around the cursor
    std::vector<std::pair<MarkdownFormat, std::pair<std::string, std::string>>> patterns = {
        {MarkdownFormat::BoldItalic, {"***", "***"}},
        {MarkdownFormat::Bold, {"**", "**"}},
        {MarkdownFormat::Italic, {"*", "*"}},
        {MarkdownFormat::InlineCode, {"`", "`"}},
        {MarkdownFormat::Strikethrough, {"~~", "~~"}},
        {MarkdownFormat::Underline, {"__", "__"}},
        {MarkdownFormat::WikiLink, {"[[", "]]"}}
    };
    
    for (const auto& [format, delims] : patterns) {
        const auto& [open, close] = delims;
        
        // Check if cursor is within this format
        int open_pos = -1;
        int close_pos = -1;
        
        // Search backwards for opening delimiter
        for (int i = static_cast<int>(position.column) - 1; i >= 0; --i) {
            if (i + open.length() <= line.length() && 
                line.substr(i, open.length()) == open) {
                open_pos = i;
                break;
            }
        }
        
        // Search forwards for closing delimiter
        for (size_t i = position.column; i < line.length(); ++i) {
            if (i + close.length() <= line.length() && 
                line.substr(i, close.length()) == close) {
                close_pos = static_cast<int>(i);
                break;
            }
        }
        
        if (open_pos >= 0 && close_pos >= 0 && open_pos < close_pos) {
            detected_formats.push_back(format);
        }
    }
    
    return detected_formats;
}

TextSelection MarkdownShortcuts::extendToWordBoundaries(
    const EditorBuffer& buffer,
    const TextSelection& selection,
    CursorPosition cursor_position) const {
    
    if (!selection.isEmpty()) {
        return selection; // Already has selection
    }
    
    if (!config_.extend_word_boundaries) {
        return selection; // Return empty selection if word extension is disabled
    }
    
    return findWordAt(buffer, cursor_position);
}

bool MarkdownShortcuts::isWordBoundary(char ch) {
    return std::isspace(ch) || (std::ispunct(ch) && ch != '_');
}

std::pair<std::string, std::string> MarkdownShortcuts::getDelimiters(MarkdownFormat format) {
    switch (format) {
        case MarkdownFormat::Bold:
            return {"**", "**"};
        case MarkdownFormat::Italic:
            return {"*", "*"};
        case MarkdownFormat::BoldItalic:
            return {"***", "***"};
        case MarkdownFormat::InlineCode:
            return {"`", "`"};
        case MarkdownFormat::Strikethrough:
            return {"~~", "~~"};
        case MarkdownFormat::Underline:
            return {"__", "__"};
        case MarkdownFormat::WikiLink:
            return {"[[", "]]"};
        case MarkdownFormat::Link:
            // Links are handled specially in createLink()
            return {"", ""};
        default:
            return {"", ""};
    }
}

void MarkdownShortcuts::setConfig(const MarkdownShortcutConfig& config) {
    config_ = config;
}

const MarkdownShortcutConfig& MarkdownShortcuts::getConfig() const {
    return config_;
}

// Private methods

MarkdownFormatResult MarkdownShortcuts::applyDelimiters(
    const EditorBuffer& buffer,
    const TextSelection& selection,
    const std::string& opening_delimiter,
    const std::string& closing_delimiter) const {
    
    MarkdownFormatResult result;
    result.original_text = selection.getText(buffer);
    result.original_selection = selection;
    
    // Calculate new cursor position after adding delimiters
    size_t delimiters_length = opening_delimiter.length() + closing_delimiter.length();
    
    if (config_.preserve_selection_after_format && !selection.isEmpty()) {
        result.new_selection = TextSelection(
            CursorPosition(selection.start.line, selection.start.column + opening_delimiter.length()),
            CursorPosition(selection.end.line, selection.end.column + opening_delimiter.length())
        );
        result.selection_changed = true;
    } else {
        // Position cursor after closing delimiter
        result.new_cursor_position = CursorPosition(
            selection.end.line, 
            selection.end.column + delimiters_length
        );
    }
    
    result.success = true;
    return result;
}

MarkdownFormatResult MarkdownShortcuts::removeDelimiters(
    const EditorBuffer& buffer,
    const TextSelection& selection,
    const std::string& opening_delimiter,
    const std::string& closing_delimiter) const {
    
    MarkdownFormatResult result;
    result.original_text = selection.getText(buffer);
    result.original_selection = selection;
    
    // Calculate new cursor position after removing delimiters
    size_t delimiters_length = opening_delimiter.length() + closing_delimiter.length();
    
    if (config_.preserve_selection_after_format && !selection.isEmpty()) {
        result.new_selection = TextSelection(
            CursorPosition(selection.start.line, selection.start.column - opening_delimiter.length()),
            CursorPosition(selection.end.line, selection.end.column - opening_delimiter.length())
        );
        result.selection_changed = true;
    } else {
        // Position cursor after the unformatted text
        result.new_cursor_position = CursorPosition(
            selection.end.line, 
            selection.end.column - delimiters_length
        );
    }
    
    result.success = true;
    return result;
}

bool MarkdownShortcuts::hasDelimitersAround(
    const EditorBuffer& buffer,
    const TextSelection& selection,
    const std::string& opening_delimiter,
    const std::string& closing_delimiter) const {
    
    if (selection.isEmpty()) return false;
    
    // For single line only for now
    if (selection.start.line != selection.end.line) return false;
    
    auto line_result = buffer.getLine(selection.start.line);
    if (!line_result.has_value()) return false;
    
    const std::string& line = line_result.value();
    
    // Check if there's space for delimiters
    if (selection.start.column < opening_delimiter.length() ||
        selection.end.column + closing_delimiter.length() > line.length()) {
        return false;
    }
    
    // Check opening delimiter before selection
    size_t open_start = selection.start.column - opening_delimiter.length();
    if (line.substr(open_start, opening_delimiter.length()) != opening_delimiter) {
        return false;
    }
    
    // Check closing delimiter after selection
    if (line.substr(selection.end.column, closing_delimiter.length()) != closing_delimiter) {
        return false;
    }
    
    return true;
}

TextSelection MarkdownShortcuts::findWordAt(const EditorBuffer& buffer, CursorPosition position) const {
    auto line_result = buffer.getLine(position.line);
    if (!line_result.has_value()) {
        return TextSelection(position, position);
    }
    
    const std::string& line = line_result.value();
    if (position.column >= line.length()) {
        return TextSelection(position, position);
    }
    
    // Find word boundaries
    size_t word_start = position.column;
    size_t word_end = position.column;
    
    // Move start backwards to beginning of word
    while (word_start > 0 && !isWordBoundary(line[word_start - 1])) {
        --word_start;
    }
    
    // Move end forwards to end of word
    while (word_end < line.length() && !isWordBoundary(line[word_end])) {
        ++word_end;
    }
    
    return TextSelection(
        CursorPosition(position.line, word_start),
        CursorPosition(position.line, word_end)
    );
}

Result<TextSelection> MarkdownShortcuts::validateSelection(
    const EditorBuffer& buffer, 
    const TextSelection& selection) const {
    
    if (!selection.isValid()) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidArgument, "Invalid selection"));
    }
    
    // Check bounds
    size_t line_count = buffer.getLineCount();
    
    if (selection.start.line >= line_count || selection.end.line >= line_count) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidArgument, "Selection extends beyond buffer bounds"));
    }
    
    // Check column bounds for each line
    for (size_t line = selection.start.line; line <= selection.end.line; ++line) {
        auto line_result = buffer.getLine(line);
        if (!line_result.has_value()) {
            return std::unexpected(nx::makeError(nx::ErrorCode::kFileError, "Cannot access line in selection"));
        }
        
        const std::string& line_text = line_result.value();
        
        if (line == selection.start.line && selection.start.column > line_text.length()) {
            return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidArgument, "Selection start extends beyond line length"));
        }
        
        if (line == selection.end.line && selection.end.column > line_text.length()) {
            return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidArgument, "Selection end extends beyond line length"));
        }
    }
    
    return selection;
}

std::pair<CursorPosition, TextSelection> MarkdownShortcuts::calculateNewPosition(
    CursorPosition original_position,
    const TextSelection& selection,
    size_t delimiters_added,
    bool preserve_selection) const {
    
    CursorPosition new_cursor = original_position;
    TextSelection new_selection;
    
    if (preserve_selection && !selection.isEmpty()) {
        // Adjust selection for added delimiters
        new_selection = TextSelection(
            CursorPosition(selection.start.line, selection.start.column + delimiters_added / 2),
            CursorPosition(selection.end.line, selection.end.column + delimiters_added / 2)
        );
    } else {
        // Position cursor after the formatted text
        new_cursor = CursorPosition(
            selection.end.line,
            selection.end.column + delimiters_added
        );
    }
    
    return {new_cursor, new_selection};
}

// MarkdownWrapCommand implementation

MarkdownWrapCommand::MarkdownWrapCommand(
    TextSelection selection,
    std::string opening_delimiter,
    std::string closing_delimiter,
    std::string original_text)
    : selection_(std::move(selection))
    , opening_delimiter_(std::move(opening_delimiter))
    , closing_delimiter_(std::move(closing_delimiter))
    , original_text_(std::move(original_text))
    , timestamp_(std::chrono::steady_clock::now())
    , executed_(false) {}

Result<void> MarkdownWrapCommand::execute(EditorBuffer& buffer) {
    if (executed_) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidState, "Command already executed"));
    }
    
    // Replace selected text with wrapped version
    std::string wrapped_text = opening_delimiter_ + original_text_ + closing_delimiter_;
    
    // For single line selections, we can use setLine
    if (selection_.start.line == selection_.end.line) {
        auto line_result = buffer.getLine(selection_.start.line);
        if (!line_result.has_value()) {
            return std::unexpected(nx::makeError(nx::ErrorCode::kFileError, "Cannot access line"));
        }
        
        std::string line = line_result.value();
        std::string new_line = line.substr(0, selection_.start.column) + 
                              wrapped_text + 
                              line.substr(selection_.end.column);
        
        auto set_result = buffer.setLine(selection_.start.line, new_line);
        if (!set_result.has_value()) {
            return std::unexpected(set_result.error());
        }
    } else {
        return std::unexpected(nx::makeError(nx::ErrorCode::kNotImplemented, "Multi-line selection not yet implemented"));
    }
    
    executed_ = true;
    return {};
}

Result<void> MarkdownWrapCommand::undo(EditorBuffer& buffer) {
    if (!executed_) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidState, "Command not executed"));
    }
    
    // For single line selections, restore original text
    if (selection_.start.line == selection_.end.line) {
        auto line_result = buffer.getLine(selection_.start.line);
        if (!line_result.has_value()) {
            return std::unexpected(nx::makeError(nx::ErrorCode::kFileError, "Cannot access line"));
        }
        
        std::string line = line_result.value();
        size_t total_delimiter_length = opening_delimiter_.length() + closing_delimiter_.length();
        
        std::string new_line = line.substr(0, selection_.start.column) + 
                              original_text_ + 
                              line.substr(selection_.start.column + original_text_.length() + total_delimiter_length);
        
        auto set_result = buffer.setLine(selection_.start.line, new_line);
        if (!set_result.has_value()) {
            return std::unexpected(set_result.error());
        }
    } else {
        return std::unexpected(nx::makeError(nx::ErrorCode::kNotImplemented, "Multi-line selection not yet implemented"));
    }
    
    executed_ = false;
    return {};
}

bool MarkdownWrapCommand::canMergeWith(const EditorCommand& other) const {
    // Markdown wrap commands generally shouldn't merge
    return false;
}

Result<std::unique_ptr<EditorCommand>> MarkdownWrapCommand::mergeWith(std::unique_ptr<EditorCommand> other) {
    return std::unexpected(nx::makeError(nx::ErrorCode::kNotImplemented, "Markdown wrap commands cannot be merged"));
}

std::chrono::steady_clock::time_point MarkdownWrapCommand::getTimestamp() const {
    return timestamp_;
}

std::string MarkdownWrapCommand::getDescription() const {
    return "Wrap text with markdown delimiters: " + opening_delimiter_ + "..." + closing_delimiter_;
}

size_t MarkdownWrapCommand::getMemoryUsage() const {
    return sizeof(*this) + original_text_.capacity() + 
           opening_delimiter_.capacity() + closing_delimiter_.capacity();
}

// MarkdownUnwrapCommand implementation

MarkdownUnwrapCommand::MarkdownUnwrapCommand(
    TextSelection selection,
    std::string opening_delimiter,
    std::string closing_delimiter,
    std::string formatted_text)
    : selection_(std::move(selection))
    , opening_delimiter_(std::move(opening_delimiter))
    , closing_delimiter_(std::move(closing_delimiter))
    , formatted_text_(std::move(formatted_text))
    , timestamp_(std::chrono::steady_clock::now())
    , executed_(false) {}

Result<void> MarkdownUnwrapCommand::execute(EditorBuffer& buffer) {
    if (executed_) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidState, "Command already executed"));
    }
    
    // Remove delimiters from the formatted text
    if (formatted_text_.length() < opening_delimiter_.length() + closing_delimiter_.length()) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidArgument, "Formatted text too short to contain delimiters"));
    }
    
    std::string unformatted_text = formatted_text_.substr(
        opening_delimiter_.length(),
        formatted_text_.length() - opening_delimiter_.length() - closing_delimiter_.length()
    );
    
    // For single line selections
    if (selection_.start.line == selection_.end.line) {
        auto line_result = buffer.getLine(selection_.start.line);
        if (!line_result.has_value()) {
            return std::unexpected(nx::makeError(nx::ErrorCode::kFileError, "Cannot access line"));
        }
        
        std::string line = line_result.value();
        std::string new_line = line.substr(0, selection_.start.column) + 
                              unformatted_text + 
                              line.substr(selection_.end.column);
        
        auto set_result = buffer.setLine(selection_.start.line, new_line);
        if (!set_result.has_value()) {
            return std::unexpected(set_result.error());
        }
    } else {
        return std::unexpected(nx::makeError(nx::ErrorCode::kNotImplemented, "Multi-line selection not yet implemented"));
    }
    
    executed_ = true;
    return {};
}

Result<void> MarkdownUnwrapCommand::undo(EditorBuffer& buffer) {
    if (!executed_) {
        return std::unexpected(nx::makeError(nx::ErrorCode::kInvalidState, "Command not executed"));
    }
    
    // For single line selections, restore formatted text
    if (selection_.start.line == selection_.end.line) {
        auto line_result = buffer.getLine(selection_.start.line);
        if (!line_result.has_value()) {
            return std::unexpected(nx::makeError(nx::ErrorCode::kFileError, "Cannot access line"));
        }
        
        std::string line = line_result.value();
        std::string unformatted_text = formatted_text_.substr(
            opening_delimiter_.length(),
            formatted_text_.length() - opening_delimiter_.length() - closing_delimiter_.length()
        );
        
        std::string new_line = line.substr(0, selection_.start.column) + 
                              formatted_text_ + 
                              line.substr(selection_.start.column + unformatted_text.length());
        
        auto set_result = buffer.setLine(selection_.start.line, new_line);
        if (!set_result.has_value()) {
            return std::unexpected(set_result.error());
        }
    } else {
        return std::unexpected(nx::makeError(nx::ErrorCode::kNotImplemented, "Multi-line selection not yet implemented"));
    }
    
    executed_ = false;
    return {};
}

bool MarkdownUnwrapCommand::canMergeWith(const EditorCommand& other) const {
    return false;
}

Result<std::unique_ptr<EditorCommand>> MarkdownUnwrapCommand::mergeWith(std::unique_ptr<EditorCommand> other) {
    return std::unexpected(nx::makeError(nx::ErrorCode::kNotImplemented, "Markdown unwrap commands cannot be merged"));
}

std::chrono::steady_clock::time_point MarkdownUnwrapCommand::getTimestamp() const {
    return timestamp_;
}

std::string MarkdownUnwrapCommand::getDescription() const {
    return "Remove markdown delimiters: " + opening_delimiter_ + "..." + closing_delimiter_;
}

size_t MarkdownUnwrapCommand::getMemoryUsage() const {
    return sizeof(*this) + formatted_text_.capacity() + 
           opening_delimiter_.capacity() + closing_delimiter_.capacity();
}

} // namespace nx::tui