#include "nx/tui/editor_autocomplete.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

namespace nx::tui {

// WikiLinkCompletionProvider Implementation

WikiLinkCompletionProvider::WikiLinkCompletionProvider(std::function<std::vector<CompletionItem>()> note_provider)
    : note_provider_(std::move(note_provider)) {
}

bool WikiLinkCompletionProvider::canProvideCompletions(const CompletionContext& context) const {
    return context.trigger == "[[";
}

Result<std::vector<CompletionItem>> WikiLinkCompletionProvider::getCompletions(const CompletionContext& context) const {
    if (!canProvideCompletions(context)) {
        return std::vector<CompletionItem>{};
    }
    
    try {
        auto notes = note_provider_();
        return filterAndRankNotes(notes, context.query);
    } catch (const std::exception& e) {
        return makeErrorResult<std::vector<CompletionItem>>(ErrorCode::kSystemError,
            "Failed to get note completions: " + std::string(e.what()));
    }
}

std::vector<CompletionItem> WikiLinkCompletionProvider::filterAndRankNotes(
    const std::vector<CompletionItem>& notes, const std::string& query) const {
    
    std::vector<CompletionItem> filtered;
    
    for (const auto& note : notes) {
        if (query.empty()) {
            // No query - return all notes
            filtered.push_back(note);
        } else {
            // Apply fuzzy matching
            double score = FuzzyMatcher::calculateScore(query, note.display_text);
            if (score > 0.0) {
                auto item = note;
                item.relevance_score = score;
                filtered.push_back(item);
            }
        }
    }
    
    // Sort by relevance score (descending)
    std::sort(filtered.begin(), filtered.end(), 
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.relevance_score > b.relevance_score;
        });
    
    return filtered;
}

// TagCompletionProvider Implementation

TagCompletionProvider::TagCompletionProvider(std::function<std::vector<CompletionItem>()> tag_provider)
    : tag_provider_(std::move(tag_provider)) {
}

bool TagCompletionProvider::canProvideCompletions(const CompletionContext& context) const {
    return context.trigger == "#";
}

Result<std::vector<CompletionItem>> TagCompletionProvider::getCompletions(const CompletionContext& context) const {
    if (!canProvideCompletions(context)) {
        return std::vector<CompletionItem>{};
    }
    
    try {
        auto tags = tag_provider_();
        return filterAndRankTags(tags, context.query);
    } catch (const std::exception& e) {
        return makeErrorResult<std::vector<CompletionItem>>(ErrorCode::kSystemError,
            "Failed to get tag completions: " + std::string(e.what()));
    }
}

std::vector<CompletionItem> TagCompletionProvider::filterAndRankTags(
    const std::vector<CompletionItem>& tags, const std::string& query) const {
    
    std::vector<CompletionItem> filtered;
    
    for (const auto& tag : tags) {
        if (query.empty()) {
            // No query - return all tags
            filtered.push_back(tag);
        } else {
            // Apply fuzzy matching
            double score = FuzzyMatcher::calculateScore(query, tag.text);
            if (score > 0.0) {
                auto item = tag;
                item.relevance_score = score;
                filtered.push_back(item);
            }
        }
    }
    
    // Sort by relevance score and usage count
    std::sort(filtered.begin(), filtered.end(), 
        [](const CompletionItem& a, const CompletionItem& b) {
            if (std::abs(a.relevance_score - b.relevance_score) < 0.01) {
                return a.usage_count > b.usage_count;
            }
            return a.relevance_score > b.relevance_score;
        });
    
    return filtered;
}

// MarkdownSnippetProvider Implementation

MarkdownSnippetProvider::MarkdownSnippetProvider() {
    initializeSnippets();
}

bool MarkdownSnippetProvider::canProvideCompletions(const CompletionContext& context) const {
    return context.trigger == "/" || context.trigger == "!!";
}

