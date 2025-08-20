#include "nx/tui/ai_explanation.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace nx::tui {

AiExplanationService::AiExplanationService(Config config) : config_(std::move(config)) {
}

Result<std::string> AiExplanationService::getBriefExplanation(
    const std::string& term,
    const std::string& context,
    const nx::config::Config::AiConfig& ai_config) {
    
    // Check cache first
    if (config_.cache_explanations) {
        if (const auto* cached = getCachedExplanation(term)) {
            cache_hits_++;
            return cached->brief;
        }
    }
    
    return makeAiRequest(term, context, ai_config, false);
}

Result<std::string> AiExplanationService::getExpandedExplanation(
    const std::string& term,
    const std::string& context,
    const nx::config::Config::AiConfig& ai_config) {
    
    // Check cache first
    if (config_.cache_explanations) {
        if (const auto* cached = getCachedExplanation(term)) {
            cache_hits_++;
            return cached->expanded;
        }
    }
    
    return makeAiRequest(term, context, ai_config, true);
}

Result<AiExplanationService::ExplanationResult> AiExplanationService::explainTerm(
    const std::string& term,
    const std::string& context,
    const nx::config::Config::AiConfig& ai_config) {
    
    ExplanationResult result;
    result.term = term;
    
    // Check cache first
    if (config_.cache_explanations) {
        if (const auto* cached = getCachedExplanation(term)) {
            cache_hits_++;
            result.brief = cached->brief;
            result.expanded = cached->expanded;
            result.is_cached = true;
            return result;
        }
    }
    
    // Get both explanations from AI
    auto brief_result = makeAiRequest(term, context, ai_config, false);
    if (!brief_result.has_value()) {
        return std::unexpected(brief_result.error());
    }
    
    auto expanded_result = makeAiRequest(term, context, ai_config, true);
    if (!expanded_result.has_value()) {
        return std::unexpected(expanded_result.error());
    }
    
    result.brief = *brief_result;
    result.expanded = *expanded_result;
    result.is_cached = false;
    
    // Cache the results
    if (config_.cache_explanations) {
        cacheExplanation(term, result.brief, result.expanded);
    }
    
    return result;
}

Result<std::string> AiExplanationService::extractWordAt(
    const EditorBuffer& buffer, size_t line, size_t col) {
    
    auto line_result = buffer.getLine(line);
    if (!line_result.has_value()) {
        return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Invalid line number"));
    }
    
    const std::string& line_text = *line_result;
    if (col >= line_text.length()) {
        return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Column out of bounds"));
    }
    
    // Find word boundaries
    size_t start = col;
    size_t end = col;
    
    // Move start backward to find word start
    while (start > 0 && (std::isalnum(line_text[start - 1]) || line_text[start - 1] == '_')) {
        start--;
    }
    
    // Move end forward to find word end
    while (end < line_text.length() && (std::isalnum(line_text[end]) || line_text[end] == '_')) {
        end++;
    }
    
    if (start == end) {
        return std::unexpected(makeError(ErrorCode::kNotFound, "No word at cursor position"));
    }
    
    return line_text.substr(start, end - start);
}

Result<std::string> AiExplanationService::extractWordBefore(
    const EditorBuffer& buffer, size_t line, size_t col) {
    
    auto line_result = buffer.getLine(line);
    if (!line_result.has_value()) {
        return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Invalid line number"));
    }
    
    const std::string& line_text = *line_result;
    if (col > line_text.length()) {
        return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Column out of bounds"));
    }
    
    // Start from position before cursor
    if (col == 0) {
        return std::unexpected(makeError(ErrorCode::kNotFound, "No word before cursor"));
    }
    
    size_t pos = col - 1;
    
    // Skip whitespace backwards
    while (pos > 0 && std::isspace(line_text[pos])) {
        pos--;
    }
    
    if (pos == 0 && std::isspace(line_text[0])) {
        return std::unexpected(makeError(ErrorCode::kNotFound, "No word before cursor"));
    }
    
    // Find word boundaries
    size_t end = pos + 1;
    size_t start = pos;
    
    // Move start backward to find word start
    while (start > 0 && (std::isalnum(line_text[start - 1]) || line_text[start - 1] == '_')) {
        start--;
    }
    
    if (start == end || (!std::isalnum(line_text[start]) && line_text[start] != '_')) {
        return std::unexpected(makeError(ErrorCode::kNotFound, "No word before cursor"));
    }
    
    return line_text.substr(start, end - start);
}

