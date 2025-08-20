#include <gtest/gtest.h>
#include "nx/tui/editor_search.hpp"
#include "nx/tui/editor_buffer.hpp"
#include "nx/tui/enhanced_cursor.hpp"

using namespace nx::tui;

class EditorSearchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize editor buffer with test content
        EditorBuffer::Config buffer_config;
        buffer_ = std::make_unique<EditorBuffer>(buffer_config);
        
        // Add test content
        buffer_->insertLine(0, "Hello World");
        buffer_->insertLine(1, "This is a test line");
        buffer_->insertLine(2, "Another TEST with different case");
        buffer_->insertLine(3, "Regular expression test: [0-9]+");
        buffer_->insertLine(4, "Unicode test: café naïve résumé");
        buffer_->insertLine(5, "Word boundary test: testing tested");
        buffer_->insertLine(6, "");
        buffer_->insertLine(7, "Final line with Hello again");
        
        // Initialize enhanced cursor
        EnhancedCursor::Config cursor_config;
        cursor_ = std::make_unique<EnhancedCursor>(cursor_config);
        cursor_->initialize(*buffer_);
        
        // Initialize search functionality
        search_ = std::make_unique<EditorSearch>(buffer_.get());
        search_->setCursor(cursor_.get());
    }
    
    std::unique_ptr<EditorBuffer> buffer_;
    std::unique_ptr<EnhancedCursor> cursor_;
    std::unique_ptr<EditorSearch> search_;
};

// Basic search functionality tests

TEST_F(EditorSearchTest, BasicLiteralSearch) {
    SearchOptions options;
    auto result = search_->startSearch("test", options);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(search_->isSearchActive());
    
    const auto& search_state = search_->getSearchState();
    EXPECT_EQ(search_state.getResultCount(), 7); // "test" appears in multiple variations
    
    const auto& results = search_state.getResults();
    EXPECT_GT(results.size(), 0);
    
    // Check first result
    EXPECT_EQ(results[0].line, 1); // "This is a test line"
    EXPECT_EQ(results[0].matched_text, "test");
}

TEST_F(EditorSearchTest, CaseSensitiveSearch) {
    SearchOptions options;
    options.case_sensitive = true;
    
    auto result = search_->startSearch("TEST", options);
    ASSERT_TRUE(result.has_value());
    
    const auto& results = search_->getSearchState().getResults();
    
    // Should only find "TEST" in uppercase, not "test"
    bool found_uppercase = false;
    for (const auto& match : results) {
        if (match.matched_text == "TEST") {
            found_uppercase = true;
            EXPECT_EQ(match.line, 2); // "Another TEST with different case"
        }
        // Should not find lowercase "test"
        EXPECT_NE(match.matched_text, "test");
    }
    EXPECT_TRUE(found_uppercase);
}

TEST_F(EditorSearchTest, CaseInsensitiveSearch) {
    SearchOptions options;
    options.case_sensitive = false;
    
    auto result = search_->startSearch("TEST", options);
    ASSERT_TRUE(result.has_value());
    
    const auto& results = search_->getSearchState().getResults();
    
    // Should find both "test" and "TEST"
    bool found_lowercase = false;
    bool found_uppercase = false;
    
    for (const auto& match : results) {
        if (match.matched_text == "test") {
            found_lowercase = true;
        } else if (match.matched_text == "TEST") {
            found_uppercase = true;
        }
    }
    
    EXPECT_TRUE(found_lowercase);
    EXPECT_TRUE(found_uppercase);
}

TEST_F(EditorSearchTest, WholeWordsSearch) {
    SearchOptions options;
    options.whole_words = true;
    
    auto result = search_->startSearch("test", options);
    ASSERT_TRUE(result.has_value());
    
    const auto& results = search_->getSearchState().getResults();
    
    // Check that "test" is found as whole word but not as part of "testing" or "tested"
    for (const auto& match : results) {
        // Should find "test" or "TEST" (preserves original case)
        EXPECT_TRUE(match.matched_text == "test" || match.matched_text == "TEST");
        // Verify context doesn't include partial matches
        if (match.line == 5) { // "Word boundary test: testing tested"
            // Should find "test" in the middle, not "test" from "testing" or "tested"
            EXPECT_TRUE(match.start_column > 0);
            EXPECT_TRUE(match.start_column < 20); // Approximate position of "test:"
        }
    }
}