Result<std::vector<CompletionItem>> MarkdownSnippetProvider::getCompletions(const CompletionContext& context) const {
    if (!canProvideCompletions(context)) {
        return std::vector<CompletionItem>{};
    }
    
    std::vector<CompletionItem> filtered;
    
    for (const auto& snippet : snippets_) {
        if (context.query.empty()) {
            filtered.push_back(snippet);
        } else {
            double score = FuzzyMatcher::calculateScore(context.query, snippet.text);
            if (score > 0.0) {
                auto item = snippet;
                item.relevance_score = score;
                filtered.push_back(item);
            }
        }
    }
    
    // Sort by relevance
    std::sort(filtered.begin(), filtered.end(), 
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.relevance_score > b.relevance_score;
        });
    
    return filtered;
}

void MarkdownSnippetProvider::initializeSnippets() {
    snippets_ = {
        {"bold", "**bold text**", "Make text bold", "formatting", 1.0, "", 0},
        {"italic", "*italic text*", "Make text italic", "formatting", 1.0, "", 0},
        {"code", "`inline code`", "Inline code", "formatting", 1.0, "", 0},
        {"codeblock", "```\ncode block\n```", "Code block", "formatting", 1.0, "", 0},
        {"link", "[link text](url)", "Create link", "formatting", 1.0, "", 0},
        {"image", "![alt text](image-url)", "Insert image", "formatting", 1.0, "", 0},
        {"h1", "# Heading 1", "Level 1 heading", "heading", 1.0, "", 0},
        {"h2", "## Heading 2", "Level 2 heading", "heading", 1.0, "", 0},
        {"h3", "### Heading 3", "Level 3 heading", "heading", 1.0, "", 0},
        {"list", "- list item", "Bullet list item", "list", 1.0, "", 0},
        {"numlist", "1. numbered item", "Numbered list item", "list", 1.0, "", 0},
        {"checkbox", "- [ ] task item", "Checkbox/task item", "list", 1.0, "", 0},
        {"quote", "> blockquote", "Block quote", "formatting", 1.0, "", 0},
        {"hr", "---", "Horizontal rule", "formatting", 1.0, "", 0},
        {"table", "| Column 1 | Column 2 |\n|----------|----------|\n| Cell 1   | Cell 2   |", "Table", "formatting", 1.0, "", 0},
        {"todo", "- [ ] TODO: ", "TODO item", "task", 1.0, "", 0},
        {"note", "> **Note**: ", "Note callout", "callout", 1.0, "", 0},
        {"warning", "> **Warning**: ", "Warning callout", "callout", 1.0, "", 0},
        {"date", "", "Current date", "utility", 1.0, "", 0}, // Will be filled with actual date
        {"time", "", "Current time", "utility", 1.0, "", 0}  // Will be filled with actual time
    };
    
    // Fill in dynamic content
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto* tm = std::localtime(&time_t);
    
    char date_buffer[32];
    char time_buffer[32];
    std::strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", tm);
    std::strftime(time_buffer, sizeof(time_buffer), "%H:%M", tm);
    
    for (auto& snippet : snippets_) {
        if (snippet.text == "date") {
            snippet.text = std::string(date_buffer);
            snippet.display_text = "date (" + std::string(date_buffer) + ")";
        } else if (snippet.text == "time") {
            snippet.text = std::string(time_buffer);
            snippet.display_text = "time (" + std::string(time_buffer) + ")";
        } else {
            snippet.display_text = snippet.text;
        }
    }
}

// AutoCompletionEngine Implementation

AutoCompletionEngine::AutoCompletionEngine() = default;
AutoCompletionEngine::~AutoCompletionEngine() = default;

void AutoCompletionEngine::registerProvider(std::unique_ptr<CompletionProvider> provider) {
    providers_.push_back(std::move(provider));
    
    // Sort providers by priority (highest first)
    std::sort(providers_.begin(), providers_.end(),
        [](const auto& a, const auto& b) {
            return a->getPriority() > b->getPriority();
        });
}

