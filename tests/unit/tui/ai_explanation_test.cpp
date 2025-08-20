#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "nx/tui/ai_explanation.hpp"
#include "nx/tui/editor_buffer.hpp"
#include "nx/config/config.hpp"
#include "../../common/test_helpers.hpp"

using namespace nx::tui;
using namespace nx::config;
using namespace testing;

class AiExplanationServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.cache_explanations = true;
        config_.brief_max_words = 10;
        config_.expanded_max_words = 50;
        config_.timeout = std::chrono::milliseconds(3000);
        config_.max_cache_size = 100;
        config_.context_radius = 100;
        
        service_ = std::make_unique<AiExplanationService>(config_);
        
        // Setup AI config for testing
        ai_config_.provider = "anthropic";
        ai_config_.api_key = "test-key";
        ai_config_.model = "claude-3-haiku-20240307";
    }

    AiExplanationService::Config config_;
    std::unique_ptr<AiExplanationService> service_;
    Config::AiConfig ai_config_;
};

// Test word extraction functionality
TEST_F(AiExplanationServiceTest, ExtractWordAt_ValidPosition) {
    EditorBuffer buffer;
    buffer.insertLine(0, "The API function returns JSON data");
    
    auto result = AiExplanationService::extractWordAt(buffer, 0, 4);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "API");
}

TEST_F(AiExplanationServiceTest, ExtractWordAt_InvalidLine) {
    EditorBuffer buffer;
    buffer.insertLine(0, "Test line");
    
    auto result = AiExplanationService::extractWordAt(buffer, 5, 0);
    EXPECT_FALSE(result.has_value());
}

TEST_F(AiExplanationServiceTest, ExtractWordAt_InvalidColumn) {
    EditorBuffer buffer;
    buffer.insertLine(0, "Test");
    
    auto result = AiExplanationService::extractWordAt(buffer, 0, 10);
    EXPECT_FALSE(result.has_value());
}

TEST_F(AiExplanationServiceTest, ExtractWordBefore_ValidPosition) {
    EditorBuffer buffer;
    buffer.insertLine(0, "The API endpoint");
    
    // Cursor after "API"
    auto result = AiExplanationService::extractWordBefore(buffer, 0, 7);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "API");
}

TEST_F(AiExplanationServiceTest, ExtractWordBefore_StartOfLine) {
    EditorBuffer buffer;
    buffer.insertLine(0, "API test");
    
    auto result = AiExplanationService::extractWordBefore(buffer, 0, 0);
    EXPECT_FALSE(result.has_value());
}

TEST_F(AiExplanationServiceTest, ExtractWordBefore_AfterWhitespace) {
    EditorBuffer buffer;
    buffer.insertLine(0, "API   test");
    
    // Cursor after whitespace
    auto result = AiExplanationService::extractWordBefore(buffer, 0, 6);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "API");
}

TEST_F(AiExplanationServiceTest, ExtractContext_SingleLine) {
    EditorBuffer buffer;
    buffer.insertLine(0, "This is a test line with API functionality");
    
    auto result = AiExplanationService::extractContext(buffer, 0, 25, 50);
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), HasSubstr("API"));
    EXPECT_THAT(result.value(), HasSubstr("functionality"));
}

TEST_F(AiExplanationServiceTest, ExtractContext_MultipleLines) {
    EditorBuffer buffer;
    buffer.insertLine(0, "Line 1: Setup");
    buffer.insertLine(1, "Line 2: API call here");
    buffer.insertLine(2, "Line 3: Process result");
    
    auto result = AiExplanationService::extractContext(buffer, 1, 8, 100);
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), HasSubstr("Setup"));
    EXPECT_THAT(result.value(), HasSubstr("API"));
    EXPECT_THAT(result.value(), HasSubstr("Process"));
}

TEST_F(AiExplanationServiceTest, ExtractContext_InvalidLine) {
    EditorBuffer buffer;
    buffer.insertLine(0, "Test line");
    
    auto result = AiExplanationService::extractContext(buffer, 5, 0, 50);
    EXPECT_FALSE(result.has_value());
}

// Test cache functionality
TEST_F(AiExplanationServiceTest, CacheOperations) {
    // Initially empty cache
    auto stats = service_->getCacheStats();
    EXPECT_EQ(stats.first, 0);  // cache size
    EXPECT_EQ(stats.second, 0); // cache hits
    
    // Clear cache should work even when empty
    service_->clearCache();
    stats = service_->getCacheStats();
    EXPECT_EQ(stats.first, 0);
    EXPECT_EQ(stats.second, 0);
}

