#include "nx/tui/editor_search.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace nx::tui {

// SearchState implementation

Result<std::vector<SearchMatch>> SearchState::search(
    const EditorBuffer& buffer,
    const std::string& query,
    const SearchOptions& options) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Validate the search query
    auto validation = validateSearchQuery(query, options);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    
    // Clear previous results
    clearResults();
    last_query_ = query;
    last_options_ = options;
    
    std::vector<SearchMatch> matches;
    
    try {
        if (options.regex_mode) {
            // Compile regex with caching
            auto regex_result = compileRegex(query, options);
            if (!regex_result) {
                return std::unexpected(regex_result.error());
            }
            
            matches = searchWithRegex(buffer, regex_result.value());
        } else {
            matches = searchLiteral(buffer, query, options);
        }
        
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        if (elapsed > options.timeout) {
            return makeErrorResult<std::vector<SearchMatch>>(
                ErrorCode::kSystemError, 
                "Search timeout after " + std::to_string(elapsed.count()) + "ms");
        }
        
        // Limit results to prevent memory exhaustion
        if (matches.size() > options.max_results) {
            matches.resize(options.max_results);
        }
        
        results_ = std::move(matches);
        current_result_index_ = results_.empty() ? -1 : 0;
        last_search_duration_ = elapsed;
        
        return results_;
        
    } catch (const std::regex_error& e) {
        return makeErrorResult<std::vector<SearchMatch>>(
            ErrorCode::kValidationError, 
            "Regex error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return makeErrorResult<std::vector<SearchMatch>>(
            ErrorCode::kSystemError, 
            "Search error: " + std::string(e.what()));
    }
}

Result<SearchMatch> SearchState::findNext(const EnhancedCursor::Position& current_pos) {
    if (results_.empty()) {
        return makeErrorResult<SearchMatch>(ErrorCode::kValidationError, "No search results available");
    }
    
    // Find first result after current position
    for (size_t i = 0; i < results_.size(); ++i) {
        const auto& match = results_[i];
        
        if (match.line > current_pos.line || 
            (match.line == current_pos.line && match.start_column > current_pos.column)) {
            current_result_index_ = static_cast<int>(i);
            return match;
        }
    }
    
    // Wrap to beginning if enabled
    if (last_options_.wrap_search && !results_.empty()) {
        current_result_index_ = 0;
        return results_[0];
    }
    
    return makeErrorResult<SearchMatch>(ErrorCode::kValidationError, "No more matches found");
}

Result<SearchMatch> SearchState::findPrevious(const EnhancedCursor::Position& current_pos) {
    if (results_.empty()) {
        return makeErrorResult<SearchMatch>(ErrorCode::kValidationError, "No search results available");
    }
    
    // Find last result before current position
    for (int i = static_cast<int>(results_.size()) - 1; i >= 0; --i) {
        const auto& match = results_[i];
        
        if (match.line < current_pos.line || 
            (match.line == current_pos.line && match.end_column <= current_pos.column)) {
            current_result_index_ = i;
            return match;
        }
    }
    
    // Wrap to end if enabled
    if (last_options_.wrap_search && !results_.empty()) {
        current_result_index_ = static_cast<int>(results_.size()) - 1;
        return results_.back();
    }
    
    return makeErrorResult<SearchMatch>(ErrorCode::kValidationError, "No previous matches found");
}

void SearchState::clearResults() {
    results_.clear();
    current_result_index_ = -1;
    last_query_.clear();
    last_search_duration_ = std::chrono::milliseconds{0};
}

Result<void> SearchState::validateSearchQuery(const std::string& query, const SearchOptions& options) {
    return SearchValidator::validateQuery(query, options);
}

Result<std::regex> SearchState::compileRegex(const std::string& pattern, const SearchOptions& options) {
    // Check cache first
    if (compiled_regex_.has_value() && cached_regex_pattern_ == pattern) {
        return compiled_regex_.value();
    }
    
    // Validate pattern security
    auto validation = SearchValidator::validateRegexPattern(pattern);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    
    try {
        std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
        
        if (!options.case_sensitive) {
            flags |= std::regex_constants::icase;
        }
        
        std::regex regex(pattern, flags);
        
        // Cache the compiled regex
        compiled_regex_ = regex;
        cached_regex_pattern_ = pattern;
        
        return regex;
        
    } catch (const std::regex_error& e) {
        return makeErrorResult<std::regex>(
            ErrorCode::kValidationError, 
            "Invalid regex pattern: " + std::string(e.what()));
    }
}

