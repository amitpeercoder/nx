#include "nx/tui/markdown_highlighter.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

namespace nx::tui {

// HighlightResult implementation

void HighlightResult::optimize() {
    if (segments.empty()) return;
    
    // Sort segments by start position
    std::sort(segments.begin(), segments.end(), 
        [](const StyledSegment& a, const StyledSegment& b) {
            return a.start_pos < b.start_pos;
        });
    
    // Remove empty segments
    segments.erase(
        std::remove_if(segments.begin(), segments.end(),
            [](const StyledSegment& seg) {
                return seg.start_pos >= seg.end_pos;
            }),
        segments.end()
    );
    
    // Merge overlapping segments with the same style
    std::vector<StyledSegment> merged;
    for (const auto& segment : segments) {
        if (merged.empty() || 
            !merged.back().overlaps(segment) ||
            merged.back().style != segment.style) {
            merged.push_back(segment);
        } else {
            // Merge overlapping segments with same style
            merged.back().end_pos = std::max(merged.back().end_pos, segment.end_pos);
        }
    }
    
    segments = std::move(merged);
}

TextStyle HighlightResult::getStyleAt(size_t pos) const {
    // Find the last segment that contains this position
    // (in case of overlapping segments, later ones take precedence)
    for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
        if (it->contains(pos)) {
            return it->style;
        }
    }
    return TextStyle{}; // Default style
}

// MarkdownHighlighter implementation

MarkdownHighlighter::MarkdownHighlighter(const MarkdownHighlightConfig& config)
    : config_(config) {
}

HighlightResult MarkdownHighlighter::highlightLine(const std::string& text, size_t line_number, bool is_in_code_block) const {
    HighlightResult result;
    
    if (!config_.enabled || text.empty()) {
        return result;
    }
    
    try {
        // Apply highlighting in order of precedence
        if (!is_in_code_block) {
            if (config_.highlight_headers) {
                highlightHeaders(text, result);
            }
            if (config_.highlight_horizontal_rules) {
                highlightHorizontalRules(text, result);
            }
            if (config_.highlight_quotes) {
                highlightQuotes(text, result);
            }
            if (config_.highlight_lists) {
                highlightLists(text, result);
            }
        }
        
        if (config_.highlight_code) {
            highlightCode(text, result, is_in_code_block);
        }
        
        if (!is_in_code_block) {
            if (config_.highlight_wiki_links) {
                highlightWikiLinks(text, result);
            }
            if (config_.highlight_links) {
                highlightLinks(text, result);
            }
            if (config_.highlight_emphasis) {
                highlightEmphasis(text, result);
            }
            if (config_.highlight_tags) {
                highlightTags(text, result);
            }
        }
        
        result.optimize();
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Highlighting error: " + std::string(e.what());
    }
    
    return result;
}

std::vector<HighlightResult> MarkdownHighlighter::highlightLines(const std::vector<std::string>& lines, size_t start_line_number) const {
    std::vector<HighlightResult> results;
    results.reserve(lines.size());
    
    bool in_code_block = false;
    
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        
        // Check for code block delimiters
        if ((line.length() >= 3 && line.substr(0, 3) == "```") || 
            (line.length() >= 3 && line.substr(0, 3) == "~~~")) {
            in_code_block = !in_code_block;
        }
        
        // Get the basic highlighting for this line
        HighlightResult line_result = highlightLine(line, start_line_number + i, in_code_block);
        
        // Check for setext headers if not in code block and we have a next line
        if (!in_code_block && config_.highlight_headers && i + 1 < lines.size()) {
            const std::string& next_line = lines[i + 1];
            
            // Check if next line is setext header underline
            if (isSetextHeaderUnderline(next_line)) {
                // Check if current line can be a setext header (non-empty, no ATX header markers)
                if (!line.empty() && (line.length() == 0 || line[0] != '#') && line.find_first_not_of(" \t") != std::string::npos) {
                    // Apply header styling to the entire current line
                    line_result.addSegment(0, line.length(), config_.header_style, "setext_header");
                }
            }
        }
        
        results.push_back(std::move(line_result));
    }
    
    // Second pass: style setext header underlines
    for (size_t i = 1; i < results.size(); ++i) {
        if (i < lines.size()) {
            const std::string& line = lines[i];
            const std::string& prev_line = lines[i - 1];
            
            // If this line is a setext underline and previous line was highlighted as setext header
            if (!in_code_block && isSetextHeaderUnderline(line) && 
                !prev_line.empty() && (prev_line.length() == 0 || prev_line[0] != '#') && 
                prev_line.find_first_not_of(" \t") != std::string::npos) {
                
                // Style the underline with a dimmed syntax style
                results[i].addSegment(0, line.length(), config_.syntax_char_style, "setext_underline");
            }
        }
    }
    
    return results;
}

