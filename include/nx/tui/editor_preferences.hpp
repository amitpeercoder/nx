#pragma once

#include <string>
#include <chrono>
#include <filesystem>
#include <functional>
#include <toml++/toml.h>
#include "nx/common.hpp"

namespace nx::tui {

/**
 * @brief Editor behavior configuration
 */
struct EditorBehaviorConfig {
    bool auto_indent = true;
    bool smart_quotes = false;
    bool auto_save = true;
    std::chrono::milliseconds auto_save_delay{5000};  // 5 seconds
    bool show_line_numbers = false;
    bool highlight_current_line = true;
    size_t max_undo_history = 50;
    bool word_wrap = false;
    size_t tab_width = 4;
    bool use_spaces_for_tabs = true;
};

/**
 * @brief Search configuration
 */
struct EditorSearchConfig {
    bool case_sensitive = false;
    bool whole_words = false;
    bool highlight_all_matches = true;
    size_t max_search_results = 1000;
    bool incremental_search = true;
    std::chrono::milliseconds search_timeout{1000};
};

/**
 * @brief Clipboard configuration
 */
struct EditorClipboardConfig {
    bool prefer_system_clipboard = true;
    std::chrono::milliseconds clipboard_timeout{1000};
    size_t internal_clipboard_size_mb = 10;
    bool auto_clear_sensitive = true;
};

/**
 * @brief Terminal integration configuration
 */
struct EditorTerminalConfig {
    bool detect_capabilities = true;
    bool force_basic_mode = false;
    std::chrono::milliseconds key_timeout{100};
    std::string color_support = "auto";  // auto, always, never
    bool mouse_support = true;
    bool bracketed_paste = true;
};

/**
 * @brief Performance configuration
 */
struct EditorPerformanceConfig {
    size_t large_file_threshold = 1000;        // lines
    size_t very_large_file_threshold = 10000;  // lines
    bool virtual_scrolling = true;
    bool lazy_rendering = true;
    size_t render_chunk_size = 100;            // lines
    size_t max_memory_usage_mb = 512;
};

/**
 * @brief Markdown-specific configuration
 */
struct EditorMarkdownConfig {
    bool auto_continue_lists = true;
    bool smart_quotes = false;
    bool auto_link_detection = false;
    bool header_folding = false;
    bool syntax_highlighting = true;
    bool wiki_links = true;
    bool table_formatting = true;
};

/**
 * @brief Complete editor configuration
 */
struct EditorConfig {
    EditorBehaviorConfig behavior;
    EditorSearchConfig search;
    EditorClipboardConfig clipboard;
    EditorTerminalConfig terminal;
    EditorPerformanceConfig performance;
    EditorMarkdownConfig markdown;
    
    // Configuration metadata
    std::string config_version = "1.0";
    std::chrono::system_clock::time_point last_modified = std::chrono::system_clock::now();
};

/**
 * @brief Editor preferences manager with TOML persistence
 * 
 * Manages editor configuration with:
 * - TOML file persistence using toml++
 * - Runtime configuration updates
 * - Configuration validation
 * - Default value fallbacks
 * - XDG-compliant configuration directory
 */
class EditorPreferences {
public:
    explicit EditorPreferences(const std::filesystem::path& config_dir = {});
    
    /**
     * @brief Load configuration from TOML file
     * @return Result with configuration or error
     */
    Result<EditorConfig> loadConfig();
    
    /**
     * @brief Save configuration to TOML file
     * @param config Configuration to save
     * @return Result indicating success or error
     */
    Result<void> saveConfig(const EditorConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const EditorConfig& getConfig() const { return config_; }
    
    /**
     * @brief Update configuration and save to file
     * @param config New configuration
     * @return Result indicating success or error
     */
    Result<void> updateConfig(const EditorConfig& config);
    
    /**
     * @brief Reset to default configuration
     * @return Result indicating success or error
     */
    Result<void> resetToDefaults();
    
    /**
     * @brief Validate configuration values
     * @param config Configuration to validate
     * @return Result indicating validity
     */
    static Result<void> validateConfig(const EditorConfig& config);
    
    /**
     * @brief Get default configuration
     * @return Default configuration
     */
    static EditorConfig getDefaultConfig();
    
    /**
     * @brief Watch configuration file for changes
     * @param callback Function to call when config changes
     * @return Result indicating success or error
     */
    Result<void> watchConfigFile(std::function<void(const EditorConfig&)> callback);
    
private:
    std::filesystem::path config_file_;
    EditorConfig config_;
    std::chrono::system_clock::time_point last_file_check_;
    
    /**
     * @brief Get XDG-compliant configuration directory
     * @return Configuration directory path
     */
    std::filesystem::path getConfigDirectory() const;
    
    /**
     * @brief Convert TOML to EditorConfig
     * @param toml_data TOML data
     * @return Result with configuration or error
     */
    Result<EditorConfig> parseTomlConfig(const toml::table& toml_data) const;
    
    /**
     * @brief Convert EditorConfig to TOML
     * @param config Configuration to convert
     * @return TOML table
     */
    toml::table configToToml(const EditorConfig& config) const;
    
    /**
     * @brief Ensure configuration directory exists
     * @return Result indicating success or error
     */
    Result<void> ensureConfigDirectory() const;
    
    /**
     * @brief Check if configuration file has been modified
     * @return true if file has been modified since last check
     */
    bool hasConfigFileChanged() const;
};

} // namespace nx::tui