std::vector<SearchMatch> SearchState::searchWithRegex(const EditorBuffer& buffer, const std::regex& regex) {
    std::vector<SearchMatch> matches;
    
    for (size_t line_num = 0; line_num < buffer.getLineCount(); ++line_num) {
        auto line_result = buffer.getLine(line_num);
        if (!line_result) {
            continue;
        }
        
        const std::string& line_text = line_result.value();
        
        // Use regex iterator for all matches in the line
        auto begin = std::sregex_iterator(line_text.begin(), line_text.end(), regex);
        auto end = std::sregex_iterator();
        
        for (auto it = begin; it != end; ++it) {
            const std::smatch& match = *it;
            
            size_t start_col = static_cast<size_t>(match.position());
            size_t end_col = start_col + static_cast<size_t>(match.length());
            
            matches.push_back(createMatch(line_num, start_col, end_col, line_text, match.str()));
        }
    }
    
    return matches;
}

std::vector<SearchMatch> SearchState::searchLiteral(
    const EditorBuffer& buffer, 
    const std::string& query, 
    const SearchOptions& options) {
    
    std::vector<SearchMatch> matches;
    
    // Early return for empty queries to avoid infinite loop
    if (query.empty()) {
        return matches;
    }
    
    // Prepare query for case-insensitive search
    std::string search_query = query;
    if (!options.case_sensitive) {
        std::transform(search_query.begin(), search_query.end(), search_query.begin(), ::tolower);
    }
    
    for (size_t line_num = 0; line_num < buffer.getLineCount(); ++line_num) {
        auto line_result = buffer.getLine(line_num);
        if (!line_result) {
            continue;
        }
        
        std::string line_text = line_result.value();
        std::string search_text = line_text;
        
        if (!options.case_sensitive) {
            std::transform(search_text.begin(), search_text.end(), search_text.begin(), ::tolower);
        }
        
        size_t pos = 0;
        while ((pos = search_text.find(search_query, pos)) != std::string::npos) {
            size_t start_col = pos;
            size_t end_col = pos + query.length();
            
            // Check whole word constraint
            if (options.whole_words) {
                bool valid_start = (start_col == 0) || !std::isalnum(search_text[start_col - 1]);
                bool valid_end = (end_col >= search_text.length()) || !std::isalnum(search_text[end_col]);
                
                if (!valid_start || !valid_end) {
                    pos++;
                    continue;
                }
            }
            
            // Extract the actual matched text from original line (preserves original case)
            std::string matched_text = line_text.substr(start_col, end_col - start_col);
            matches.push_back(createMatch(line_num, start_col, end_col, line_text, matched_text));
            
            pos = end_col;
        }
    }
    
    return matches;
}

SearchMatch SearchState::createMatch(
    size_t line, 
    size_t start_col, 
    size_t end_col,
    const std::string& line_text, 
    const std::string& matched_text) {
    
    SearchMatch match;
    match.line = line;
    match.start_column = start_col;
    match.end_column = end_col;
    match.matched_text = matched_text;
    
    // Extract context before and after
    constexpr size_t CONTEXT_LENGTH = 50;
    
    size_t context_start = (start_col > CONTEXT_LENGTH) ? start_col - CONTEXT_LENGTH : 0;
    size_t context_end = std::min(end_col + CONTEXT_LENGTH, line_text.length());
    
    if (context_start < start_col) {
        match.context_before = line_text.substr(context_start, start_col - context_start);
    }
    
    if (end_col < context_end) {
        match.context_after = line_text.substr(end_col, context_end - end_col);
    }
    
    return match;
}

// EditorSearch implementation

EditorSearch::EditorSearch(EditorBuffer* buffer) 
    : buffer_(buffer), cursor_(nullptr), command_history_(nullptr) {
}

Result<void> EditorSearch::startSearch(const std::string& query, const SearchOptions& options) {
    if (!buffer_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No buffer available for search");
    }
    
    auto search_result = search_state_.search(*buffer_, query, options);
    if (!search_result) {
        return std::unexpected(search_result.error());
    }
    
    search_active_ = !search_result.value().empty();
    
    // If results found and cursor is available, navigate to first match
    if (search_active_ && cursor_ && !search_result.value().empty()) {
        auto navigate_result = navigateToMatch(search_result.value()[0]);
        if (!navigate_result) {
            return navigate_result;
        }
    }
    
    return {};
}