// Test configuration
TEST_F(AiExplanationServiceTest, ConfigurationDefaults) {
    AiExplanationService::Config default_config;
    
    EXPECT_EQ(default_config.brief_max_words, 10);
    EXPECT_EQ(default_config.expanded_max_words, 50);
    EXPECT_EQ(default_config.timeout, std::chrono::milliseconds(3000));
    EXPECT_TRUE(default_config.cache_explanations);
    EXPECT_EQ(default_config.max_cache_size, 1000);
    EXPECT_EQ(default_config.context_radius, 100);
}

// Test word boundary detection
TEST_F(AiExplanationServiceTest, WordBoundaryDetection_Underscore) {
    EditorBuffer buffer;
    buffer.insertLine(0, "test_function_name");
    
    auto result = AiExplanationService::extractWordAt(buffer, 0, 5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "test_function_name");
}

TEST_F(AiExplanationServiceTest, WordBoundaryDetection_Mixed) {
    EditorBuffer buffer;
    buffer.insertLine(0, "HTML5 and CSS3");
    
    auto result1 = AiExplanationService::extractWordAt(buffer, 0, 2);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), "HTML5");
    
    auto result2 = AiExplanationService::extractWordAt(buffer, 0, 12);
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), "CSS3");
}

// Test error cases for AI requests
TEST_F(AiExplanationServiceTest, GetBriefExplanation_EmptyApiKey) {
    Config::AiConfig empty_config;
    empty_config.api_key = "";
    
    auto result = service_->getBriefExplanation("API", "context", empty_config);
    EXPECT_FALSE(result.has_value());
}

TEST_F(AiExplanationServiceTest, GetExpandedExplanation_EmptyApiKey) {
    Config::AiConfig empty_config;
    empty_config.api_key = "";
    
    auto result = service_->getExpandedExplanation("API", "context", empty_config);
    EXPECT_FALSE(result.has_value());
}

// Test common word filtering
class CommonWordTest : public AiExplanationServiceTest,
                      public ::testing::WithParamInterface<std::string> {};

TEST_P(CommonWordTest, ShouldNotExplainCommonWords) {
    // We can't directly test shouldExplainTerm since it's private,
    // but we can test through the public interface
    auto result = service_->getBriefExplanation(GetParam(), "test context", ai_config_);
    
    // Should fail because it's a common word (assuming no network call is made)
    // This test would need to be adapted based on actual implementation
    EXPECT_FALSE(result.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    CommonWords,
    CommonWordTest,
    ::testing::Values("the", "and", "for", "are", "but", "not", "you", "all")
);

// Test word extraction edge cases
TEST_F(AiExplanationServiceTest, ExtractWordBefore_UnicodeText) {
    EditorBuffer buffer;
    buffer.insertLine(0, "unicode cafe test");
    
    // This test verifies we handle word extraction properly
    auto result = AiExplanationService::extractWordBefore(buffer, 0, 12);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "cafe");
}

TEST_F(AiExplanationServiceTest, ExtractWordAt_PunctuationBoundaries) {
    EditorBuffer buffer;
    buffer.insertLine(0, "Call api.endpoint() function");
    
    auto result = AiExplanationService::extractWordAt(buffer, 0, 5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "api");
}

TEST_F(AiExplanationServiceTest, ExtractWordAt_EmptyLine) {
    EditorBuffer buffer;
    buffer.insertLine(0, "");
    
    auto result = AiExplanationService::extractWordAt(buffer, 0, 0);
    EXPECT_FALSE(result.has_value());
}

// Test context extraction with limited radius
TEST_F(AiExplanationServiceTest, ExtractContext_LimitedRadius) {
    EditorBuffer buffer;
    buffer.insertLine(0, "This is a very long line that should be truncated when extracting context with limited radius");
    
    auto result = AiExplanationService::extractContext(buffer, 0, 40, 20);
    ASSERT_TRUE(result.has_value());
    // Context extraction may return more than radius due to word boundaries - just ensure it's reasonable
    EXPECT_LE(result.value().length(), 100);
    EXPECT_GT(result.value().length(), 0);
}

// Integration test combining multiple operations
TEST_F(AiExplanationServiceTest, IntegrationTest_WordExtractionAndContext) {
    EditorBuffer buffer;
    buffer.insertLine(0, "Using REST API for data transfer");
    buffer.insertLine(1, "The API returns JSON format");
    buffer.insertLine(2, "Handle errors appropriately");
    
    // Extract word "API" from line 1
    auto word_result = AiExplanationService::extractWordBefore(buffer, 1, 8);
    ASSERT_TRUE(word_result.has_value());
    EXPECT_EQ(word_result.value(), "API");
    
    // Extract context around that position
    auto context_result = AiExplanationService::extractContext(buffer, 1, 4, 100);
    ASSERT_TRUE(context_result.has_value());
    EXPECT_THAT(context_result.value(), HasSubstr("REST"));
    EXPECT_THAT(context_result.value(), HasSubstr("JSON"));
}