void MarkdownHighlighter::highlightHeaders(const std::string& text, HighlightResult& result) const {
    // ATX headers: # Header, ## Header, etc.
    std::regex header_regex(R"(^(\s*)(#{1,6})(\s+)(.+)$)");
    std::smatch match;
    
    if (std::regex_match(text, match, header_regex)) {
        size_t header_start = match[1].length();
        size_t hash_end = header_start + match[2].length();
        size_t space_end = hash_end + match[3].length();
        
        // Style the hash symbols
        if (config_.dim_syntax_chars) {
            result.addSegment(header_start, hash_end, config_.syntax_char_style, "header_syntax");
        }
        
        // Style the header text
        result.addSegment(space_end, text.length(), config_.header_style, "header_text");
    }
    
    // Setext headers: Header\n===== or Header\n-----
    // Note: Setext headers are handled in highlightLines() method with multi-line context
}

void MarkdownHighlighter::highlightEmphasis(const std::string& text, HighlightResult& result) const {
    // Bold italic: ***text***
    std::regex bold_italic_regex(R"(\*\*\*([^*]+?)\*\*\*)");
    std::sregex_iterator bold_italic_begin(text.begin(), text.end(), bold_italic_regex);
    std::sregex_iterator bold_italic_end;
    
    for (auto it = bold_italic_begin; it != bold_italic_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        size_t content_start = start + 3;
        size_t content_end = end - 3;
        
        if (config_.dim_syntax_chars) {
            result.addSegment(start, content_start, config_.syntax_char_style, "emphasis_syntax");
            result.addSegment(content_end, end, config_.syntax_char_style, "emphasis_syntax");
        }
        result.addSegment(content_start, content_end, config_.emphasis_bold_italic_style, "bold_italic");
    }
    
    // Simple bold: **text**
    std::regex bold_regex(R"(\*\*([^*]+?)\*\*)");
    std::sregex_iterator bold_begin(text.begin(), text.end(), bold_regex);
    std::sregex_iterator bold_end;
    
    for (auto it = bold_begin; it != bold_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        size_t content_start = start + 2;
        size_t content_end = end - 2;
        
        // Skip if this overlaps with bold italic
        bool overlaps_bold_italic = false;
        for (auto bi_it = bold_italic_begin; bi_it != bold_italic_end; ++bi_it) {
            size_t bi_start = bi_it->position();
            size_t bi_end_pos = bi_start + bi_it->length();
            if (start < bi_end_pos && end > bi_start) {
                overlaps_bold_italic = true;
                break;
            }
        }
        
        if (!overlaps_bold_italic) {
            if (config_.dim_syntax_chars) {
                result.addSegment(start, content_start, config_.syntax_char_style, "emphasis_syntax");
                result.addSegment(content_end, end, config_.syntax_char_style, "emphasis_syntax");
            }
            result.addSegment(content_start, content_end, config_.emphasis_bold_style, "bold");
        }
    }
    
    // Simple italic: *text*
    std::regex italic_regex(R"(\*([^*]+?)\*)");
    std::sregex_iterator italic_begin(text.begin(), text.end(), italic_regex);
    std::sregex_iterator italic_end;
    
    for (auto it = italic_begin; it != italic_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        size_t content_start = start + 1;
        size_t content_end = end - 1;
        
        // Skip if this overlaps with bold or bold italic
        bool overlaps = false;
        for (auto bold_it = bold_begin; bold_it != bold_end; ++bold_it) {
            size_t bold_start = bold_it->position();
            size_t bold_end_pos = bold_start + bold_it->length();
            if (start < bold_end_pos && end > bold_start) {
                overlaps = true;
                break;
            }
        }
        for (auto bi_it = bold_italic_begin; bi_it != bold_italic_end && !overlaps; ++bi_it) {
            size_t bi_start = bi_it->position();
            size_t bi_end_pos = bi_start + bi_it->length();
            if (start < bi_end_pos && end > bi_start) {
                overlaps = true;
                break;
            }
        }
        
        if (!overlaps) {
            if (config_.dim_syntax_chars) {
                result.addSegment(start, content_start, config_.syntax_char_style, "emphasis_syntax");
                result.addSegment(content_end, end, config_.syntax_char_style, "emphasis_syntax");
            }
            result.addSegment(content_start, content_end, config_.emphasis_italic_style, "italic");
        }
    }
    
    // Underscore emphasis: __bold__ and _italic_
    std::regex underscore_bold_regex(R"(__([^_]+?)__)");
    std::sregex_iterator ub_begin(text.begin(), text.end(), underscore_bold_regex);
    std::sregex_iterator ub_end;
    
    for (auto it = ub_begin; it != ub_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        size_t content_start = start + 2;
        size_t content_end = end - 2;
        
        if (config_.dim_syntax_chars) {
            result.addSegment(start, content_start, config_.syntax_char_style, "emphasis_syntax");
            result.addSegment(content_end, end, config_.syntax_char_style, "emphasis_syntax");
        }
        result.addSegment(content_start, content_end, config_.emphasis_bold_style, "bold");
    }
    
    std::regex underscore_italic_regex(R"(_([^_]+?)_)");
    std::sregex_iterator ui_begin(text.begin(), text.end(), underscore_italic_regex);
    std::sregex_iterator ui_end;
    
    for (auto it = ui_begin; it != ui_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        size_t content_start = start + 1;
        size_t content_end = end - 1;
        
        // Skip if this overlaps with underscore bold
        bool overlaps = false;
        for (auto ub_it = ub_begin; ub_it != ub_end; ++ub_it) {
            size_t ub_start = ub_it->position();
            size_t ub_end_pos = ub_start + ub_it->length();
            if (start < ub_end_pos && end > ub_start) {
                overlaps = true;
                break;
            }
        }
        
        if (!overlaps) {
            if (config_.dim_syntax_chars) {
                result.addSegment(start, content_start, config_.syntax_char_style, "emphasis_syntax");
                result.addSegment(content_end, end, config_.syntax_char_style, "emphasis_syntax");
            }
            result.addSegment(content_start, content_end, config_.emphasis_italic_style, "italic");
        }
    }
}