Result<void> EditorSearch::findNext() {
    if (!search_active_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No active search");
    }
    
    if (!cursor_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No cursor available");
    }
    
    auto next_result = search_state_.findNext(cursor_->getPosition());
    if (!next_result) {
        return std::unexpected(next_result.error());
    }
    
    return navigateToMatch(next_result.value());
}

Result<void> EditorSearch::findPrevious() {
    if (!search_active_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No active search");
    }
    
    if (!cursor_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No cursor available");
    }
    
    auto prev_result = search_state_.findPrevious(cursor_->getPosition());
    if (!prev_result) {
        return std::unexpected(prev_result.error());
    }
    
    return navigateToMatch(prev_result.value());
}

Result<void> EditorSearch::replaceNext(const std::string& replacement) {
    if (!search_active_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No active search");
    }
    
    if (!cursor_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No cursor available");
    }
    
    if (!buffer_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No buffer available");
    }
    
    // Find the next match from current cursor position
    auto next_result = search_state_.findNext(cursor_->getPosition());
    if (!next_result) {
        return std::unexpected(next_result.error());
    }
    
    const auto& match = next_result.value();
    
    // Create replace command using the command system
    CursorPosition start_pos(match.line, match.start_column);
    CursorPosition end_pos(match.line, match.end_column);
    
    auto replace_cmd = CommandFactory::createReplaceText(
        start_pos, end_pos, match.matched_text, replacement);
    
    // Execute the replace command through the command history
    if (!command_history_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, 
            "No command history available for replace operation");
    }
    
    auto execute_result = command_history_->executeCommand(*buffer_, std::move(replace_cmd));
    if (!execute_result) {
        return execute_result;
    }
    
    // Update search results to reflect the change
    SearchOptions current_options = search_state_.getLastOptions();
    std::string current_query = search_state_.getLastQuery();
    
    // Re-run search to update results
    auto search_result = search_state_.search(*buffer_, current_query, current_options);
    if (!search_result) {
        return std::unexpected(search_result.error());
    }
    
    return {};
}

Result<void> EditorSearch::replaceAll(const std::string& replacement) {
    if (!search_active_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No active search");
    }
    
    if (!command_history_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, 
            "No command history available for replace operation");
    }
    
    if (!buffer_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No buffer available");
    }
    
    const auto& results = search_state_.getResults();
    if (results.empty()) {
        return {}; // No matches to replace
    }
    
    // Replace matches in reverse order to maintain valid positions
    std::vector<SearchMatch> sorted_results = results;
    std::sort(sorted_results.begin(), sorted_results.end(), 
        [](const SearchMatch& a, const SearchMatch& b) {
            if (a.line != b.line) {
                return a.line > b.line; // Reverse line order
            }
            return a.start_column > b.start_column; // Reverse column order within line
        });
    
    size_t replaced_count = 0;
    for (const auto& match : sorted_results) {
        CursorPosition start_pos(match.line, match.start_column);
        CursorPosition end_pos(match.line, match.end_column);
        
        auto replace_cmd = CommandFactory::createReplaceText(
            start_pos, end_pos, match.matched_text, replacement);
        
        auto execute_result = command_history_->executeCommand(*buffer_, std::move(replace_cmd));
        if (execute_result) {
            replaced_count++;
        }
        // Continue even if some replacements fail
    }
    
    // Update search results to reflect all changes
    SearchOptions current_options = search_state_.getLastOptions();
    std::string current_query = search_state_.getLastQuery();
    
    auto search_result = search_state_.search(*buffer_, current_query, current_options);
    if (!search_result) {
        return std::unexpected(search_result.error());
    }
    
    return {};
}

void EditorSearch::cancelSearch() {
    search_active_ = false;
    search_state_.clearResults();
}

std::vector<SearchMatch> EditorSearch::getMatchesInRange(size_t start_line, size_t end_line) const {
    std::vector<SearchMatch> matches_in_range;
    
    for (const auto& match : search_state_.getResults()) {
        if (match.line >= start_line && match.line <= end_line) {
            matches_in_range.push_back(match);
        }
    }
    
    return matches_in_range;
}

