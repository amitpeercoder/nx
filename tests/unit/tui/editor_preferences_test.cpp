#include <gtest/gtest.h>
#include "nx/tui/editor_preferences.hpp"
#include <filesystem>
#include <fstream>

using namespace nx::tui;

class EditorPreferencesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test config
        test_dir_ = std::filesystem::temp_directory_path() / "nx_test_config";
        std::filesystem::create_directories(test_dir_);
        config_file_ = test_dir_ / "editor.toml";
    }
    
    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path config_file_;
};

TEST_F(EditorPreferencesTest, DefaultConfigCreation) {
    EditorPreferences prefs(test_dir_);
    auto config = prefs.getConfig();
    
    // Test default values
    EXPECT_TRUE(config.behavior.auto_indent);
    EXPECT_FALSE(config.behavior.smart_quotes);
    EXPECT_TRUE(config.behavior.auto_save);
    EXPECT_EQ(config.behavior.tab_width, 4);
    EXPECT_FALSE(config.behavior.show_line_numbers);  // Default is false
    EXPECT_FALSE(config.behavior.word_wrap);  // Default is false
    
    EXPECT_FALSE(config.search.case_sensitive);
    EXPECT_FALSE(config.search.whole_words);
    EXPECT_TRUE(config.search.highlight_all_matches);
    EXPECT_EQ(config.search.max_search_results, 1000);
    
    EXPECT_TRUE(config.clipboard.prefer_system_clipboard);
    EXPECT_EQ(config.clipboard.internal_clipboard_size_mb, 10);
    
    EXPECT_EQ(config.terminal.color_support, "auto");
    EXPECT_TRUE(config.terminal.mouse_support);
    
    EXPECT_TRUE(config.performance.virtual_scrolling);
    EXPECT_TRUE(config.performance.lazy_rendering);
    EXPECT_EQ(config.performance.large_file_threshold, 1000);
    
    EXPECT_TRUE(config.markdown.syntax_highlighting);
    EXPECT_TRUE(config.markdown.auto_continue_lists);
    EXPECT_TRUE(config.markdown.wiki_links);
}

TEST_F(EditorPreferencesTest, SaveAndLoadConfig) {
    // Create preferences with custom config
    EditorPreferences prefs(test_dir_);
    auto config = prefs.getConfig();
    
    // Modify some values
    config.behavior.auto_indent = false;
    config.behavior.tab_width = 8;
    config.search.case_sensitive = true;
    config.search.max_search_results = 500;
    config.clipboard.prefer_system_clipboard = false;
    config.terminal.color_support = "never";
    config.performance.virtual_scrolling = false;
    config.markdown.syntax_highlighting = false;
    
    // Save config
    auto save_result = prefs.updateConfig(config);
    ASSERT_TRUE(save_result);
    
    // Verify file was created
    EXPECT_TRUE(std::filesystem::exists(config_file_));
    
    // Create new preferences instance and load config
    EditorPreferences prefs2(test_dir_);
    auto loaded_config = prefs2.getConfig();
    
    // Verify loaded values match saved values
    EXPECT_FALSE(loaded_config.behavior.auto_indent);
    EXPECT_EQ(loaded_config.behavior.tab_width, 8);
    EXPECT_TRUE(loaded_config.search.case_sensitive);
    EXPECT_EQ(loaded_config.search.max_search_results, 500);
    EXPECT_FALSE(loaded_config.clipboard.prefer_system_clipboard);
    EXPECT_EQ(loaded_config.terminal.color_support, "never");
    EXPECT_FALSE(loaded_config.performance.virtual_scrolling);
    EXPECT_FALSE(loaded_config.markdown.syntax_highlighting);
}

TEST_F(EditorPreferencesTest, ConfigValidation) {
    EditorPreferences prefs(test_dir_);
    auto config = prefs.getConfig();
    
    // Test invalid tab_width
    config.behavior.tab_width = 0;
    auto validation_result = prefs.validateConfig(config);
    EXPECT_FALSE(validation_result);
    
    config.behavior.tab_width = 20;
    validation_result = prefs.validateConfig(config);
    EXPECT_FALSE(validation_result);
    
    // Test invalid max_search_results
    config.behavior.tab_width = 4; // Reset to valid
    config.search.max_search_results = 0;
    validation_result = prefs.validateConfig(config);
    EXPECT_FALSE(validation_result);
    
    // Test invalid color_support
    config.search.max_search_results = 1000; // Reset to valid
    config.terminal.color_support = "invalid";
    validation_result = prefs.validateConfig(config);
    EXPECT_FALSE(validation_result);
    
    // Test valid config
    config.terminal.color_support = "auto"; // Reset to valid
    validation_result = prefs.validateConfig(config);
    EXPECT_TRUE(validation_result);
}

TEST_F(EditorPreferencesTest, TOMLFormat) {
    EditorPreferences prefs(test_dir_);
    auto config = prefs.getConfig();
    
    // Modify config
    config.behavior.auto_indent = false;
    config.behavior.tab_width = 2;
    config.search.case_sensitive = true;
    config.markdown.syntax_highlighting = false;
    
    // Save config
    auto save_result = prefs.updateConfig(config);
    ASSERT_TRUE(save_result);
    
    // Read and verify TOML file content
    std::ifstream file(config_file_);
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Check for expected TOML sections and values
    EXPECT_NE(content.find("[behavior]"), std::string::npos);
    EXPECT_NE(content.find("[search]"), std::string::npos);
    EXPECT_NE(content.find("[clipboard]"), std::string::npos);
    EXPECT_NE(content.find("[terminal]"), std::string::npos);
    EXPECT_NE(content.find("[performance]"), std::string::npos);
    EXPECT_NE(content.find("[markdown]"), std::string::npos);
    
    EXPECT_NE(content.find("auto_indent = false"), std::string::npos);
    EXPECT_NE(content.find("tab_width = 2"), std::string::npos);
    EXPECT_NE(content.find("case_sensitive = true"), std::string::npos);
    EXPECT_NE(content.find("syntax_highlighting = false"), std::string::npos);
}

TEST_F(EditorPreferencesTest, ResetToDefaults) {
    EditorPreferences prefs(test_dir_);
    auto config = prefs.getConfig();
    
    // Modify config
    config.behavior.auto_indent = false;
    config.behavior.tab_width = 8;
    config.search.case_sensitive = true;
    
    auto save_result = prefs.updateConfig(config);
    ASSERT_TRUE(save_result);
    
    // Reset to defaults
    auto reset_result = prefs.resetToDefaults();
    ASSERT_TRUE(reset_result);
    
    // Verify config is back to defaults
    auto default_config = prefs.getConfig();
    EXPECT_TRUE(default_config.behavior.auto_indent);
    EXPECT_EQ(default_config.behavior.tab_width, 4);
    EXPECT_FALSE(default_config.search.case_sensitive);
}