void MarkdownHighlighter::highlightCode(const std::string& text, HighlightResult& result, bool is_in_code_block) const {
    if (is_in_code_block) {
        // Entire line is code
        result.addSegment(0, text.length(), config_.code_block_style, "code_block");
        return;
    }
    
    // Code fences: ```code``` or ~~~code~~~
    std::regex code_fence_regex(R"((```|~~~)(.+?)\1)");
    std::sregex_iterator fence_begin(text.begin(), text.end(), code_fence_regex);
    std::sregex_iterator fence_end;
    
    for (auto it = fence_begin; it != fence_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        
        result.addSegment(start, end, config_.code_block_style, "code_fence");
    }
    
    // Inline code: `code`
    std::regex inline_code_regex(R"(`([^`]+?)`)");  
    std::sregex_iterator code_begin(text.begin(), text.end(), inline_code_regex);
    std::sregex_iterator code_end;
    
    for (auto it = code_begin; it != code_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        
        // Skip if this overlaps with a code fence
        bool overlaps_fence = false;
        for (auto fence_it = fence_begin; fence_it != fence_end; ++fence_it) {
            size_t fence_start = fence_it->position();
            size_t fence_end_pos = fence_start + fence_it->length();
            if (start < fence_end_pos && end > fence_start) {
                overlaps_fence = true;
                break;
            }
        }
        
        if (!overlaps_fence) {
            result.addSegment(start, end, config_.code_inline_style, "inline_code");
        }
    }
}