Result<std::string> AiExplanationService::extractContext(
    const EditorBuffer& buffer, size_t line, size_t col, size_t radius) {
    
    size_t line_count = buffer.getLineCount();
    if (line >= line_count) {
        return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Invalid line number"));
    }
    
    std::string context;
    
    // Calculate line range to include
    size_t start_line = (line >= 2) ? line - 2 : 0;
    size_t end_line = std::min(line + 2, line_count - 1);
    
    // Build context from multiple lines
    for (size_t i = start_line; i <= end_line; ++i) {
        auto line_result = buffer.getLine(i);
        if (line_result.has_value()) {
            if (!context.empty()) {
                context += " ";
            }
            context += *line_result;
        }
    }
    
    // Trim context to radius around cursor position
    if (context.length() > radius * 2) {
        size_t cursor_pos_in_context = 0;
        
        // Calculate approximate cursor position in concatenated context
        for (size_t i = start_line; i < line; ++i) {
            auto line_result = buffer.getLine(i);
            if (line_result.has_value()) {
                cursor_pos_in_context += line_result->length() + 1; // +1 for space
            }
        }
        cursor_pos_in_context += col;
        
        // Extract substring around cursor
        size_t start_pos = (cursor_pos_in_context >= radius) ? cursor_pos_in_context - radius : 0;
        size_t length = std::min(radius * 2, context.length() - start_pos);
        
        context = context.substr(start_pos, length);
    }
    
    return context;
}

void AiExplanationService::clearCache() {
    cache_.clear();
    cache_hits_ = 0;
}

std::pair<size_t, size_t> AiExplanationService::getCacheStats() const {
    return {cache_.size(), cache_hits_};
}

Result<std::string> AiExplanationService::makeAiRequest(
    const std::string& term,
    const std::string& context,
    const nx::config::Config::AiConfig& ai_config,
    bool is_expanded) {
    
    // Check if term should be explained
    if (!shouldExplainTerm(term)) {
        return std::unexpected(makeError(ErrorCode::kInvalidArgument, 
                                       "Term too common or short to explain"));
    }
    
    // Prepare the request payload for Anthropic API
    nlohmann::json request_body;
    request_body["model"] = ai_config.model;
    request_body["max_tokens"] = is_expanded ? 256 : 64;
    
    std::string system_prompt;
    if (is_expanded) {
        system_prompt = "You are a technical writing assistant. Given a term and its context, "
                       "provide a detailed explanation (2-3 sentences) that includes the term's "
                       "meaning, significance, and common usage. Be clear and educational. "
                       "Return ONLY the explanation, no quotes or formatting.";
    } else {
        system_prompt = "You are a technical writing assistant. Given a term and its context, "
                       "provide a very brief explanation (5-10 words maximum) of what the term means. "
                       "Return ONLY the explanation, no quotes or formatting.";
    }
    
    request_body["system"] = system_prompt;
    
    nlohmann::json messages = nlohmann::json::array();
    nlohmann::json user_message;
    user_message["role"] = "user";
    
    std::string content = "Term: \"" + term + "\"\n\nContext:\n" + context + 
                         "\n\nProvide " + (is_expanded ? "a detailed" : "a brief") + 
                         " explanation of the term:";
    
    user_message["content"] = content;
    messages.push_back(user_message);
    
    request_body["messages"] = messages;
    
    // Make HTTP request to Anthropic API
    nx::util::HttpClient client;
    
    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "x-api-key: " + ai_config.api_key,
        "anthropic-version: 2023-06-01"
    };
    
    auto response = client.post("https://api.anthropic.com/v1/messages", 
                               request_body.dump(), headers);
    
    if (!response.has_value()) {
        return std::unexpected(makeError(ErrorCode::kNetworkError, 
                                       "Failed to call Anthropic API: " + response.error().message()));
    }
    
    if (response->status_code != 200) {
        return std::unexpected(makeError(ErrorCode::kNetworkError, 
                                       "Anthropic API returned error " + std::to_string(response->status_code) + 
                                       ": " + response->body));
    }
    
    // Parse response
    try {
        auto response_json = nlohmann::json::parse(response->body);
        
        if (response_json.contains("error")) {
            std::string error_message = "Anthropic API error";
            if (response_json["error"].contains("message")) {
                error_message = response_json["error"]["message"];
            }
            return std::unexpected(makeError(ErrorCode::kNetworkError, error_message));
        }
        
        if (!response_json.contains("content") || !response_json["content"].is_array() || 
            response_json["content"].empty()) {
            return std::unexpected(makeError(ErrorCode::kParseError, 
                                           "Invalid response format from Anthropic API"));
        }
        
        auto content_item = response_json["content"][0];
        if (!content_item.contains("text")) {
            return std::unexpected(makeError(ErrorCode::kParseError, 
                                           "No text content in Anthropic API response"));
        }
        
        std::string explanation = content_item["text"];
        
        // Clean and validate the explanation
        size_t max_words = is_expanded ? config_.expanded_max_words : config_.brief_max_words;
        return cleanExplanation(explanation, max_words);
        
    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected(makeError(ErrorCode::kParseError, 
                                       "Failed to parse Anthropic API response: " + std::string(e.what())));
    }
}

