#include "nx/tui/editor_preferences.hpp"
#include "nx/util/xdg.hpp"
#include <toml++/toml.h>
#include <fstream>
#include <filesystem>

namespace nx::tui {

EditorPreferences::EditorPreferences(const std::filesystem::path& config_dir) 
    : last_file_check_(std::chrono::system_clock::now()) {
    
    if (config_dir.empty()) {
        config_file_ = getConfigDirectory() / "editor.toml";
    } else {
        config_file_ = config_dir / "editor.toml";
    }
    
    // Load existing configuration or use defaults
    auto load_result = loadConfig();
    if (!load_result) {
        // Use default configuration if loading fails
        config_ = getDefaultConfig();
    } else {
        config_ = load_result.value();
    }
}

Result<EditorConfig> EditorPreferences::loadConfig() {
    if (!std::filesystem::exists(config_file_)) {
        // Return default config if file doesn't exist
        return getDefaultConfig();
    }
    
    try {
        auto toml_data = toml::parse_file(config_file_.string());
        return parseTomlConfig(toml_data);
    } catch (const toml::parse_error& e) {
        return makeErrorResult<EditorConfig>(ErrorCode::kConfigError,
            "Failed to parse TOML configuration: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return makeErrorResult<EditorConfig>(ErrorCode::kConfigError,
            "Failed to load configuration: " + std::string(e.what()));
    }
}

Result<void> EditorPreferences::saveConfig(const EditorConfig& config) {
    // Validate configuration before saving
    auto validation = validateConfig(config);
    if (!validation) {
        return validation;
    }
    
    // Ensure config directory exists
    auto dir_result = ensureConfigDirectory();
    if (!dir_result) {
        return dir_result;
    }
    
    try {
        auto toml_data = configToToml(config);
        
        // Write to temporary file first for atomic update
        auto temp_file = config_file_.string() + ".tmp";
        std::ofstream file(temp_file);
        if (!file) {
            return makeErrorResult<void>(ErrorCode::kFileError,
                "Failed to open configuration file for writing");
        }
        
        file << toml_data;
        file.close();
        
        if (file.fail()) {
            std::filesystem::remove(temp_file);
            return makeErrorResult<void>(ErrorCode::kFileError,
                "Failed to write configuration data");
        }
        
        // Atomic rename
        std::filesystem::rename(temp_file, config_file_);
        
        // Update modification time
        config_.last_modified = std::chrono::system_clock::now();
        last_file_check_ = config_.last_modified;
        
        return {};
        
    } catch (const std::exception& e) {
        return makeErrorResult<void>(ErrorCode::kConfigError,
            "Failed to save configuration: " + std::string(e.what()));
    }
}

Result<void> EditorPreferences::updateConfig(const EditorConfig& config) {
    config_ = config;
    return saveConfig(config_);
}

Result<void> EditorPreferences::resetToDefaults() {
    config_ = getDefaultConfig();
    return saveConfig(config_);
}

Result<void> EditorPreferences::validateConfig(const EditorConfig& config) {
    // Validate behavior config
    if (config.behavior.max_undo_history == 0 || config.behavior.max_undo_history > 1000) {
        return makeErrorResult<void>(ErrorCode::kValidationError,
            "max_undo_history must be between 1 and 1000");
    }
    
    if (config.behavior.tab_width == 0 || config.behavior.tab_width > 16) {
        return makeErrorResult<void>(ErrorCode::kValidationError,
            "tab_width must be between 1 and 16");
    }
    
    if (config.behavior.auto_save_delay < std::chrono::milliseconds(1000) ||
        config.behavior.auto_save_delay > std::chrono::minutes(30)) {
        return makeErrorResult<void>(ErrorCode::kValidationError,
            "auto_save_delay must be between 1 second and 30 minutes");
    }
    
    // Validate search config
    if (config.search.max_search_results == 0 || config.search.max_search_results > 100000) {
        return makeErrorResult<void>(ErrorCode::kValidationError,
            "max_search_results must be between 1 and 100000");
    }
    
    // Validate clipboard config
    if (config.clipboard.internal_clipboard_size_mb == 0 || 
        config.clipboard.internal_clipboard_size_mb > 1000) {
        return makeErrorResult<void>(ErrorCode::kValidationError,
            "internal_clipboard_size_mb must be between 1 and 1000");
    }
    
    // Validate terminal config
    if (config.terminal.color_support != "auto" && 
        config.terminal.color_support != "always" && 
        config.terminal.color_support != "never") {
        return makeErrorResult<void>(ErrorCode::kValidationError,
            "color_support must be 'auto', 'always', or 'never'");
    }
    
    // Validate performance config
    if (config.performance.large_file_threshold == 0 || 
        config.performance.large_file_threshold > 1000000) {
        return makeErrorResult<void>(ErrorCode::kValidationError,
            "large_file_threshold must be between 1 and 1000000");
    }
    
    if (config.performance.max_memory_usage_mb < 64 || 
        config.performance.max_memory_usage_mb > 8192) {
        return makeErrorResult<void>(ErrorCode::kValidationError,
            "max_memory_usage_mb must be between 64 and 8192");
    }
    
    return {};
}

EditorConfig EditorPreferences::getDefaultConfig() {
    return EditorConfig{}; // Uses default values from struct initialization
}

std::filesystem::path EditorPreferences::getConfigDirectory() const {
    return nx::util::Xdg::configHome() / "nx";
}

Result<EditorConfig> EditorPreferences::parseTomlConfig(const toml::table& toml_data) const {
    EditorConfig config = getDefaultConfig();
    
    try {
        // Parse behavior section - simplified version
        if (auto behavior = toml_data["behavior"].as_table()) {
            auto& b = *behavior;
            if (auto val = b["auto_indent"].value<bool>()) config.behavior.auto_indent = *val;
            if (auto val = b["smart_quotes"].value<bool>()) config.behavior.smart_quotes = *val;
            if (auto val = b["auto_save"].value<bool>()) config.behavior.auto_save = *val;
            if (auto val = b["show_line_numbers"].value<bool>()) config.behavior.show_line_numbers = *val;
            if (auto val = b["highlight_current_line"].value<bool>()) config.behavior.highlight_current_line = *val;
            if (auto val = b["word_wrap"].value<bool>()) config.behavior.word_wrap = *val;
            if (auto val = b["use_spaces_for_tabs"].value<bool>()) config.behavior.use_spaces_for_tabs = *val;
            
            if (auto val = b["auto_save_delay"].value<int64_t>()) {
                config.behavior.auto_save_delay = std::chrono::milliseconds(*val);
            }
            if (auto val = b["max_undo_history"].value<int64_t>()) {
                config.behavior.max_undo_history = static_cast<size_t>(*val);
            }
            if (auto val = b["tab_width"].value<int64_t>()) {
                config.behavior.tab_width = static_cast<size_t>(*val);
            }
        }
        
        // Parse search section
        if (auto search = toml_data["search"].as_table()) {
            auto& s = *search;
            if (auto val = s["case_sensitive"].value<bool>()) config.search.case_sensitive = *val;
            if (auto val = s["whole_words"].value<bool>()) config.search.whole_words = *val;
            if (auto val = s["highlight_all_matches"].value<bool>()) config.search.highlight_all_matches = *val;
            if (auto val = s["incremental_search"].value<bool>()) config.search.incremental_search = *val;
            
            if (auto val = s["max_search_results"].value<int64_t>()) {
                config.search.max_search_results = static_cast<size_t>(*val);
            }
            if (auto val = s["search_timeout"].value<int64_t>()) {
                config.search.search_timeout = std::chrono::milliseconds(*val);
            }
        }
        
        // Parse clipboard section
        if (auto clipboard = toml_data["clipboard"].as_table()) {
            auto& c = *clipboard;
            if (auto val = c["prefer_system_clipboard"].value<bool>()) config.clipboard.prefer_system_clipboard = *val;
            if (auto val = c["internal_clipboard_size_mb"].value<int64_t>()) {
                config.clipboard.internal_clipboard_size_mb = static_cast<size_t>(*val);
            }
            if (auto val = c["clipboard_timeout"].value<int64_t>()) {
                config.clipboard.clipboard_timeout = std::chrono::milliseconds(*val);
            }
            if (auto val = c["auto_clear_sensitive"].value<bool>()) config.clipboard.auto_clear_sensitive = *val;
        }
        
        // Parse terminal section
        if (auto terminal = toml_data["terminal"].as_table()) {
            auto& t = *terminal;
            if (auto val = t["color_support"].value<std::string>()) config.terminal.color_support = *val;
            if (auto val = t["mouse_support"].value<bool>()) config.terminal.mouse_support = *val;
            if (auto val = t["detect_capabilities"].value<bool>()) config.terminal.detect_capabilities = *val;
            if (auto val = t["force_basic_mode"].value<bool>()) config.terminal.force_basic_mode = *val;
            if (auto val = t["bracketed_paste"].value<bool>()) config.terminal.bracketed_paste = *val;
            if (auto val = t["key_timeout"].value<int64_t>()) {
                config.terminal.key_timeout = std::chrono::milliseconds(*val);
            }
        }
        
        // Parse performance section
        if (auto performance = toml_data["performance"].as_table()) {
            auto& p = *performance;
            if (auto val = p["virtual_scrolling"].value<bool>()) config.performance.virtual_scrolling = *val;
            if (auto val = p["lazy_rendering"].value<bool>()) config.performance.lazy_rendering = *val;
            
            if (auto val = p["large_file_threshold"].value<int64_t>()) {
                config.performance.large_file_threshold = static_cast<size_t>(*val);
            }
            if (auto val = p["very_large_file_threshold"].value<int64_t>()) {
                config.performance.very_large_file_threshold = static_cast<size_t>(*val);
            }
            if (auto val = p["render_chunk_size"].value<int64_t>()) {
                config.performance.render_chunk_size = static_cast<size_t>(*val);
            }
            if (auto val = p["max_memory_usage_mb"].value<int64_t>()) {
                config.performance.max_memory_usage_mb = static_cast<size_t>(*val);
            }
        }
        
        // Parse markdown section
        if (auto markdown = toml_data["markdown"].as_table()) {
            auto& m = *markdown;
            if (auto val = m["syntax_highlighting"].value<bool>()) config.markdown.syntax_highlighting = *val;
            if (auto val = m["auto_continue_lists"].value<bool>()) config.markdown.auto_continue_lists = *val;
            if (auto val = m["smart_quotes"].value<bool>()) config.markdown.smart_quotes = *val;
            if (auto val = m["auto_link_detection"].value<bool>()) config.markdown.auto_link_detection = *val;
            if (auto val = m["header_folding"].value<bool>()) config.markdown.header_folding = *val;
            if (auto val = m["wiki_links"].value<bool>()) config.markdown.wiki_links = *val;
            if (auto val = m["table_formatting"].value<bool>()) config.markdown.table_formatting = *val;
        }
        
        // Parse metadata
        if (auto val = toml_data["config_version"].value<std::string>()) {
            config.config_version = *val;
        }
        
        return config;
        
    } catch (const std::exception& e) {
        return makeErrorResult<EditorConfig>(ErrorCode::kConfigError,
            "Error parsing TOML configuration: " + std::string(e.what()));
    }
}

toml::table EditorPreferences::configToToml(const EditorConfig& config) const {
    toml::table root;
    
    // Metadata
    root.insert("config_version", config.config_version);
    root.insert("last_modified", std::chrono::duration_cast<std::chrono::seconds>(
        config.last_modified.time_since_epoch()).count());
    
    // Behavior section
    toml::table behavior;
    behavior.insert("auto_indent", config.behavior.auto_indent);
    behavior.insert("smart_quotes", config.behavior.smart_quotes);
    behavior.insert("auto_save", config.behavior.auto_save);
    behavior.insert("auto_save_delay", static_cast<int64_t>(config.behavior.auto_save_delay.count()));
    behavior.insert("show_line_numbers", config.behavior.show_line_numbers);
    behavior.insert("highlight_current_line", config.behavior.highlight_current_line);
    behavior.insert("max_undo_history", static_cast<int64_t>(config.behavior.max_undo_history));
    behavior.insert("word_wrap", config.behavior.word_wrap);
    behavior.insert("tab_width", static_cast<int64_t>(config.behavior.tab_width));
    behavior.insert("use_spaces_for_tabs", config.behavior.use_spaces_for_tabs);
    root.insert("behavior", std::move(behavior));
    
    // Search section
    toml::table search;
    search.insert("case_sensitive", config.search.case_sensitive);
    search.insert("whole_words", config.search.whole_words);
    search.insert("highlight_all_matches", config.search.highlight_all_matches);
    search.insert("max_search_results", static_cast<int64_t>(config.search.max_search_results));
    search.insert("incremental_search", config.search.incremental_search);
    search.insert("search_timeout", static_cast<int64_t>(config.search.search_timeout.count()));
    root.insert("search", std::move(search));
    
    // Clipboard section
    toml::table clipboard;
    clipboard.insert("prefer_system_clipboard", config.clipboard.prefer_system_clipboard);
    clipboard.insert("internal_clipboard_size_mb", static_cast<int64_t>(config.clipboard.internal_clipboard_size_mb));
    clipboard.insert("clipboard_timeout", static_cast<int64_t>(config.clipboard.clipboard_timeout.count()));
    clipboard.insert("auto_clear_sensitive", config.clipboard.auto_clear_sensitive);
    root.insert("clipboard", std::move(clipboard));
    
    // Terminal section
    toml::table terminal;
    terminal.insert("color_support", config.terminal.color_support);
    terminal.insert("mouse_support", config.terminal.mouse_support);
    terminal.insert("detect_capabilities", config.terminal.detect_capabilities);
    terminal.insert("force_basic_mode", config.terminal.force_basic_mode);
    terminal.insert("bracketed_paste", config.terminal.bracketed_paste);
    terminal.insert("key_timeout", static_cast<int64_t>(config.terminal.key_timeout.count()));
    root.insert("terminal", std::move(terminal));
    
    // Performance section
    toml::table performance;
    performance.insert("virtual_scrolling", config.performance.virtual_scrolling);
    performance.insert("lazy_rendering", config.performance.lazy_rendering);
    performance.insert("large_file_threshold", static_cast<int64_t>(config.performance.large_file_threshold));
    performance.insert("very_large_file_threshold", static_cast<int64_t>(config.performance.very_large_file_threshold));
    performance.insert("render_chunk_size", static_cast<int64_t>(config.performance.render_chunk_size));
    performance.insert("max_memory_usage_mb", static_cast<int64_t>(config.performance.max_memory_usage_mb));
    root.insert("performance", std::move(performance));
    
    // Markdown section
    toml::table markdown;
    markdown.insert("syntax_highlighting", config.markdown.syntax_highlighting);
    markdown.insert("auto_continue_lists", config.markdown.auto_continue_lists);
    markdown.insert("smart_quotes", config.markdown.smart_quotes);
    markdown.insert("auto_link_detection", config.markdown.auto_link_detection);
    markdown.insert("header_folding", config.markdown.header_folding);
    markdown.insert("wiki_links", config.markdown.wiki_links);
    markdown.insert("table_formatting", config.markdown.table_formatting);
    root.insert("markdown", std::move(markdown));
    
    return root;
}

Result<void> EditorPreferences::ensureConfigDirectory() const {
    try {
        auto config_dir = config_file_.parent_path();
        if (!std::filesystem::exists(config_dir)) {
            std::filesystem::create_directories(config_dir);
        }
        return {};
    } catch (const std::exception& e) {
        return makeErrorResult<void>(ErrorCode::kFileError,
            "Failed to create configuration directory: " + std::string(e.what()));
    }
}

bool EditorPreferences::hasConfigFileChanged() const {
    if (!std::filesystem::exists(config_file_)) {
        return false;
    }
    
    try {
        auto file_time = std::filesystem::last_write_time(config_file_);
        auto system_time = std::chrono::file_clock::to_sys(file_time);
        return system_time > last_file_check_;
    } catch (const std::exception&) {
        return false;
    }
}

Result<void> EditorPreferences::watchConfigFile(std::function<void(const EditorConfig&)> callback) {
    // Simple polling-based file watching
    // In a production system, you might want to use inotify/kqueue for efficiency
    
    if (hasConfigFileChanged()) {
        auto load_result = loadConfig();
        if (load_result) {
            config_ = load_result.value();
            callback(config_);
            last_file_check_ = std::chrono::system_clock::now();
        }
    }
    
    return {};
}

} // namespace nx::tui