void MarkdownHighlighter::highlightLinks(const std::string& text, HighlightResult& result) const {
    // Markdown links: [text](url)
    std::regex link_regex(R"(\[([^\]]+)\]\(([^)]+)\))");
    std::sregex_iterator link_begin(text.begin(), text.end(), link_regex);
    std::sregex_iterator link_end;
    
    for (auto it = link_begin; it != link_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        
        // Find the parts
        size_t text_start = start + 1;  // After '['
        size_t text_end = text_start + match[1].length();
        size_t url_start = text_end + 2;  // After ']('
        size_t url_end = url_start + match[2].length();
        
        // Style the brackets and parentheses
        if (config_.dim_syntax_chars) {
            result.addSegment(start, text_start, config_.syntax_char_style, "link_syntax");
            result.addSegment(text_end, url_start, config_.syntax_char_style, "link_syntax");
            result.addSegment(url_end, end, config_.syntax_char_style, "link_syntax");
        }
        
        // Style the link text and URL
        result.addSegment(text_start, text_end, config_.link_text_style, "link_text");
        result.addSegment(url_start, url_end, config_.link_url_style, "link_url");
    }
    
    // Auto links: <url>
    std::regex auto_link_regex(R"(<(https?://[^>]+)>)");
    std::sregex_iterator auto_begin(text.begin(), text.end(), auto_link_regex);
    std::sregex_iterator auto_end;
    
    for (auto it = auto_begin; it != auto_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        
        result.addSegment(start, end, config_.link_style, "auto_link");
    }
}

void MarkdownHighlighter::highlightLists(const std::string& text, HighlightResult& result) const {
    std::smatch match;
    
    // Task lists: - [ ] or - [x] (check this first, as it's more specific)
    std::regex task_list_regex(R"(^(\s*)([-*+])(\s+)(\[[ xX]\])(\s+))");
    if (std::regex_search(text, match, task_list_regex)) {
        size_t marker_start = match[1].length();
        size_t marker_end = marker_start + match[2].length();
        size_t checkbox_start = marker_end + match[3].length();
        size_t checkbox_end = checkbox_start + match[4].length();
        
        result.addSegment(marker_start, marker_end, config_.list_marker_style, "task_list_marker");
        result.addSegment(checkbox_start, checkbox_end, config_.list_marker_style, "task_checkbox");
        return; // Don't process as regular list
    }
    
    // Unordered lists: -, *, +
    std::regex unordered_list_regex(R"(^(\s*)([-*+])(\s+))");
    if (std::regex_search(text, match, unordered_list_regex)) {
        size_t marker_start = match[1].length();
        size_t marker_end = marker_start + match[2].length();
        
        result.addSegment(marker_start, marker_end, config_.list_marker_style, "list_marker");
    }
    
    // Ordered lists: 1., 2., etc.
    std::regex ordered_list_regex(R"(^(\s*)(\d+)(\.)(\s+))");
    if (std::regex_search(text, match, ordered_list_regex)) {
        size_t number_start = match[1].length();
        size_t number_end = number_start + match[2].length();
        size_t dot_end = number_end + match[3].length();
        
        result.addSegment(number_start, dot_end, config_.list_marker_style, "ordered_list_marker");
    }
}

void MarkdownHighlighter::highlightQuotes(const std::string& text, HighlightResult& result) const {
    std::regex quote_regex(R"(^(\s*)(>+)(\s*))");
    std::smatch match;
    
    if (std::regex_search(text, match, quote_regex)) {
        size_t quote_start = match[1].length();
        size_t quote_end = quote_start + match[2].length();
        size_t content_start = quote_end + match[3].length();
        
        // Style the quote marker
        if (config_.dim_syntax_chars) {
            result.addSegment(quote_start, content_start, config_.syntax_char_style, "quote_syntax");
        }
        
        // Style the quoted content
        result.addSegment(content_start, text.length(), config_.quote_style, "quote_content");
    }
}

