#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include "nx/common.hpp"
#include "nx/index/index.hpp"

namespace nx::index {

/**
 * @brief Query parser for converting natural language queries to structured SearchQuery
 * 
 * Supports parsing queries like:
 * - "hello world" -> simple text search
 * - "tag:programming content:algorithms" -> tag and content filters
 * - "notebook:work created:2024-01-01..2024-12-31" -> notebook and date range
 * - "title:\"My Note\" -tag:draft" -> title search excluding draft tag
 */
class QueryParser {
public:
  /**
   * @brief Parse a query string into a structured SearchQuery
   * @param query_str The query string to parse
   * @return SearchQuery on success, Error on parse failure
   */
  static Result<SearchQuery> parse(const std::string& query_str);

private:
  struct Token {
    enum Type {
      kText,      // Regular text
      kField,     // field:value
      kQuoted,    // "quoted text"
      kRange,     // field:start..end
    };
    
    Type type;
    std::string field;
    std::string value;
    std::string value2; // For ranges
    bool negated = false;
  };
  
  static std::vector<Token> tokenize(const std::string& query_str);
  static SearchQuery buildQuery(const std::vector<Token>& tokens);
  static std::string unquote(const std::string& str);
  static bool isDateString(const std::string& str);
  static std::chrono::system_clock::time_point parseDate(const std::string& str);
};

/**
 * @brief Query builder for programmatically constructing SearchQuery objects
 */
class QueryBuilder {
public:
  QueryBuilder() = default;
  
  QueryBuilder& text(const std::string& text);
  QueryBuilder& tag(const std::string& tag);
  QueryBuilder& excludeTag(const std::string& tag);
  QueryBuilder& notebook(const std::string& notebook);
  QueryBuilder& title(const std::string& title);
  QueryBuilder& createdAfter(std::chrono::system_clock::time_point date);
  QueryBuilder& createdBefore(std::chrono::system_clock::time_point date);
  QueryBuilder& modifiedAfter(std::chrono::system_clock::time_point date);
  QueryBuilder& modifiedBefore(std::chrono::system_clock::time_point date);
  QueryBuilder& limit(size_t limit);
  QueryBuilder& offset(size_t offset);
  QueryBuilder& highlight(bool enable = true);
  
  SearchQuery build() const;
  
private:
  SearchQuery query_;
  std::vector<std::string> exclude_tags_;
};

} // namespace nx::index