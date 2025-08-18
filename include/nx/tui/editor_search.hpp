#pragma once

#include <string>
#include <vector>
#include <regex>
#include <chrono>
#include <optional>
#include <memory>
#include "nx/common.hpp"
#include "nx/tui/editor_buffer.hpp"
#include "nx/tui/enhanced_cursor.hpp"
#include "nx/tui/editor_commands.hpp"

namespace nx::tui {

/**
 * @brief Search result representing a match in the editor buffer
 */
struct SearchMatch {
    size_t line;                    // Line number of match
    size_t start_column;            // Start column of match
    size_t end_column;              // End column of match (exclusive)
    std::string matched_text;       // The actual matched text
    std::string context_before;     // Context before match (up to 50 chars)
    std::string context_after;      // Context after match (up to 50 chars)
};

/**
 * @brief Search options and configuration
 */
struct SearchOptions {
    bool case_sensitive = false;
    bool whole_words = false;
    bool regex_mode = false;
    bool wrap_search = true;
    size_t max_results = 1000;     // Prevent memory exhaustion
    std::chrono::milliseconds timeout{5000}; // Search timeout
};

/**
 * @brief Search state and result management
 */
class SearchState {
public:
    SearchState() = default;
    ~SearchState() = default;

    // Search execution
    Result<std::vector<SearchMatch>> search(
        const EditorBuffer& buffer,
        const std::string& query,
        const SearchOptions& options = {});

    // Navigation through results
    Result<SearchMatch> findNext(const EnhancedCursor::Position& current_pos);
    Result<SearchMatch> findPrevious(const EnhancedCursor::Position& current_pos);
    
    // Result access
    const std::vector<SearchMatch>& getResults() const { return results_; }
    size_t getResultCount() const { return results_.size(); }
    int getCurrentResultIndex() const { return current_result_index_; }
    
    // Query information
    const std::string& getLastQuery() const { return last_query_; }
    const SearchOptions& getLastOptions() const { return last_options_; }
    
    // Cache management
    void clearResults();
    bool hasResults() const { return !results_.empty(); }
    
    // Performance metrics
    std::chrono::milliseconds getLastSearchDuration() const { return last_search_duration_; }

private:
    std::vector<SearchMatch> results_;
    std::string last_query_;
    SearchOptions last_options_;
    int current_result_index_ = -1;
    std::chrono::milliseconds last_search_duration_{0};
    
    // Cache for compiled regex
    std::optional<std::regex> compiled_regex_;
    std::string cached_regex_pattern_;
    
    // Internal search methods
    Result<void> validateSearchQuery(const std::string& query, const SearchOptions& options);
    Result<std::regex> compileRegex(const std::string& pattern, const SearchOptions& options);
    std::vector<SearchMatch> searchWithRegex(const EditorBuffer& buffer, const std::regex& regex);
    std::vector<SearchMatch> searchLiteral(const EditorBuffer& buffer, const std::string& query, const SearchOptions& options);
    SearchMatch createMatch(size_t line, size_t start_col, size_t end_col, 
                          const std::string& line_text, const std::string& matched_text);
};

/**
 * @brief High-level search manager for editor integration
 */
class EditorSearch {
public:
    explicit EditorSearch(EditorBuffer* buffer);
    ~EditorSearch() = default;

    // Search operations
    Result<void> startSearch(const std::string& query, const SearchOptions& options = {});
    Result<void> findNext();
    Result<void> findPrevious();
    Result<void> replaceNext(const std::string& replacement);
    Result<void> replaceAll(const std::string& replacement);
    
    // Search state
    bool isSearchActive() const { return search_active_; }
    void cancelSearch();
    
    // Result access
    const SearchState& getSearchState() const { return search_state_; }
    
    // Cursor integration
    void setCursor(EnhancedCursor* cursor) { cursor_ = cursor; }
    
    // Command history integration
    void setCommandHistory(CommandHistory* command_history) { command_history_ = command_history; }
    
    // Highlighting support
    std::vector<SearchMatch> getMatchesInRange(size_t start_line, size_t end_line) const;

private:
    EditorBuffer* buffer_;
    EnhancedCursor* cursor_;
    CommandHistory* command_history_;
    SearchState search_state_;
    bool search_active_ = false;
    
    // Helper methods
    Result<void> navigateToMatch(const SearchMatch& match);
    bool isMatchVisible(const SearchMatch& match, size_t viewport_start, size_t viewport_end) const;
};

/**
 * @brief Search input validator for security
 */
class SearchValidator {
public:
    /**
     * @brief Validate search query for security and performance
     * @param query The search query to validate
     * @param options Search options
     * @return Result indicating if query is safe to execute
     */
    static Result<void> validateQuery(const std::string& query, const SearchOptions& options);
    
    /**
     * @brief Check if regex pattern is safe to compile and execute
     * @param pattern Regex pattern to validate
     * @return Result indicating if pattern is safe
     */
    static Result<void> validateRegexPattern(const std::string& pattern);
    
    /**
     * @brief Estimate regex complexity to prevent ReDoS attacks
     * @param pattern Regex pattern to analyze
     * @return Complexity score (higher = more complex)
     */
    static size_t estimateRegexComplexity(const std::string& pattern);

private:
    static constexpr size_t MAX_QUERY_LENGTH = 1000;
    static constexpr size_t MAX_REGEX_COMPLEXITY = 100;
    static constexpr size_t MAX_QUANTIFIER_REPETITION = 1000;
};

} // namespace nx::tui