void MarkdownHighlighter::highlightWikiLinks(const std::string& text, HighlightResult& result) const {
    std::regex wiki_link_regex(R"(\[\[([^\]]+)\]\])");
    std::sregex_iterator wiki_begin(text.begin(), text.end(), wiki_link_regex);
    std::sregex_iterator wiki_end;
    
    for (auto it = wiki_begin; it != wiki_end; ++it) {
        const auto& match = *it;
        size_t start = match.position();
        size_t end = start + match.length();
        
        result.addSegment(start, end, config_.wiki_link_style, "wiki_link");
    }
}

void MarkdownHighlighter::highlightTags(const std::string& text, HighlightResult& result) const {
    std::regex tag_regex(R"((?:^|\s)(#\w+))");
    std::sregex_iterator tag_begin(text.begin(), text.end(), tag_regex);
    std::sregex_iterator tag_end;
    
    for (auto it = tag_begin; it != tag_end; ++it) {
        const auto& match = *it;
        size_t tag_start = match.position(1);  // Position of the tag (group 1)
        size_t tag_end_pos = tag_start + match[1].length();
        
        result.addSegment(tag_start, tag_end_pos, config_.tag_style, "tag");
    }
}

void MarkdownHighlighter::highlightHorizontalRules(const std::string& text, HighlightResult& result) const {
    // Horizontal rules: ---, ***, ___
    std::regex hr_regex(R"(^(\s*)([-*_])\2{2,}(\s*)$)");
    std::smatch match;
    
    if (std::regex_match(text, match, hr_regex)) {
        result.addSegment(0, text.length(), config_.horizontal_rule_style, "horizontal_rule");
    }
}

bool MarkdownHighlighter::isAtWordBoundary(const std::string& text, size_t pos) const {
    if (pos == 0 || pos >= text.length()) return true;
    
    char prev = text[pos - 1];
    char curr = text[pos];
    
    return std::isspace(prev) || std::isspace(curr) || 
           std::ispunct(prev) || std::ispunct(curr);
}

size_t MarkdownHighlighter::findClosingDelimiter(const std::string& text, size_t start, const std::string& delimiter) const {
    size_t pos = start + delimiter.length();
    
    while (pos <= text.length() - delimiter.length()) {
        if (text.substr(pos, delimiter.length()) == delimiter) {
            return pos;
        }
        pos++;
    }
    
    return std::string::npos;
}

void MarkdownHighlighter::addSyntaxCharStyle(const std::string& text, HighlightResult& result, size_t start, size_t end) const {
    if (config_.dim_syntax_chars) {
        result.addSegment(start, end, config_.syntax_char_style, "syntax_char");
    }
}

// HighlightThemes implementation

MarkdownHighlightConfig HighlightThemes::getDefaultTheme() {
    return MarkdownHighlightConfig{};  // Uses default values
}

MarkdownHighlightConfig HighlightThemes::getDarkTheme() {
    MarkdownHighlightConfig config;
    config.header_style = {ftxui::Color::Cyan, ftxui::Color::Default, true, false};
    config.emphasis_italic_style = {ftxui::Color::Yellow, ftxui::Color::Default, false, true};
    config.emphasis_bold_style = {ftxui::Color::Yellow, ftxui::Color::Default, true, false};
    config.emphasis_bold_italic_style = {ftxui::Color::Yellow, ftxui::Color::Default, true, true};
    config.code_inline_style = {ftxui::Color::Green, ftxui::Color::Black};
    config.code_block_style = {ftxui::Color::Green, ftxui::Color::Black};
    config.link_style = {ftxui::Color::Blue, ftxui::Color::Default, false, false, true};
    config.wiki_link_style = {ftxui::Color::Magenta, ftxui::Color::Default};
    config.tag_style = {ftxui::Color::Red, ftxui::Color::Default};
    return config;
}

MarkdownHighlightConfig HighlightThemes::getLightTheme() {
    MarkdownHighlightConfig config;
    config.header_style = {ftxui::Color::Blue, ftxui::Color::Default, true, false};
    config.code_inline_style = {ftxui::Color::Green, ftxui::Color::GrayLight};
    config.code_block_style = {ftxui::Color::Green, ftxui::Color::GrayLight};
    config.syntax_char_style = {ftxui::Color::GrayDark, ftxui::Color::Default, false, false, false, true};
    return config;
}