const AiExplanationService::CacheEntry* AiExplanationService::getCachedExplanation(
    const std::string& term) const {
    
    auto it = cache_.find(term);
    if (it != cache_.end()) {
        return &it->second;
    }
    return nullptr;
}

void AiExplanationService::cacheExplanation(
    const std::string& term,
    const std::string& brief,
    const std::string& expanded) {
    
    // Clean cache if it's getting too large
    if (cache_.size() >= config_.max_cache_size) {
        cleanCache();
    }
    
    cache_.emplace(term, CacheEntry(brief, expanded));
}

void AiExplanationService::cleanCache() {
    if (cache_.size() <= config_.max_cache_size / 2) {
        return;
    }
    
    // Remove oldest entries (simple LRU-like cleanup)
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::hours(24); // Remove entries older than 24 hours
    
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.timestamp < cutoff) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
    
    // If still too large, remove half randomly
    if (cache_.size() > config_.max_cache_size / 2) {
        size_t to_remove = cache_.size() - config_.max_cache_size / 2;
        auto it = cache_.begin();
        for (size_t i = 0; i < to_remove && it != cache_.end(); ++i) {
            it = cache_.erase(it);
        }
    }
}

std::string AiExplanationService::cleanExplanation(
    const std::string& explanation, size_t max_words) const {
    
    std::string cleaned = explanation;
    
    // Remove quotes if present
    if (cleaned.front() == '"' && cleaned.back() == '"') {
        cleaned = cleaned.substr(1, cleaned.length() - 2);
    }
    
    // Trim whitespace
    cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r"));
    cleaned.erase(cleaned.find_last_not_of(" \t\n\r") + 1);
    
    // Limit word count
    std::istringstream iss(cleaned);
    std::string word;
    std::string result;
    size_t word_count = 0;
    
    while (iss >> word && word_count < max_words) {
        if (!result.empty()) {
            result += " ";
        }
        result += word;
        word_count++;
    }
    
    return result;
}

bool AiExplanationService::shouldExplainTerm(const std::string& term) const {
    // Don't explain very short terms or common words
    if (term.length() < 2) {
        return false;
    }
    
    // List of common words that don't need explanation
    static const std::unordered_set<std::string> common_words = {
        "the", "and", "for", "are", "but", "not", "you", "all", "can", "had", 
        "her", "was", "one", "our", "out", "day", "get", "use", "man", "new",
        "now", "old", "see", "him", "two", "way", "who", "its", "did", "yes",
        "his", "has", "how", "put", "end", "why", "try", "god", "six", "dog",
        "eat", "ago", "sit", "fun", "bad", "yet", "arm", "far", "off", "ill",
        "own", "say", "run", "let", "big", "car", "top", "cut", "cry", "got"
    };
    
    std::string lower_term = term;
    std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(), ::tolower);
    
    return common_words.find(lower_term) == common_words.end();
}

} // namespace nx::tui