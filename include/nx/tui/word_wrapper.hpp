#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace nx::tui {

/**
 * @brief Simple word wrapper for text content
 * 
 * Provides basic word wrapping functionality that respects word boundaries
 * and handles Unicode text properly.
 */
class WordWrapper {
public:
    /**
     * @brief Configuration options for word wrapping
     */
    struct Config {
        bool preserve_indentation;  // Maintain indentation on wrapped lines
        bool break_long_words;      // Break words longer than max_width
        size_t hang_indent;            // Additional indent for continuation lines
        std::string line_break_indicator;  // String to show at line breaks
        
        Config() 
            : preserve_indentation(true)
            , break_long_words(true)
            , hang_indent(0)
            , line_break_indicator("")
        {}
    };

    /**
     * @brief Wrap a single line of text
     * 
     * @param line The line to wrap
     * @param max_width Maximum width for wrapped lines
     * @param config Wrapping configuration options
     * @return Vector of wrapped line segments
     */
    static std::vector<std::string> wrapLine(
        const std::string& line,
        size_t max_width,
        const Config& config = Config{}
    );

    /**
     * @brief Wrap multiple lines of text
     * 
     * @param lines Vector of lines to wrap
     * @param max_width Maximum width for wrapped lines
     * @param config Wrapping configuration options
     * @return Vector of all wrapped line segments
     */
    static std::vector<std::string> wrapLines(
        const std::vector<std::string>& lines,
        size_t max_width,
        const Config& config = Config{}
    );

    /**
     * @brief Calculate display width of text accounting for Unicode
     * 
     * @param text Text to measure
     * @return Display width in characters
     */
    static size_t calculateDisplayWidth(const std::string& text);

    /**
     * @brief Check if a character is a word boundary
     * 
     * @param c Character to check
     * @return True if character is a word boundary
     */
    static bool isWordBoundary(char c);

    /**
     * @brief Find the best break point in a line
     * 
     * @param line Line to analyze
     * @param max_width Maximum width
     * @return Position to break at, or string::npos if no good break point
     */
    static size_t findBreakPoint(const std::string& line, size_t max_width);

private:
    /**
     * @brief Extract leading whitespace from a line
     * 
     * @param line Line to analyze
     * @return Leading whitespace string
     */
    static std::string extractIndentation(const std::string& line);

    /**
     * @brief Check if a line should not be wrapped (e.g., code blocks)
     * 
     * @param line Line to check
     * @return True if line should remain unwrapped
     */
    static bool shouldSkipWrapping(const std::string& line);
};

} // namespace nx::tui