MarkdownHighlightConfig HighlightThemes::getMinimalTheme() {
    MarkdownHighlightConfig config;
    config.header_style = {ftxui::Color::Default, ftxui::Color::Default, true, false};
    config.emphasis_italic_style = {ftxui::Color::Default, ftxui::Color::Default, false, true};
    config.emphasis_bold_style = {ftxui::Color::Default, ftxui::Color::Default, true, false};
    config.code_inline_style = {ftxui::Color::Default, ftxui::Color::Default};
    config.code_block_style = {ftxui::Color::Default, ftxui::Color::Default};
    config.link_style = {ftxui::Color::Default, ftxui::Color::Default, false, false, true};
    config.wiki_link_style = {ftxui::Color::Default, ftxui::Color::Default, false, false, true};
    config.tag_style = {ftxui::Color::Default, ftxui::Color::Default};
    config.dim_syntax_chars = false;
    return config;
}

MarkdownHighlightConfig HighlightThemes::getGithubTheme() {
    MarkdownHighlightConfig config;
    config.header_style = {ftxui::Color::Blue, ftxui::Color::Default, true, false};
    config.emphasis_italic_style = {ftxui::Color::Default, ftxui::Color::Default, false, true};
    config.emphasis_bold_style = {ftxui::Color::Default, ftxui::Color::Default, true, false};
    config.code_inline_style = {ftxui::Color::Red, ftxui::Color::GrayLight};
    config.code_block_style = {ftxui::Color::Default, ftxui::Color::GrayLight};
    config.link_style = {ftxui::Color::Blue, ftxui::Color::Default, false, false, true};
    config.quote_style = {ftxui::Color::GrayDark, ftxui::Color::Default};
    return config;
}

MarkdownHighlightConfig HighlightThemes::getMonochromeTheme() {
    MarkdownHighlightConfig config;
    config.header_style = {ftxui::Color::Default, ftxui::Color::Default, true, false};
    config.emphasis_italic_style = {ftxui::Color::Default, ftxui::Color::Default, false, true};
    config.emphasis_bold_style = {ftxui::Color::Default, ftxui::Color::Default, true, false};
    config.emphasis_bold_italic_style = {ftxui::Color::Default, ftxui::Color::Default, true, true};
    config.code_inline_style = {ftxui::Color::Default, ftxui::Color::Default, false, false, false, false, false, true};
    config.code_block_style = {ftxui::Color::Default, ftxui::Color::Default, false, false, false, false, false, true};
    config.link_style = {ftxui::Color::Default, ftxui::Color::Default, false, false, true};
    config.wiki_link_style = {ftxui::Color::Default, ftxui::Color::Default, false, false, true};
    config.tag_style = {ftxui::Color::Default, ftxui::Color::Default, true, false};
    config.horizontal_rule_style = {ftxui::Color::Default, ftxui::Color::Default, false, false, false, true};
    config.syntax_char_style = {ftxui::Color::Default, ftxui::Color::Default, false, false, false, true};
    return config;
}

bool MarkdownHighlighter::isSetextHeaderUnderline(const std::string& text) const {
    if (text.empty()) {
        return false;
    }
    
    // Remove leading and trailing whitespace
    std::string trimmed = text;
    size_t start = trimmed.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return false;
    }
    
    size_t end = trimmed.find_last_not_of(" \t");
    trimmed = trimmed.substr(start, end - start + 1);
    
    if (trimmed.empty()) {
        return false;
    }
    
    // Check for setext header level 1: ===== (at least 1 equals sign)
    if (trimmed[0] == '=') {
        for (char c : trimmed) {
            if (c != '=') {
                return false;
            }
        }
        return true;
    }
    
    // Check for setext header level 2: ----- (at least 1 dash)
    if (trimmed[0] == '-') {
        for (char c : trimmed) {
            if (c != '-') {
                return false;
            }
        }
        return true;
    }
    
    return false;
}

} // namespace nx::tui