TEST_F(EditorSearchTest, RegexSearch) {
    SearchOptions options;
    options.regex_mode = true;
    
    auto result = search_->startSearch("[0-9]+", options);
    ASSERT_TRUE(result.has_value());
    
    const auto& results = search_->getSearchState().getResults();
    EXPECT_GT(results.size(), 0);
    
    // Should find the numeric pattern in line 3
    bool found_number = false;
    for (const auto& match : results) {
        if (match.line == 3) {
            found_number = true;
            // The match should be numeric
            EXPECT_FALSE(match.matched_text.empty());
            // Verify it's actually a number
            for (char c : match.matched_text) {
                EXPECT_TRUE(std::isdigit(c));
            }
        }
    }
    EXPECT_TRUE(found_number);
}

TEST_F(EditorSearchTest, EmptyQuerySearch) {
    SearchOptions options;
    auto result = search_->startSearch("", options);
    
    ASSERT_TRUE(result.has_value());
    const auto& results = search_->getSearchState().getResults();
    EXPECT_EQ(results.size(), 0); // Empty query should return no results
}

TEST_F(EditorSearchTest, NonExistentTextSearch) {
    SearchOptions options;
    auto result = search_->startSearch("nonexistent", options);
    
    ASSERT_TRUE(result.has_value());
    const auto& results = search_->getSearchState().getResults();
    EXPECT_EQ(results.size(), 0);
}

// Navigation tests

TEST_F(EditorSearchTest, FindNext) {
    SearchOptions options;
    
    auto result = search_->startSearch("Hello", options);
    ASSERT_TRUE(result.has_value());
    
    const auto& results = search_->getSearchState().getResults();
    EXPECT_GE(results.size(), 2); // "Hello" appears at least twice
    
    // Move cursor away from any matches first to test findNext properly
    cursor_->setPosition(5, 0); // Position at empty line 6 (0-indexed)
    
    // Test finding next - should go to first match after cursor position
    auto next_result = search_->findNext();
    EXPECT_TRUE(next_result.has_value());
    
    // Cursor should be positioned at the end of the found match
    auto cursor_pos = cursor_->getPosition();
    
    // Since cursor was at line 5 and matches are at (0,0) and (7,16), 
    // it should go to (7,16) as that's the next match after line 5
    // findNext() positions cursor at the END of the match
    const auto& expected_match = results[1]; // Second match
    size_t expected_end_column = expected_match.start_column + expected_match.matched_text.length();
    
    EXPECT_EQ(cursor_pos.line, expected_match.line);
    EXPECT_EQ(cursor_pos.column, expected_end_column);
}

TEST_F(EditorSearchTest, FindPrevious) {
    SearchOptions options;
    auto result = search_->startSearch("Hello", options);
    ASSERT_TRUE(result.has_value());
    
    // Move cursor to end of document
    cursor_->setPosition(7, 0);
    
    auto prev_result = search_->findPrevious();
    EXPECT_TRUE(prev_result.has_value());
    
    // Should find a match before the current position
    auto cursor_pos = cursor_->getPosition();
    EXPECT_LT(cursor_pos.line, 7);
}

TEST_F(EditorSearchTest, WrapSearch) {
    SearchOptions options;
    options.wrap_search = true;
    
    auto result = search_->startSearch("Hello", options);
    ASSERT_TRUE(result.has_value());
    
    // Move to end of document
    cursor_->setPosition(7, 20);
    
    // Find next should wrap to beginning
    auto next_result = search_->findNext();
    EXPECT_TRUE(next_result.has_value());
    
    auto cursor_pos = cursor_->getPosition();
    EXPECT_EQ(cursor_pos.line, 0); // Should wrap to first occurrence
}

// Security and validation tests

