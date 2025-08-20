#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>

#include "nx/common.hpp"
#include "nx/config/config.hpp"
#include "nx/tui/editor_buffer.hpp"
#include "nx/util/http_client.hpp"

namespace nx::tui {

/**
 * @brief AI-powered explanation service for technical terms and abbreviations
 * 
 * Provides contextual explanations for terms in the editor by leveraging AI
 * to generate brief and expanded explanations based on the surrounding context.
 */
class AiExplanationService {
public:
    /**
     * @brief Result structure for term explanations
     */
    struct ExplanationResult {
        std::string term;           // Original term being explained
        std::string brief;          // Short explanation (5-10 words)
        std::string expanded;       // Detailed explanation (2-3 sentences)
        bool is_cached = false;     // Whether result came from cache
    };

    /**
     * @brief Configuration for explanation service
     */
    struct Config {
        size_t brief_max_words;             // Maximum words in brief explanation
        size_t expanded_max_words;          // Maximum words in expanded explanation
        std::chrono::milliseconds timeout;
        bool cache_explanations;            // Whether to cache explanations
        size_t max_cache_size;              // Maximum cached explanations
        size_t context_radius;              // Characters around term for context
        
        Config() 
            : brief_max_words(10)
            , expanded_max_words(50)
            , timeout(std::chrono::milliseconds(3000))
            , cache_explanations(true)
            , max_cache_size(1000)
            , context_radius(100) {}
    };

    /**
     * @brief Constructor
     * @param config Configuration for the service
     */
    explicit AiExplanationService(Config config = Config{});

    /**
     * @brief Get brief explanation for a term
     * @param term The term to explain
     * @param context Surrounding text context
     * @param ai_config AI configuration
     * @return Result with brief explanation
     */
    Result<std::string> getBriefExplanation(const std::string& term,
                                            const std::string& context,
                                            const nx::config::Config::AiConfig& ai_config);

    /**
     * @brief Get expanded explanation for a term
     * @param term The term to explain
     * @param context Surrounding text context
     * @param ai_config AI configuration
     * @return Result with expanded explanation
     */
    Result<std::string> getExpandedExplanation(const std::string& term,
                                               const std::string& context,
                                               const nx::config::Config::AiConfig& ai_config);

    /**
     * @brief Get complete explanation result (both brief and expanded)
     * @param term The term to explain
     * @param context Surrounding text context
     * @param ai_config AI configuration
     * @return Result with complete explanation
     */
    Result<ExplanationResult> explainTerm(const std::string& term,
                                          const std::string& context,
                                          const nx::config::Config::AiConfig& ai_config);

    /**
     * @brief Extract word at cursor position from editor buffer
     * @param buffer Editor buffer to extract from
     * @param line Line number (0-based)
     * @param col Column number (0-based)
     * @return Result with extracted word
     */
    static Result<std::string> extractWordAt(const EditorBuffer& buffer,
                                             size_t line, size_t col);

    /**
     * @brief Extract word before cursor position from editor buffer
     * @param buffer Editor buffer to extract from
     * @param line Line number (0-based)
     * @param col Column number (0-based)
     * @return Result with extracted word
     */
    static Result<std::string> extractWordBefore(const EditorBuffer& buffer,
                                                 size_t line, size_t col);

    /**
     * @brief Extract context around cursor position
     * @param buffer Editor buffer to extract from
     * @param line Line number (0-based)
     * @param col Column number (0-based)
     * @param radius Number of characters to include around position
     * @return Result with context string
     */
    static Result<std::string> extractContext(const EditorBuffer& buffer,
                                              size_t line, size_t col,
                                              size_t radius = 100);

    /**
     * @brief Clear explanation cache
     */
    void clearCache();

    /**
     * @brief Get cache statistics
     * @return Pair of (cache_size, cache_hits)
     */
    std::pair<size_t, size_t> getCacheStats() const;

private:
    /**
     * @brief Cache entry for explanations
     */
    struct CacheEntry {
        std::string brief;
        std::string expanded;
        std::chrono::steady_clock::time_point timestamp;
        
        CacheEntry(const std::string& b, const std::string& e)
            : brief(b), expanded(e), timestamp(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Make AI request for explanation
     * @param term Term to explain
     * @param context Context around term
     * @param ai_config AI configuration
     * @param is_expanded Whether to request expanded explanation
     * @return Result with explanation
     */
    Result<std::string> makeAiRequest(const std::string& term,
                                      const std::string& context,
                                      const nx::config::Config::AiConfig& ai_config,
                                      bool is_expanded);

    /**
     * @brief Check if term is in cache
     * @param term Term to check
     * @return Pointer to cache entry or nullptr
     */
    const CacheEntry* getCachedExplanation(const std::string& term) const;

    /**
     * @brief Add explanation to cache
     * @param term Term being explained
     * @param brief Brief explanation
     * @param expanded Expanded explanation
     */
    void cacheExplanation(const std::string& term,
                          const std::string& brief,
                          const std::string& expanded);

    /**
     * @brief Clean old entries from cache
     */
    void cleanCache();

    /**
     * @brief Validate and clean explanation text
     * @param explanation Raw explanation text
     * @param max_words Maximum number of words
     * @return Cleaned explanation
     */
    std::string cleanExplanation(const std::string& explanation, size_t max_words) const;

    /**
     * @brief Check if a term is worth explaining (not too common)
     * @param term Term to check
     * @return True if term should be explained
     */
    bool shouldExplainTerm(const std::string& term) const;

    Config config_;
    mutable std::unordered_map<std::string, CacheEntry> cache_;
    mutable size_t cache_hits_ = 0;
};

} // namespace nx::tui