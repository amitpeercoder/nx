#include "nx/tui/word_wrapper.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace nx::tui {

std::vector<std::string> WordWrapper::wrapLine(
    const std::string& line,
    size_t max_width,
    const Config& config
) {
    std::vector<std::string> wrapped;
    
    // Handle empty lines or very small width
    if (line.empty() || max_width < 2) {
        wrapped.push_back(line);
        return wrapped;
    }
    
    // Check if line should be skipped (e.g., code blocks)
    if (shouldSkipWrapping(line)) {
        wrapped.push_back(line);
        return wrapped;
    }
    
    // Extract indentation for continuation lines
    std::string indentation;
    if (config.preserve_indentation) {
        indentation = extractIndentation(line);
    }
    
    // Add hang indent if specified
    std::string continuation_indent = indentation;
    if (config.hang_indent > 0) {
        continuation_indent += std::string(config.hang_indent, ' ');
    }
    
    std::string remaining = line;
    bool first_line = true;
    
    while (!remaining.empty()) {
        size_t available_width = max_width;
        
        // Account for indentation on continuation lines
        if (!first_line && config.preserve_indentation) {
            if (continuation_indent.length() >= max_width) {
                // If indentation is too long, just use original line
                wrapped.push_back(remaining);
                break;
            }
            available_width -= continuation_indent.length();
        }
        
        // If remaining text fits in available width
        if (calculateDisplayWidth(remaining) <= available_width) {
            if (first_line) {
                wrapped.push_back(remaining);
            } else {
                wrapped.push_back(continuation_indent + remaining);
            }
            break;
        }
        
        // Find the best break point
        size_t break_point = findBreakPoint(remaining, available_width);
        
        if (break_point == std::string::npos) {
            // No good break point found
            if (config.break_long_words && available_width > 0) {
                // Force break at available width
                break_point = std::min(available_width, remaining.length());
            } else {
                // Can't break, add whole remaining text
                if (first_line) {
                    wrapped.push_back(remaining);
                } else {
                    wrapped.push_back(continuation_indent + remaining);
                }
                break;
            }
        }
        
        // Extract the line segment
        std::string segment = remaining.substr(0, break_point);
        
        // Remove trailing whitespace
        while (!segment.empty() && std::isspace(segment.back())) {
            segment.pop_back();
        }
        
        // Add line break indicator if configured
        if (!config.line_break_indicator.empty() && break_point < remaining.length()) {
            segment += config.line_break_indicator;
        }
        
        // Add to wrapped lines
        if (first_line) {
            wrapped.push_back(segment);
        } else {
            wrapped.push_back(continuation_indent + segment);
        }
        
        // Move to next segment
        remaining = remaining.substr(break_point);
        
        // Skip leading whitespace on continuation lines
        while (!remaining.empty() && std::isspace(remaining.front())) {
            remaining.erase(0, 1);
        }
        
        first_line = false;
    }
    
    // Ensure we always return at least one line
    if (wrapped.empty()) {
        wrapped.push_back("");
    }
    
    return wrapped;
}

std::vector<std::string> WordWrapper::wrapLines(
    const std::vector<std::string>& lines,
    size_t max_width,
    const Config& config
) {
    std::vector<std::string> all_wrapped;
    
    for (const auto& line : lines) {
        auto wrapped_line = wrapLine(line, max_width, config);
        all_wrapped.insert(all_wrapped.end(), wrapped_line.begin(), wrapped_line.end());
    }
    
    return all_wrapped;
}

size_t WordWrapper::calculateDisplayWidth(const std::string& text) {
    // Simple implementation - count characters
    // TODO: Could be enhanced with proper Unicode width calculation
    size_t width = 0;
    for (char c : text) {
        if (c == '\t') {
            // Tab expands to next multiple of 4
            width = (width + 4) & ~static_cast<size_t>(3);
        } else if (static_cast<unsigned char>(c) >= 32) {
            // Printable character
            width++;
        }
        // Skip control characters
    }
    return width;
}

bool WordWrapper::isWordBoundary(char c) {
    return std::isspace(c) || 
           c == '-' || c == '_' || 
           c == '.' || c == ',' || 
           c == ';' || c == ':' || 
           c == '!' || c == '?' ||
           c == '(' || c == ')' ||
           c == '[' || c == ']' ||
           c == '{' || c == '}' ||
           c == '/' || c == '\\' ||
           c == '|' || c == '&';
}

size_t WordWrapper::findBreakPoint(const std::string& line, size_t max_width) {
    if (line.empty() || max_width == 0) {
        return std::string::npos;
    }
    
    // If line fits completely, no break needed
    if (calculateDisplayWidth(line) <= max_width) {
        return std::string::npos;
    }
    
    // Find the last word boundary within max_width
    size_t best_break = std::string::npos;
    size_t current_width = 0;
    
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        
        // Calculate width of this character
        size_t char_width = 1;
        if (c == '\t') {
            char_width = 4 - (current_width % 4);
        }
        
        // Check if adding this character would exceed width
        if (current_width + char_width > max_width) {
            break;
        }
        
        current_width += char_width;
        
        // If this is a word boundary, it's a potential break point
        if (isWordBoundary(c)) {
            best_break = i + 1; // Break after the boundary character
        }
    }
    
    // If we found a word boundary, use it
    if (best_break != std::string::npos) {
        return best_break;
    }
    
    // No word boundary found within width - return position at max_width
    current_width = 0;
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        size_t char_width = (c == '\t') ? (4 - (current_width % 4)) : 1;
        
        if (current_width + char_width > max_width) {
            return i;
        }
        current_width += char_width;
    }
    
    return line.length();
}

std::string WordWrapper::extractIndentation(const std::string& line) {
    std::string indentation;
    
    for (char c : line) {
        if (c == ' ' || c == '\t') {
            indentation += c;
        } else {
            break;
        }
    }
    
    return indentation;
}

bool WordWrapper::shouldSkipWrapping(const std::string& line) {
    // Skip code blocks (lines starting with 4 spaces or tab + spaces)
    if (line.length() >= 4 && 
        (line.substr(0, 4) == "    " || 
         (line[0] == '\t' && line.find_first_not_of(" \t") != std::string::npos))) {
        return true;
    }
    
    // Skip fenced code blocks
    if (line.length() >= 3 && (
        line.substr(0, 3) == "```" || 
        line.substr(0, 3) == "~~~")) {
        return true;
    }
    
    // Skip horizontal rules
    if (line.length() >= 3) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
        
        if (trimmed.length() >= 3 && 
            (trimmed.find_first_not_of('-') == std::string::npos ||
             trimmed.find_first_not_of('*') == std::string::npos ||
             trimmed.find_first_not_of('_') == std::string::npos)) {
            return true;
        }
    }
    
    return false;
}

} // namespace nx::tui