TEST_F(EditorSearchTest, SecurityValidation) {
    SearchOptions options;
    options.regex_mode = true;
    
    // Test potentially dangerous regex patterns
    auto result1 = search_->startSearch("(.*).*", options);
    EXPECT_FALSE(result1.has_value()); // Should be rejected
    
    auto result2 = search_->startSearch("(.*)+", options);
    EXPECT_FALSE(result2.has_value()); // Should be rejected
    
    auto result3 = search_->startSearch(".*.*", options);
    EXPECT_FALSE(result3.has_value()); // Should be rejected
}

TEST_F(EditorSearchTest, MaxResultsLimit) {
    SearchOptions options;
    options.max_results = 2; // Limit to 2 results
    
    auto result = search_->startSearch("e", options); // Common letter
    ASSERT_TRUE(result.has_value());
    
    const auto& results = search_->getSearchState().getResults();
    EXPECT_LE(results.size(), 2); // Should not exceed limit
}

TEST_F(EditorSearchTest, SearchTimeout) {
    SearchOptions options;
    options.timeout = std::chrono::milliseconds(1); // Very short timeout
    options.regex_mode = true;
    
    // This search might timeout depending on system performance
    auto result = search_->startSearch(".*test.*", options);
    
    // Either succeeds quickly or times out
    if (!result.has_value()) {
        EXPECT_TRUE(result.error().message().find("timeout") != std::string::npos);
    }
}

// Performance tests

TEST_F(EditorSearchTest, PerformanceLargeText) {
    // Add more lines for performance testing
    for (int i = 0; i < 100; ++i) {
        buffer_->insertLine(buffer_->getLineCount(), 
            "Performance test line " + std::to_string(i) + " with test content");
    }
    
    SearchOptions options;
    auto start_time = std::chrono::steady_clock::now();
    
    auto result = search_->startSearch("test", options);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_LT(duration.count(), 100); // Should complete within 100ms
    
    const auto& results = search_->getSearchState().getResults();
    EXPECT_GT(results.size(), 100); // Should find many matches
}

// Search result structure tests

TEST_F(EditorSearchTest, SearchResultStructure) {
    SearchOptions options;
    auto result = search_->startSearch("test", options);
    ASSERT_TRUE(result.has_value());
    
    const auto& results = search_->getSearchState().getResults();
    ASSERT_GT(results.size(), 0);
    
    const auto& first_result = results[0];
    
    // Verify result structure
    EXPECT_GE(first_result.line, 0);
    EXPECT_LT(first_result.line, buffer_->getLineCount());
    EXPECT_GE(first_result.start_column, 0);
    EXPECT_GT(first_result.end_column, first_result.start_column);
    EXPECT_EQ(first_result.matched_text, "test");
    
    // Context should be provided
    EXPECT_FALSE(first_result.context_before.empty() || first_result.context_after.empty());
    
    // Verify the match actually exists in the buffer
    auto line_result = buffer_->getLine(first_result.line);
    ASSERT_TRUE(line_result.has_value());
    
    std::string line_text = line_result.value();
    std::string extracted = line_text.substr(
        first_result.start_column, 
        first_result.end_column - first_result.start_column);
    EXPECT_EQ(extracted, first_result.matched_text);
}

// Cancel and cleanup tests

TEST_F(EditorSearchTest, CancelSearch) {
    SearchOptions options;
    auto result = search_->startSearch("test", options);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(search_->isSearchActive());
    
    search_->cancelSearch();
    EXPECT_FALSE(search_->isSearchActive());
    EXPECT_EQ(search_->getSearchState().getResultCount(), 0);
}

TEST_F(EditorSearchTest, MultipleSearches) {
    SearchOptions options;
    
    // First search
    auto result1 = search_->startSearch("test", options);
    ASSERT_TRUE(result1.has_value());
    size_t first_count = search_->getSearchState().getResultCount();
    
    // Second search should replace first
    auto result2 = search_->startSearch("Hello", options);
    ASSERT_TRUE(result2.has_value());
    size_t second_count = search_->getSearchState().getResultCount();
    
    EXPECT_NE(first_count, second_count);
    EXPECT_EQ(search_->getSearchState().getLastQuery(), "Hello");
}

