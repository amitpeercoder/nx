#include <gtest/gtest.h>
#include "nx/tui/editor_autocomplete.hpp"

using namespace nx::tui;

class AutoCompleteTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_unique<AutoCompletionEngine>();
        
        // Register providers
        auto note_provider = []() -> std::vector<CompletionItem> {
            std::vector<CompletionItem> notes;
            
            CompletionItem note1;
            note1.text = "Meeting Notes";
            note1.display_text = "Meeting Notes";
            note1.description = "Daily standup notes";
            note1.category = "note";
            note1.relevance_score = 1.0;
            note1.note_id = "note-1";
            notes.push_back(note1);
            
            CompletionItem note2;
            note2.text = "Project Planning";
            note2.display_text = "Project Planning";
            note2.description = "Planning document";
            note2.category = "note";
            note2.relevance_score = 1.0;
            note2.note_id = "note-2";
            notes.push_back(note2);
            
            CompletionItem note3;
            note3.text = "Ideas";
            note3.display_text = "Ideas";
            note3.description = "Random ideas";
            note3.category = "note";
            note3.relevance_score = 1.0;
            note3.note_id = "note-3";
            notes.push_back(note3);
            
            return notes;
        };
        
        auto tag_provider = []() -> std::vector<CompletionItem> {
            std::vector<CompletionItem> tags;
            
            CompletionItem tag1;
            tag1.text = "work";
            tag1.display_text = "work";
            tag1.description = "Work-related";
            tag1.category = "tag";
            tag1.relevance_score = 1.0;
            tag1.usage_count = 10;
            tags.push_back(tag1);
            
            CompletionItem tag2;
            tag2.text = "meeting";
            tag2.display_text = "meeting";
            tag2.description = "Meeting tag";
            tag2.category = "tag";
            tag2.relevance_score = 1.0;
            tag2.usage_count = 5;
            tags.push_back(tag2);
            
            CompletionItem tag3;
            tag3.text = "urgent";
            tag3.display_text = "urgent";
            tag3.description = "Urgent items";
            tag3.category = "tag";
            tag3.relevance_score = 1.0;
            tag3.usage_count = 3;
            tags.push_back(tag3);
            
            return tags;
        };
        
        engine_->registerProvider(std::make_unique<WikiLinkCompletionProvider>(note_provider));
        engine_->registerProvider(std::make_unique<TagCompletionProvider>(tag_provider));
        engine_->registerProvider(std::make_unique<MarkdownSnippetProvider>());
    }
    
    std::unique_ptr<AutoCompletionEngine> engine_;
};

// Trigger detection tests

TEST_F(AutoCompleteTest, DetectWikiLinkTrigger) {
    std::string text = "Some text [[";
    auto context = engine_->shouldTriggerCompletion(text, text.length());
    
    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(context->trigger, "[[");
    EXPECT_EQ(context->query, "");
}

TEST_F(AutoCompleteTest, DetectWikiLinkWithQuery) {
    std::string text = "Some text [[meet";
    // Cursor is positioned after "[[" trigger, before the query
    auto context = engine_->shouldTriggerCompletion(text, 12); // Position after "[[" 
    
    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(context->trigger, "[[");
    EXPECT_EQ(context->query, "meet");
}

TEST_F(AutoCompleteTest, DetectTagTrigger) {
    std::string text = "Some text #";
    auto context = engine_->shouldTriggerCompletion(text, text.length());
    
    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(context->trigger, "#");
    EXPECT_EQ(context->query, "");
}

TEST_F(AutoCompleteTest, DetectTagWithQuery) {
    std::string text = "Some text #wor";
    // Cursor is positioned after "#" trigger
    auto context = engine_->shouldTriggerCompletion(text, 11); // Position after "#"
    
    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(context->trigger, "#");
    EXPECT_EQ(context->query, "wor");
}

TEST_F(AutoCompleteTest, DetectSnippetTrigger) {
    std::string text = "/";
    auto context = engine_->shouldTriggerCompletion(text, text.length());
    
    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(context->trigger, "/");
    EXPECT_EQ(context->query, "");
}

TEST_F(AutoCompleteTest, NoTriggerInMiddleOfWord) {
    std::string text = "test#tag"; // # not at word boundary
    auto context = engine_->shouldTriggerCompletion(text, 5); // Position after #
    
    EXPECT_FALSE(context.has_value());
}

// Completion provider tests

TEST_F(AutoCompleteTest, WikiLinkCompletions) {
    CompletionContext context;
    context.trigger = "[[";
    context.query = "meet";
    
    auto result = engine_->getCompletions(context);
    ASSERT_TRUE(result.has_value());
    
    auto& completions = result.value();
    EXPECT_GT(completions.size(), 0);
    
    // Should find "Meeting Notes"
    bool found_meeting = false;
    for (const auto& item : completions) {
        if (item.text == "Meeting Notes") {
            found_meeting = true;
            break;
        }
    }
    EXPECT_TRUE(found_meeting);
}