Result<void> EditorSearch::navigateToMatch(const SearchMatch& match) {
    if (!cursor_) {
        return makeErrorResult<void>(ErrorCode::kInvalidState, "No cursor available");
    }
    
    // Move cursor to the start of the match
    auto move_result = cursor_->setPosition(match.line, match.start_column);
    if (!move_result) {
        return move_result;
    }
    
    // Create selection covering the match
    auto select_result = cursor_->startSelection(EnhancedCursor::SelectionMode::Character);
    if (!select_result) {
        return select_result;
    }
    
    // Move to end of match to complete selection
    auto end_result = cursor_->setPosition(match.line, match.end_column);
    if (!end_result) {
        return end_result;
    }
    
    return {};
}

bool EditorSearch::isMatchVisible(const SearchMatch& match, size_t viewport_start, size_t viewport_end) const {
    return match.line >= viewport_start && match.line <= viewport_end;
}

// SearchValidator implementation

Result<void> SearchValidator::validateQuery(const std::string& query, const SearchOptions& options) {
    // Check query length
    if (query.length() > MAX_QUERY_LENGTH) {
        return makeErrorResult<void>(
            ErrorCode::kValidationError, 
            "Search query too long (max " + std::to_string(MAX_QUERY_LENGTH) + " characters)");
    }
    
    // Empty query is allowed (will match nothing)
    if (query.empty()) {
        return {};
    }
    
    // Validate regex pattern if in regex mode
    if (options.regex_mode) {
        return validateRegexPattern(query);
    }
    
    return {};
}

Result<void> SearchValidator::validateRegexPattern(const std::string& pattern) {
    // Check pattern complexity
    size_t complexity = estimateRegexComplexity(pattern);
    if (complexity > MAX_REGEX_COMPLEXITY) {
        return makeErrorResult<void>(
            ErrorCode::kValidationError, 
            "Regex pattern too complex (score: " + std::to_string(complexity) + 
            ", max: " + std::to_string(MAX_REGEX_COMPLEXITY) + ")");
    }
    
    // Check for potentially dangerous patterns
    if (pattern.find("(.*).*") != std::string::npos ||
        pattern.find("(.*)+") != std::string::npos ||
        pattern.find(".*.*") != std::string::npos) {
        return makeErrorResult<void>(
            ErrorCode::kValidationError, 
            "Potentially dangerous regex pattern detected");
    }
    
    // Try to compile the pattern to check syntax
    try {
        std::regex test_regex(pattern, std::regex_constants::ECMAScript);
        return {};
    } catch (const std::regex_error& e) {
        return makeErrorResult<void>(
            ErrorCode::kValidationError, 
            "Invalid regex syntax: " + std::string(e.what()));
    }
}

size_t SearchValidator::estimateRegexComplexity(const std::string& pattern) {
    size_t complexity = 0;
    
    // Count potentially expensive constructs
    for (size_t i = 0; i < pattern.length(); ++i) {
        char c = pattern[i];
        
        switch (c) {
            case '*':
            case '+':
                complexity += 10;
                break;
            case '?':
                complexity += 5;
                break;
            case '{':
                // Look for quantifier ranges
                if (i + 1 < pattern.length()) {
                    std::string quantifier;
                    size_t j = i + 1;
                    while (j < pattern.length() && pattern[j] != '}') {
                        quantifier += pattern[j];
                        j++;
                    }
                    
                    if (j < pattern.length()) {
                        // Parse quantifier
                        if (quantifier.find(',') != std::string::npos) {
                            complexity += 15; // Range quantifier
                        } else {
                            try {
                                size_t count = std::stoull(quantifier);
                                if (count > MAX_QUANTIFIER_REPETITION) {
                                    complexity += 50; // Very high repetition
                                } else {
                                    complexity += count / 10;
                                }
                            } catch (...) {
                                complexity += 5;
                            }
                        }
                        i = j; // Skip processed quantifier
                    }
                }
                break;
            case '(':
                complexity += 3; // Grouping
                break;
            case '[':
                complexity += 2; // Character class
                break;
            case '.':
                complexity += 1; // Wildcard
                break;
        }
    }
    
    return complexity;
}

} // namespace nx::tui