#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <optional>
#include <map>
#include "nx/common.hpp"

namespace nx::tui {

/**
 * @brief Auto-completion suggestion
 */
struct CompletionItem {
    std::string text;               // The completion text
    std::string display_text;       // Text to display in completion popup
    std::string description;        // Optional description
    std::string category;           // Category (e.g., "note", "tag", "file")
    double relevance_score = 1.0;   // Relevance score for sorting
    
    // Metadata for different completion types
    std::string note_id;            // For note completions
    size_t usage_count = 0;         // For frequency-based ranking
    std::chrono::system_clock::time_point last_used = std::chrono::system_clock::now();
};

/**
 * @brief Auto-completion context information
 */
struct CompletionContext {
    std::string trigger;            // The trigger sequence (e.g., "[[", "#")
    std::string query;              // Current partial input after trigger
    size_t cursor_position;         // Position in the document
    size_t line_number;             // Current line number
    std::string current_line;       // Current line content
    std::string surrounding_text;   // Text around cursor for context
};

/**
 * @brief Auto-completion provider interface
 */
class CompletionProvider {
public:
    virtual ~CompletionProvider() = default;
    
    /**
     * @brief Check if this provider can handle the given context
     * @param context Completion context
     * @return true if provider can provide completions
     */
    virtual bool canProvideCompletions(const CompletionContext& context) const = 0;
    
    /**
     * @brief Get completions for the given context
     * @param context Completion context
     * @return Result with list of completion items
     */
    virtual Result<std::vector<CompletionItem>> getCompletions(const CompletionContext& context) const = 0;
    
    /**
     * @brief Get provider priority (higher = more important)
     * @return Priority value
     */
    virtual int getPriority() const = 0;
    
    /**
     * @brief Get provider name for debugging
     * @return Provider name
     */
    virtual std::string getName() const = 0;
};

/**
 * @brief Wiki-link completion provider
 */
class WikiLinkCompletionProvider : public CompletionProvider {
public:
    explicit WikiLinkCompletionProvider(std::function<std::vector<CompletionItem>()> note_provider);
    
    bool canProvideCompletions(const CompletionContext& context) const override;
    Result<std::vector<CompletionItem>> getCompletions(const CompletionContext& context) const override;
    int getPriority() const override { return 100; }
    std::string getName() const override { return "WikiLink"; }
    
private:
    std::function<std::vector<CompletionItem>()> note_provider_;
    
    /**
     * @brief Filter and rank notes based on query
     * @param notes Available notes
     * @param query Search query
     * @return Filtered and ranked notes
     */
    std::vector<CompletionItem> filterAndRankNotes(const std::vector<CompletionItem>& notes, const std::string& query) const;
};

/**
 * @brief Tag completion provider
 */
class TagCompletionProvider : public CompletionProvider {
public:
    explicit TagCompletionProvider(std::function<std::vector<CompletionItem>()> tag_provider);
    
    bool canProvideCompletions(const CompletionContext& context) const override;
    Result<std::vector<CompletionItem>> getCompletions(const CompletionContext& context) const override;
    int getPriority() const override { return 90; }
    std::string getName() const override { return "Tag"; }
    
private:
    std::function<std::vector<CompletionItem>()> tag_provider_;
    
    /**
     * @brief Filter and rank tags based on query
     * @param tags Available tags
     * @param query Search query
     * @return Filtered and ranked tags
     */
    std::vector<CompletionItem> filterAndRankTags(const std::vector<CompletionItem>& tags, const std::string& query) const;
};

/**
 * @brief Markdown snippet completion provider
 */
class MarkdownSnippetProvider : public CompletionProvider {
public:
    MarkdownSnippetProvider();
    
    bool canProvideCompletions(const CompletionContext& context) const override;
    Result<std::vector<CompletionItem>> getCompletions(const CompletionContext& context) const override;
    int getPriority() const override { return 80; }
    std::string getName() const override { return "MarkdownSnippet"; }
    
private:
    std::vector<CompletionItem> snippets_;
    
