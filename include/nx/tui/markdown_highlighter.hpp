#pragma once

#include <string>
#include <vector>
#include <memory>
#include <regex>
#include <ftxui/screen/color.hpp>
#include "nx/common.hpp"

namespace nx::tui {

/**
 * @brief Style information for text segments
 */
struct TextStyle {
    ftxui::Color foreground = ftxui::Color::Default;
    ftxui::Color background = ftxui::Color::Default;
    bool bold = false;
    bool italic = false;
    bool underlined = false;
    bool dim = false;
    bool blink = false;
    bool inverted = false;
    
    bool operator==(const TextStyle& other) const {
        return foreground == other.foreground &&
               background == other.background &&
               bold == other.bold &&
               italic == other.italic &&
               underlined == other.underlined &&
               dim == other.dim &&
               blink == other.blink &&
               inverted == other.inverted;
    }
    
    bool operator!=(const TextStyle& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Styled text segment
 */
struct StyledSegment {
    size_t start_pos;
    size_t end_pos;
    TextStyle style;
    std::string element_type;  // For debugging/testing
    
    StyledSegment(size_t start, size_t end, const TextStyle& s, const std::string& type = "")
        : start_pos(start), end_pos(end), style(s), element_type(type) {}
    
    bool contains(size_t pos) const {
        return pos >= start_pos && pos < end_pos;
    }
    
    bool overlaps(const StyledSegment& other) const {
        return start_pos < other.end_pos && end_pos > other.start_pos;
    }
};

/**
 * @brief Markdown syntax highlighting configuration
 */
struct MarkdownHighlightConfig {
    bool enabled = true;
    bool highlight_headers = true;
    bool highlight_emphasis = true;
    bool highlight_code = true;
    bool highlight_links = true;
    bool highlight_lists = true;
    bool highlight_quotes = true;
    bool highlight_wiki_links = true;
    bool highlight_tags = true;
    bool highlight_horizontal_rules = true;
    bool dim_syntax_chars = true;
    
    // Color scheme
    TextStyle header_style{ftxui::Color::Blue, ftxui::Color::Default, true, false};
    TextStyle emphasis_italic_style{ftxui::Color::Default, ftxui::Color::Default, false, true};
    TextStyle emphasis_bold_style{ftxui::Color::Default, ftxui::Color::Default, true, false};
    TextStyle emphasis_bold_italic_style{ftxui::Color::Default, ftxui::Color::Default, true, true};
    TextStyle code_inline_style{ftxui::Color::Green, ftxui::Color::GrayDark};
    TextStyle code_block_style{ftxui::Color::Green, ftxui::Color::GrayDark};
    TextStyle link_style{ftxui::Color::Blue, ftxui::Color::Default, false, false, true};
    TextStyle link_text_style{ftxui::Color::Blue, ftxui::Color::Default};
    TextStyle link_url_style{ftxui::Color::Cyan, ftxui::Color::Default, false, false, false, true};
    TextStyle list_marker_style{ftxui::Color::Yellow, ftxui::Color::Default, true};
    TextStyle quote_style{ftxui::Color::GrayDark, ftxui::Color::Default, false, true};
    TextStyle wiki_link_style{ftxui::Color::Magenta, ftxui::Color::Default};
    TextStyle tag_style{ftxui::Color::Yellow, ftxui::Color::Default};
    TextStyle horizontal_rule_style{ftxui::Color::GrayDark, ftxui::Color::Default, false, false, false, true};
    TextStyle syntax_char_style{ftxui::Color::GrayDark, ftxui::Color::Default, false, false, false, true};
};

/**
 * @brief Result of syntax highlighting
 */
struct HighlightResult {
    std::vector<StyledSegment> segments;
    bool success = true;
    std::string error_message;
    
    void addSegment(size_t start, size_t end, const TextStyle& style, const std::string& type = "") {
        segments.emplace_back(start, end, style, type);
    }
    
    void clear() {
        segments.clear();
        success = true;
        error_message.clear();
    }
    
    // Sort segments by position and merge overlapping ones
    void optimize();
    
    // Get effective style at a specific position
    TextStyle getStyleAt(size_t pos) const;
};

/**
 * @brief Markdown syntax highlighter
 */
class MarkdownHighlighter {
public:
    explicit MarkdownHighlighter(const MarkdownHighlightConfig& config = {});
    ~MarkdownHighlighter() = default;
    
    /**
     * @brief Highlight a line of markdown text
     * @param text The text to highlight
     * @param line_number Line number (for context)
     * @param is_in_code_block Whether this line is inside a code block
     * @return Highlight result with styled segments
     */
    HighlightResult highlightLine(const std::string& text, size_t line_number = 0, bool is_in_code_block = false) const;
    
    /**
     * @brief Highlight multiple lines of markdown text
     * @param lines Vector of text lines
     * @param start_line_number Starting line number
     * @return Vector of highlight results for each line
     */
    std::vector<HighlightResult> highlightLines(const std::vector<std::string>& lines, size_t start_line_number = 0) const;
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void setConfig(const MarkdownHighlightConfig& config) { config_ = config; }
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const MarkdownHighlightConfig& getConfig() const { return config_; }
    
    /**
     * @brief Check if highlighting is enabled
     * @return true if enabled
     */
    bool isEnabled() const { return config_.enabled; }
    
    /**
     * @brief Enable or disable highlighting
     * @param enabled Whether to enable highlighting
     */
    void setEnabled(bool enabled) { config_.enabled = enabled; }

private:
    MarkdownHighlightConfig config_;
    
    // Pattern matching methods
    void highlightHeaders(const std::string& text, HighlightResult& result) const;
    void highlightEmphasis(const std::string& text, HighlightResult& result) const;
    void highlightCode(const std::string& text, HighlightResult& result, bool is_in_code_block) const;
    void highlightLinks(const std::string& text, HighlightResult& result) const;
    void highlightLists(const std::string& text, HighlightResult& result) const;
    void highlightQuotes(const std::string& text, HighlightResult& result) const;
    void highlightWikiLinks(const std::string& text, HighlightResult& result) const;
    void highlightTags(const std::string& text, HighlightResult& result) const;
    void highlightHorizontalRules(const std::string& text, HighlightResult& result) const;
    
    // Helper methods
    bool isAtWordBoundary(const std::string& text, size_t pos) const;
    size_t findClosingDelimiter(const std::string& text, size_t start, const std::string& delimiter) const;
    void addSyntaxCharStyle(const std::string& text, HighlightResult& result, size_t start, size_t end) const;
    bool isSetextHeaderUnderline(const std::string& text) const;
};

/**
 * @brief Factory for creating theme-based highlight configurations
 */
class HighlightThemes {
public:
    static MarkdownHighlightConfig getDefaultTheme();
    static MarkdownHighlightConfig getDarkTheme();
    static MarkdownHighlightConfig getLightTheme();
    static MarkdownHighlightConfig getMinimalTheme();
    static MarkdownHighlightConfig getGithubTheme();
    static MarkdownHighlightConfig getMonochromeTheme();
};

} // namespace nx::tui