Result<std::vector<CompletionItem>> AutoCompletionEngine::getCompletions(const CompletionContext& context) const {
    if (!config_.enable_auto_completion) {
        return std::vector<CompletionItem>{};
    }
    
    if (context.query.size() < config_.min_query_length && !context.query.empty()) {
        return std::vector<CompletionItem>{};
    }
    
    std::vector<std::vector<CompletionItem>> provider_results;
    
    for (const auto& provider : providers_) {
        if (provider->canProvideCompletions(context)) {
            auto result = provider->getCompletions(context);
            if (result.has_value()) {
                provider_results.push_back(result.value());
            }
        }
    }
    
    auto merged = mergeAndRankCompletions(provider_results);
    
    // Apply frequency ranking if enabled
    if (config_.frequency_ranking) {
        applyFrequencyRanking(merged);
    }
    
    // Limit results
    if (merged.size() > config_.max_suggestions) {
        merged.resize(config_.max_suggestions);
    }
    
    return merged;
}

std::optional<CompletionContext> AutoCompletionEngine::shouldTriggerCompletion(
    const std::string& text, size_t cursor_position) const {
    
    if (!config_.enable_auto_completion || text.empty() || cursor_position > text.size()) {
        return std::nullopt;
    }
    
    return detectTrigger(text, cursor_position);
}

void AutoCompletionEngine::recordCompletionUsage(const CompletionItem& item) {
    completion_usage_[item.text]++;
    last_usage_[item.text] = std::chrono::system_clock::now();
}

std::optional<CompletionContext> AutoCompletionEngine::detectTrigger(
    const std::string& text, size_t cursor_position) const {
    
    // Check for wiki-link triggers
    for (const auto& trigger : config_.wiki_link_triggers) {
        if (cursor_position >= trigger.length()) {
            size_t start_pos = cursor_position - trigger.length();
            if (text.substr(start_pos, trigger.length()) == trigger) {
                CompletionContext context;
                context.trigger = trigger;
                context.cursor_position = cursor_position;
                
                // Extract query after trigger
                size_t query_start = cursor_position;
                size_t query_end = cursor_position;
                while (query_end < text.size() && text[query_end] != ']' && text[query_end] != '\n') {
                    query_end++;
                }
                if (query_end > query_start) {
                    context.query = text.substr(query_start, query_end - query_start);
                }
                
                return context;
            }
        }
    }
    
    // Check for tag triggers
    for (const auto& trigger : config_.tag_triggers) {
        if (cursor_position >= trigger.length()) {
            size_t start_pos = cursor_position - trigger.length();
            if (text.substr(start_pos, trigger.length()) == trigger) {
                // Make sure it's at word boundary
                if (start_pos == 0 || std::isspace(text[start_pos - 1])) {
                    CompletionContext context;
                    context.trigger = trigger;
                    context.cursor_position = cursor_position;
                    
                    // Extract query after trigger
                    size_t query_start = cursor_position;
                    size_t query_end = cursor_position;
                    while (query_end < text.size() && 
                           (std::isalnum(text[query_end]) || text[query_end] == '_' || text[query_end] == '-')) {
                        query_end++;
                    }
                    if (query_end > query_start) {
                        context.query = text.substr(query_start, query_end - query_start);
                    }
                    
                    return context;
                }
            }
        }
    }
    
    // Check for snippet triggers
    for (const auto& trigger : config_.snippet_triggers) {
        if (cursor_position >= trigger.length()) {
            size_t start_pos = cursor_position - trigger.length();
            if (text.substr(start_pos, trigger.length()) == trigger) {
                // Make sure it's at start of line or after whitespace
                if (start_pos == 0 || std::isspace(text[start_pos - 1])) {
                    CompletionContext context;
                    context.trigger = trigger;
                    context.cursor_position = cursor_position;
                    
                    // Extract query after trigger
                    size_t query_start = cursor_position;
                    size_t query_end = cursor_position;
                    while (query_end < text.size() && 
                           (std::isalnum(text[query_end]) || text[query_end] == '_')) {
                        query_end++;
                    }
                    if (query_end > query_start) {
                        context.query = text.substr(query_start, query_end - query_start);
                    }
                    
                    return context;
                }
            }
        }
    }
    
    return std::nullopt;
}

