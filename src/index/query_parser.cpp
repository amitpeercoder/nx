#include "nx/index/query_parser.hpp"

#include <regex>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace nx::index {

Result<SearchQuery> QueryParser::parse(const std::string& query_str) {
  if (query_str.empty()) {
    return SearchQuery{}; // Empty query
  }
  
  try {
    auto tokens = tokenize(query_str);
    return buildQuery(tokens);
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Query parse error: " + std::string(e.what())));
  }
}

std::vector<QueryParser::Token> QueryParser::tokenize(const std::string& query_str) {
  std::vector<Token> tokens;
  
  // Regex patterns for different token types
  std::regex field_regex("(-?)(\\w+):([^:\\s]+(?:\\.\\.[^:\\s]+)?)");
  std::regex quoted_regex("(-?)(\\w+):\"([^\"]*)\"");
  std::regex simple_quoted_regex("\"([^\"]*)\"");
  std::regex word_regex("\\S+");
  
  std::string remaining = query_str;
  std::smatch match;
  
  while (!remaining.empty()) {
    // Skip whitespace
    size_t start = remaining.find_first_not_of(" \t\n");
    if (start == std::string::npos) break;
    remaining = remaining.substr(start);
    
    bool matched = false;
    
    // Try quoted field pattern: [(-)]field:"value"
    if (std::regex_search(remaining, match, quoted_regex)) {
      if (match.position() == 0) {
        Token token;
        token.type = Token::kField;
        token.negated = !match[1].str().empty();
        token.field = match[2].str();
        token.value = match[3].str();
        tokens.push_back(token);
        
        remaining = remaining.substr(match.length());
        matched = true;
      }
    }
    
    // Try field pattern: [(-)]field:value[..value2]
    if (!matched && std::regex_search(remaining, match, field_regex)) {
      if (match.position() == 0) {
        Token token;
        token.negated = !match[1].str().empty();
        token.field = match[2].str();
        std::string value = match[3].str();
        
        // Check for range syntax (value..value2)
        size_t range_pos = value.find("..");
        if (range_pos != std::string::npos) {
          token.type = Token::kRange;
          token.value = value.substr(0, range_pos);
          token.value2 = value.substr(range_pos + 2);
        } else {
          token.type = Token::kField;
          token.value = value;
        }
        
        tokens.push_back(token);
        remaining = remaining.substr(match.length());
        matched = true;
      }
    }
    
    // Try simple quoted text: "value"
    if (!matched && std::regex_search(remaining, match, simple_quoted_regex)) {
      if (match.position() == 0) {
        Token token;
        token.type = Token::kQuoted;
        token.value = match[1].str();
        tokens.push_back(token);
        
        remaining = remaining.substr(match.length());
        matched = true;
      }
    }
    
    // Try regular word
    if (!matched && std::regex_search(remaining, match, word_regex)) {
      if (match.position() == 0) {
        Token token;
        token.type = Token::kText;
        token.value = match[0].str();
        tokens.push_back(token);
        
        remaining = remaining.substr(match.length());
        matched = true;
      }
    }
    
    if (!matched) {
      // Skip one character and continue
      remaining = remaining.substr(1);
    }
  }
  
  return tokens;
}