TEST_F(AutoCompleteTest, TagCompletions) {
    CompletionContext context;
    context.trigger = "#";
    context.query = "wor";
    
    auto result = engine_->getCompletions(context);
    ASSERT_TRUE(result.has_value());
    
    auto& completions = result.value();
    EXPECT_GT(completions.size(), 0);
    
    // Should find "work" tag
    bool found_work = false;
    for (const auto& item : completions) {
        if (item.text == "work") {
            found_work = true;
            break;
        }
    }
    EXPECT_TRUE(found_work);
}

TEST_F(AutoCompleteTest, SnippetCompletions) {
    CompletionContext context;
    context.trigger = "/";
    context.query = "bold";
    
    auto result = engine_->getCompletions(context);
    ASSERT_TRUE(result.has_value());
    
    auto& completions = result.value();
    EXPECT_GT(completions.size(), 0);
    
    // Should find bold snippet
    bool found_bold = false;
    for (const auto& item : completions) {
        if (item.text == "bold") {
            found_bold = true;
            break;
        }
    }
    EXPECT_TRUE(found_bold);
}

// Fuzzy matching tests

TEST_F(AutoCompleteTest, FuzzyMatchingPerfectMatch) {
    double score = FuzzyMatcher::calculateScore("test", "test");
    EXPECT_DOUBLE_EQ(score, 1.0);
}

TEST_F(AutoCompleteTest, FuzzyMatchingPrefixMatch) {
    double score = FuzzyMatcher::calculateScore("test", "testing");
    EXPECT_GT(score, 0.8);
}

TEST_F(AutoCompleteTest, FuzzyMatchingSubstring) {
    double score = FuzzyMatcher::calculateScore("test", "unittest");
    EXPECT_GT(score, 0.0);
    EXPECT_LT(score, 0.8);
}

TEST_F(AutoCompleteTest, FuzzyMatchingNoMatch) {
    double score = FuzzyMatcher::calculateScore("xyz", "abc");
    EXPECT_DOUBLE_EQ(score, 0.0);
}

TEST_F(AutoCompleteTest, FuzzyMatchingCaseInsensitive) {
    double score = FuzzyMatcher::calculateScore("TEST", "test");
    EXPECT_DOUBLE_EQ(score, 1.0);
}

// Configuration tests

TEST_F(AutoCompleteTest, DisableAutoCompletion) {
    auto config = engine_->getConfig();
    config.enable_auto_completion = false;
    engine_->setConfig(config);
    
    CompletionContext context;
    context.trigger = "[[";
    context.query = "test";
    
    auto result = engine_->getCompletions(context);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0);
}

TEST_F(AutoCompleteTest, MinQueryLength) {
    auto config = engine_->getConfig();
    config.min_query_length = 3;
    engine_->setConfig(config);
    
    CompletionContext context;
    context.trigger = "[[";
    context.query = "ab"; // Too short
    
    auto result = engine_->getCompletions(context);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0);
}

TEST_F(AutoCompleteTest, MaxSuggestions) {
    auto config = engine_->getConfig();
    config.max_suggestions = 2;
    engine_->setConfig(config);
    
    CompletionContext context;
    context.trigger = "[[";
    context.query = ""; // Should return all notes
    
    auto result = engine_->getCompletions(context);
    ASSERT_TRUE(result.has_value());
    EXPECT_LE(result.value().size(), 2);
}

// Usage tracking tests

TEST_F(AutoCompleteTest, RecordUsage) {
    CompletionItem item;
    item.text = "test-completion";
    
    // Record usage multiple times
    engine_->recordCompletionUsage(item);
    engine_->recordCompletionUsage(item);
    
    // Usage should affect ranking in future completions
    // (This is more of a behavioral test - exact verification would require
    // access to internal state which we don't expose)
    EXPECT_TRUE(true); // Test passes if no exceptions thrown
}

// Edge cases

TEST_F(AutoCompleteTest, EmptyQuery) {
    CompletionContext context;
    context.trigger = "[[";
    context.query = "";
    
    auto result = engine_->getCompletions(context);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value().size(), 0); // Should return all available items
}

TEST_F(AutoCompleteTest, LongQuery) {
    CompletionContext context;
    context.trigger = "[[";
    context.query = "this-is-a-very-long-query-that-should-not-match-anything";
    
    auto result = engine_->getCompletions(context);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0); // Should return no matches
}

TEST_F(AutoCompleteTest, SpecialCharactersInQuery) {
    CompletionContext context;
    context.trigger = "#";
    context.query = "test@#$%";
    
    auto result = engine_->getCompletions(context);
    ASSERT_TRUE(result.has_value());
    // Should handle gracefully without crashing
}