#include <gtest/gtest.h>

#include "nx/index/query_parser.hpp"

using namespace nx::index;

class QueryParserTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(QueryParserTest, ParseSimpleText) {
  auto result = QueryParser::parse("hello world");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_EQ(query.text, "hello world");
  EXPECT_TRUE(query.tags.empty());
  EXPECT_FALSE(query.notebook.has_value());
}

TEST_F(QueryParserTest, ParseEmptyQuery) {
  auto result = QueryParser::parse("");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_TRUE(query.text.empty());
  EXPECT_TRUE(query.tags.empty());
  EXPECT_FALSE(query.notebook.has_value());
}

TEST_F(QueryParserTest, ParseTagFilter) {
  auto result = QueryParser::parse("tag:programming content");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_EQ(query.text, "content");
  ASSERT_EQ(query.tags.size(), 1);
  EXPECT_EQ(query.tags[0], "programming");
}

TEST_F(QueryParserTest, ParseMultipleTags) {
  auto result = QueryParser::parse("tag:programming tag:cpp algorithms");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_EQ(query.text, "algorithms");
  ASSERT_EQ(query.tags.size(), 2);
  EXPECT_EQ(query.tags[0], "programming");
  EXPECT_EQ(query.tags[1], "cpp");
}

TEST_F(QueryParserTest, ParseNotebookFilter) {
  auto result = QueryParser::parse("notebook:work meeting notes");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_EQ(query.text, "meeting notes");
  ASSERT_TRUE(query.notebook.has_value());
  EXPECT_EQ(*query.notebook, "work");
}

TEST_F(QueryParserTest, ParseQuotedValues) {
  auto result = QueryParser::parse(R"(tag:"complex tag" title:"My Note")");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_EQ(query.text, R"(title:"My Note")");
  ASSERT_EQ(query.tags.size(), 1);
  EXPECT_EQ(query.tags[0], "complex tag");
}

TEST_F(QueryParserTest, ParseQuotedText) {
  auto result = QueryParser::parse(R"("exact phrase" other words)");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_EQ(query.text, "exact phrase other words");
}

TEST_F(QueryParserTest, ParseNegatedTag) {
  auto result = QueryParser::parse("-tag:draft content");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_EQ(query.text, R"(content -tags:"draft")");
  EXPECT_TRUE(query.tags.empty()); // Negated tags don't go in tags field
}

TEST_F(QueryParserTest, ParseDateRange) {
  auto result = QueryParser::parse("date:2024-01-01..2024-12-31 content");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_EQ(query.text, "content");
  EXPECT_TRUE(query.since.has_value());
  EXPECT_TRUE(query.until.has_value());
}

TEST_F(QueryParserTest, ParseComplexQuery) {
  auto result = QueryParser::parse(R"(tag:programming tag:tutorial -tag:draft notebook:learning "data structures" algorithms)");
  ASSERT_TRUE(result.has_value());
  
  const auto& query = *result;
  EXPECT_EQ(query.text, R"(data structures algorithms -tags:"draft")");
  ASSERT_EQ(query.tags.size(), 2);
  EXPECT_EQ(query.tags[0], "programming");
  EXPECT_EQ(query.tags[1], "tutorial");
  ASSERT_TRUE(query.notebook.has_value());
  EXPECT_EQ(*query.notebook, "learning");
}

// QueryBuilder tests

TEST_F(QueryParserTest, QueryBuilderBasic) {
  auto query = QueryBuilder()
    .text("hello world")
    .tag("programming")
    .notebook("work")
    .limit(10)
    .offset(5)
    .highlight(true)
    .build();
  
  EXPECT_EQ(query.text, "hello world");
  ASSERT_EQ(query.tags.size(), 1);
  EXPECT_EQ(query.tags[0], "programming");
  ASSERT_TRUE(query.notebook.has_value());
  EXPECT_EQ(*query.notebook, "work");
  EXPECT_EQ(query.limit, 10);
  EXPECT_EQ(query.offset, 5);
  EXPECT_TRUE(query.highlight);
}

TEST_F(QueryParserTest, QueryBuilderWithDates) {
  auto now = std::chrono::system_clock::now();
  auto yesterday = now - std::chrono::hours(24);
  
  auto query = QueryBuilder()
    .text("content")
    .createdAfter(yesterday)
    .modifiedBefore(now)
    .build();
  
  EXPECT_EQ(query.text, "content");
  ASSERT_TRUE(query.since.has_value());
  ASSERT_TRUE(query.until.has_value());
  EXPECT_EQ(*query.since, yesterday);
  EXPECT_EQ(*query.until, now);
}

TEST_F(QueryParserTest, QueryBuilderExcludeTags) {
  auto query = QueryBuilder()
    .text("content")
    .tag("programming")
    .excludeTag("draft")
    .excludeTag("incomplete")
    .build();
  
  EXPECT_EQ(query.text, R"(content -tags:"draft" -tags:"incomplete")");
  ASSERT_EQ(query.tags.size(), 1);
  EXPECT_EQ(query.tags[0], "programming");
}

TEST_F(QueryParserTest, QueryBuilderTitleSearch) {
  auto query = QueryBuilder()
    .title("My Important Note")
    .tag("important")
    .build();
  
  EXPECT_EQ(query.text, R"(title:"My Important Note")");
  ASSERT_EQ(query.tags.size(), 1);
  EXPECT_EQ(query.tags[0], "important");
}