SearchQuery QueryParser::buildQuery(const std::vector<Token>& tokens) {
  SearchQuery query;
  std::vector<std::string> text_parts;
  std::vector<std::string> exclude_tags;
  
  for (const auto& token : tokens) {
    switch (token.type) {
      case Token::kText:
      case Token::kQuoted:
        text_parts.push_back(token.value);
        break;
        
      case Token::kField:
        if (token.field == "tag") {
          if (token.negated) {
            exclude_tags.push_back(token.value);
          } else {
            query.tags.push_back(token.value);
          }
        } else if (token.field == "notebook") {
          if (!token.negated) {
            query.notebook = token.value;
          }
        } else if (token.field == "title") {
          if (!token.negated) {
            text_parts.push_back("title:\"" + token.value + "\"");
          }
        } else if (token.field == "content") {
          if (!token.negated) {
            text_parts.push_back(token.value);
          }
        } else if (token.field == "since" || token.field == "after") {
          if (isDateString(token.value)) {
            auto date = parseDate(token.value);
            if (!token.negated) {
              query.since = date;
            }
          }
        } else if (token.field == "until" || token.field == "before") {
          if (isDateString(token.value)) {
            auto date = parseDate(token.value);
            if (!token.negated) {
              query.until = date;
            }
          }
        }
        break;
        
      case Token::kRange:
        if (token.field == "date" || token.field == "created" || token.field == "modified") {
          if (isDateString(token.value) && isDateString(token.value2)) {
            query.since = parseDate(token.value);
            query.until = parseDate(token.value2);
          }
        }
        break;
    }
  }
  
  // Combine text parts
  if (!text_parts.empty()) {
    std::ostringstream oss;
    for (size_t i = 0; i < text_parts.size(); ++i) {
      if (i > 0) oss << " ";
      oss << text_parts[i];
    }
    query.text = oss.str();
  }
  
  // Handle excluded tags by adding them as negative filters to the query text
  for (const auto& exclude_tag : exclude_tags) {
    if (!query.text.empty()) {
      query.text += " ";
    }
    query.text += "-tags:\"" + exclude_tag + "\"";
  }
  
  return query;
}

std::string QueryParser::unquote(const std::string& str) {
  if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
    return str.substr(1, str.length() - 2);
  }
  return str;
}

bool QueryParser::isDateString(const std::string& str) {
  // Basic date format validation (YYYY-MM-DD or YYYY-MM-DD HH:MM:SS)
  std::regex date_regex(R"(\d{4}-\d{2}-\d{2}(?:\s+\d{2}:\d{2}:\d{2})?)");
  return std::regex_match(str, date_regex);
}

std::chrono::system_clock::time_point QueryParser::parseDate(const std::string& str) {
  std::tm tm = {};
  std::istringstream ss(str);
  
  // Try YYYY-MM-DD HH:MM:SS format first
  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
  if (ss.fail()) {
    // Fall back to YYYY-MM-DD format
    ss.clear();
    ss.str(str);
    ss >> std::get_time(&tm, "%Y-%m-%d");
  }
  
  if (ss.fail()) {
    // Return epoch on parse failure
    return std::chrono::system_clock::time_point{};
  }
  
  return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// QueryBuilder implementation

QueryBuilder& QueryBuilder::text(const std::string& text) {
  query_.text = text;
  return *this;
}

QueryBuilder& QueryBuilder::tag(const std::string& tag) {
  query_.tags.push_back(tag);
  return *this;
}

QueryBuilder& QueryBuilder::excludeTag(const std::string& tag) {
  exclude_tags_.push_back(tag);
  return *this;
}

QueryBuilder& QueryBuilder::notebook(const std::string& notebook) {
  query_.notebook = notebook;
  return *this;
}

QueryBuilder& QueryBuilder::title(const std::string& title) {
  if (!query_.text.empty()) {
    query_.text += " ";
  }
  query_.text += "title:\"" + title + "\"";
  return *this;
}

QueryBuilder& QueryBuilder::createdAfter(std::chrono::system_clock::time_point date) {
  query_.since = date;
  return *this;
}

QueryBuilder& QueryBuilder::createdBefore(std::chrono::system_clock::time_point date) {
  query_.until = date;
  return *this;
}

QueryBuilder& QueryBuilder::modifiedAfter(std::chrono::system_clock::time_point date) {
  query_.since = date;
  return *this;
}

QueryBuilder& QueryBuilder::modifiedBefore(std::chrono::system_clock::time_point date) {
  query_.until = date;
  return *this;
}

QueryBuilder& QueryBuilder::limit(size_t limit) {
  query_.limit = limit;
  return *this;
}

QueryBuilder& QueryBuilder::offset(size_t offset) {
  query_.offset = offset;
  return *this;
}

QueryBuilder& QueryBuilder::highlight(bool enable) {
  query_.highlight = enable;
  return *this;
}

SearchQuery QueryBuilder::build() const {
  SearchQuery result = query_;
  
  // Add excluded tags to the text query
  for (const auto& exclude_tag : exclude_tags_) {
    if (!result.text.empty()) {
      result.text += " ";
    }
    result.text += "-tags:\"" + exclude_tag + "\"";
  }
  
  return result;
}

} // namespace nx::index