    /**
     * @brief Initialize built-in markdown snippets
     */
    void initializeSnippets();
};

/**
 * @brief Auto-completion engine
 */
class AutoCompletionEngine {
public:
    AutoCompletionEngine();
    ~AutoCompletionEngine();
    
    /**
     * @brief Register a completion provider
     * @param provider Completion provider to register
     */
    void registerProvider(std::unique_ptr<CompletionProvider> provider);
    
    /**
     * @brief Get completions for the given context
     * @param context Completion context
     * @return Result with list of completion items
     */
    Result<std::vector<CompletionItem>> getCompletions(const CompletionContext& context) const;
    
    /**
     * @brief Check if auto-completion should be triggered
     * @param text Current text being typed
     * @param cursor_position Current cursor position
     * @return Completion context if triggered, empty optional otherwise
     */
    std::optional<CompletionContext> shouldTriggerCompletion(const std::string& text, size_t cursor_position) const;
    
    /**
     * @brief Record completion usage for ranking
     * @param item Completion item that was used
     */
    void recordCompletionUsage(const CompletionItem& item);
    
    /**
     * @brief Set completion configuration
     */
    struct Config {
        bool enable_auto_completion = true;
        size_t min_query_length = 1;           // Minimum characters to trigger
        size_t max_suggestions = 10;           // Maximum suggestions to show
        std::chrono::milliseconds trigger_delay{100}; // Delay before showing completions
        bool fuzzy_matching = true;            // Enable fuzzy string matching
        bool frequency_ranking = true;         // Rank by usage frequency
        
        // Trigger patterns
        std::vector<std::string> wiki_link_triggers = {"[["};
        std::vector<std::string> tag_triggers = {"#"};
        std::vector<std::string> snippet_triggers = {"/", "!!"};
    };
    
    void setConfig(const Config& config) { config_ = config; }
    const Config& getConfig() const { return config_; }
    
private:
    std::vector<std::unique_ptr<CompletionProvider>> providers_;
    Config config_;
    
    // Usage tracking for frequency-based ranking
    mutable std::map<std::string, size_t> completion_usage_;
    mutable std::map<std::string, std::chrono::system_clock::time_point> last_usage_;
    
    /**
     * @brief Detect completion trigger in text
     * @param text Text to analyze
     * @param cursor_position Current cursor position
     * @return Completion context if trigger detected
     */
    std::optional<CompletionContext> detectTrigger(const std::string& text, size_t cursor_position) const;
    
    /**
     * @brief Merge and rank completions from multiple providers
     * @param provider_results Results from different providers
     * @return Merged and ranked completion list
     */
    std::vector<CompletionItem> mergeAndRankCompletions(const std::vector<std::vector<CompletionItem>>& provider_results) const;
    
    /**
     * @brief Apply frequency-based ranking boost
     * @param items Completion items to boost
     */
    void applyFrequencyRanking(std::vector<CompletionItem>& items) const;
};

/**
 * @brief Fuzzy string matching utility
 */
class FuzzyMatcher {
public:
    /**
     * @brief Calculate fuzzy match score between query and target
     * @param query Search query
     * @param target Target string
     * @return Match score (0.0 = no match, 1.0 = perfect match)
     */
    static double calculateScore(const std::string& query, const std::string& target);
    
    /**
     * @brief Check if query fuzzy matches target
     * @param query Search query
     * @param target Target string
     * @param threshold Minimum score threshold
     * @return true if match score >= threshold
     */
    static bool matches(const std::string& query, const std::string& target, double threshold = 0.3);
    
    /**
     * @brief Get match positions for highlighting
     * @param query Search query
     * @param target Target string
     * @return Positions of matched characters in target
     */
    static std::vector<size_t> getMatchPositions(const std::string& query, const std::string& target);
};

} // namespace nx::tui