// SearchValidator tests

TEST_F(EditorSearchTest, SearchValidatorBasic) {
    SearchOptions options;
    
    // Valid queries
    auto result1 = SearchValidator::validateQuery("test", options);
    EXPECT_TRUE(result1.has_value());
    
    auto result2 = SearchValidator::validateQuery("", options);
    EXPECT_TRUE(result2.has_value()); // Empty query is allowed
    
    // Query too long
    std::string long_query(2000, 'a'); // Exceeds MAX_QUERY_LENGTH
    auto result3 = SearchValidator::validateQuery(long_query, options);
    EXPECT_FALSE(result3.has_value());
}

TEST_F(EditorSearchTest, RegexValidation) {
    // Valid regex patterns
    auto result1 = SearchValidator::validateRegexPattern("[a-z]+");
    EXPECT_TRUE(result1.has_value());
    
    auto result2 = SearchValidator::validateRegexPattern("test|hello");
    EXPECT_TRUE(result2.has_value());
    
    // Invalid regex syntax
    auto result3 = SearchValidator::validateRegexPattern("[invalid");
    EXPECT_FALSE(result3.has_value());
    
    // Dangerous patterns
    auto result4 = SearchValidator::validateRegexPattern("(.*).*");
    EXPECT_FALSE(result4.has_value());
}

TEST_F(EditorSearchTest, RegexComplexityEstimation) {
    // Simple patterns should have low complexity
    auto complexity1 = SearchValidator::estimateRegexComplexity("test");
    EXPECT_LT(complexity1, 10);
    
    // Complex patterns should have higher complexity
    auto complexity2 = SearchValidator::estimateRegexComplexity("(.*)+.*{100,1000}");
    EXPECT_GE(complexity2, 50);
    
    // Patterns with many quantifiers
    auto complexity3 = SearchValidator::estimateRegexComplexity("a*b+c?d{10}");
    EXPECT_GT(complexity3, complexity1);
}

// Unicode support tests

TEST_F(EditorSearchTest, UnicodeSearch) {
    SearchOptions options;
    
    // Search for accented characters
    auto result = search_->startSearch("café", options);
    ASSERT_TRUE(result.has_value());
    
    const auto& results = search_->getSearchState().getResults();
    EXPECT_GT(results.size(), 0);
    
    // Verify the match
    bool found_cafe = false;
    for (const auto& match : results) {
        if (match.matched_text == "café") {
            found_cafe = true;
            EXPECT_EQ(match.line, 4); // "Unicode test: café naïve résumé"
        }
    }
    EXPECT_TRUE(found_cafe);
}

// Edge cases

TEST_F(EditorSearchTest, SearchInEmptyBuffer) {
    // Create empty buffer
    EditorBuffer::Config buffer_config;
    auto empty_buffer = std::make_unique<EditorBuffer>(buffer_config);
    
    EditorSearch empty_search(empty_buffer.get());
    
    SearchOptions options;
    auto result = empty_search.startSearch("test", options);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(empty_search.getSearchState().getResultCount(), 0);
}

TEST_F(EditorSearchTest, SearchAtBufferBoundaries) {
    SearchOptions options;
    
    // Search for text at the very beginning
    auto result1 = search_->startSearch("Hello", options);
    ASSERT_TRUE(result1.has_value());
    
    const auto& results1 = search_->getSearchState().getResults();
    bool found_at_start = false;
    for (const auto& match : results1) {
        if (match.line == 0 && match.start_column == 0) {
            found_at_start = true;
        }
    }
    EXPECT_TRUE(found_at_start);
    
    // Search for text at the very end
    auto result2 = search_->startSearch("again", options);
    ASSERT_TRUE(result2.has_value());
    
    const auto& results2 = search_->getSearchState().getResults();
    bool found_at_end = false;
    for (const auto& match : results2) {
        if (match.line == 7) { // Last line
            found_at_end = true;
        }
    }
    EXPECT_TRUE(found_at_end);
}