std::vector<CompletionItem> AutoCompletionEngine::mergeAndRankCompletions(
    const std::vector<std::vector<CompletionItem>>& provider_results) const {
    
    std::vector<CompletionItem> merged;
    
    for (const auto& provider_result : provider_results) {
        for (const auto& item : provider_result) {
            merged.push_back(item);
        }
    }
    
    // Remove duplicates based on text
    std::sort(merged.begin(), merged.end(),
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.text < b.text;
        });
    
    merged.erase(std::unique(merged.begin(), merged.end(),
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.text == b.text;
        }), merged.end());
    
    // Sort by relevance score
    std::sort(merged.begin(), merged.end(),
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.relevance_score > b.relevance_score;
        });
    
    return merged;
}

void AutoCompletionEngine::applyFrequencyRanking(std::vector<CompletionItem>& items) const {
    for (auto& item : items) {
        auto usage_it = completion_usage_.find(item.text);
        if (usage_it != completion_usage_.end()) {
            // Boost score based on usage frequency
            double frequency_boost = std::min(0.5, usage_it->second * 0.1);
            item.relevance_score += frequency_boost;
        }
        
        auto last_used_it = last_usage_.find(item.text);
        if (last_used_it != last_usage_.end()) {
            // Boost recently used items
            auto now = std::chrono::system_clock::now();
            auto time_since_use = std::chrono::duration_cast<std::chrono::hours>(now - last_used_it->second);
            if (time_since_use.count() < 24) {
                double recency_boost = 0.2 * (24 - time_since_use.count()) / 24.0;
                item.relevance_score += recency_boost;
            }
        }
    }
    
    // Re-sort with frequency adjustments
    std::sort(items.begin(), items.end(),
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.relevance_score > b.relevance_score;
        });
}

// FuzzyMatcher Implementation

double FuzzyMatcher::calculateScore(const std::string& query, const std::string& target) {
    if (query.empty()) return 1.0;
    if (target.empty()) return 0.0;
    
    std::string lower_query = query;
    std::string lower_target = target;
    
    // Convert to lowercase for case-insensitive matching
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    std::transform(lower_target.begin(), lower_target.end(), lower_target.begin(), ::tolower);
    
    // Perfect match
    if (lower_query == lower_target) {
        return 1.0;
    }
    
    // Prefix match gets high score
    if (lower_target.find(lower_query) == 0) {
        return 0.9;
    }
    
    // Substring match
    size_t pos = lower_target.find(lower_query);
    if (pos != std::string::npos) {
        // Score based on position (earlier = better)
        return 0.7 - (pos * 0.1) / lower_target.length();
    }
    
    // Fuzzy character matching
    size_t query_idx = 0;
    size_t matched_chars = 0;
    
    for (size_t target_idx = 0; target_idx < lower_target.length() && query_idx < lower_query.length(); ++target_idx) {
        if (lower_target[target_idx] == lower_query[query_idx]) {
            matched_chars++;
            query_idx++;
        }
    }
    
    if (matched_chars == lower_query.length()) {
        // All query characters found, score based on ratio
        return 0.5 * matched_chars / lower_target.length();
    }
    
    return 0.0;
}

bool FuzzyMatcher::matches(const std::string& query, const std::string& target, double threshold) {
    return calculateScore(query, target) >= threshold;
}

std::vector<size_t> FuzzyMatcher::getMatchPositions(const std::string& query, const std::string& target) {
    std::vector<size_t> positions;
    
    if (query.empty() || target.empty()) {
        return positions;
    }
    
    std::string lower_query = query;
    std::string lower_target = target;
    
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    std::transform(lower_target.begin(), lower_target.end(), lower_target.begin(), ::tolower);
    
    size_t query_idx = 0;
    
    for (size_t target_idx = 0; target_idx < lower_target.length() && query_idx < lower_query.length(); ++target_idx) {
        if (lower_target[target_idx] == lower_query[query_idx]) {
            positions.push_back(target_idx);
            query_idx++;
        }
    }
    
    return positions;
}

} // namespace nx::tui