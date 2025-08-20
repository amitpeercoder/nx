#include <gtest/gtest.h>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <nlohmann/json.hpp>

#include "nx/cli/application.hpp"
#include "nx/core/note.hpp"
#include "temp_directory.hpp"

namespace nx::cli {

class NotebookCLITest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::make_unique<nx::test::TempDirectory>();
        
        // Set up notes directory
        notes_dir_ = temp_dir_->path() / "notes";
        std::filesystem::create_directories(notes_dir_);
        
        app_ = std::make_unique<Application>();
        
        // Override notes directory
#ifdef _WIN32
        _putenv_s("NX_NOTES_DIR", notes_dir_.string().c_str());
#else
        setenv("NX_NOTES_DIR", notes_dir_.string().c_str(), 1);
#endif
        
        // Generate unique test suffix for this test run
        test_suffix_ = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    }

    void TearDown() override {
#ifdef _WIN32
        _putenv_s("NX_NOTES_DIR", "");
#else
        unsetenv("NX_NOTES_DIR");
#endif
        app_.reset();
        temp_dir_.reset();
    }
    
    // Helper to run CLI command and capture output
    std::pair<int, std::string> runCommand(const std::vector<std::string>& args) {
        // Prepare argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>("nx"));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        
        // Capture stdout and stderr
        std::ostringstream cout_output, cerr_output;
        std::streambuf* orig_cout = std::cout.rdbuf();
        std::streambuf* orig_cerr = std::cerr.rdbuf();
        std::cout.rdbuf(cout_output.rdbuf());
        std::cerr.rdbuf(cerr_output.rdbuf());
        
        int result = 0;
        try {
            result = app_->run(static_cast<int>(argv.size()), argv.data());
        } catch (const std::exception& e) {
            std::cout.rdbuf(orig_cout);
            std::cerr.rdbuf(orig_cerr);
            throw;
        }
        
        std::cout.rdbuf(orig_cout);
        std::cerr.rdbuf(orig_cerr);
        
        // Combine stdout and stderr for unified output
        std::string combined_output = cout_output.str() + cerr_output.str();
        return {result, combined_output};
    }
    
    // Helper to create a test note file
    void createTestNote(const std::string& title, const std::string& content, const std::string& notebook = "") {
        auto note = nx::core::Note::create(title, content);
        if (!notebook.empty()) {
            note.setNotebook(notebook);
        }
        
        // Manually create note file
        auto note_path = notes_dir_ / (note.id().toString() + ".md");
        std::ofstream file(note_path);
        file << "---\n";
        file << "id: " << note.id().toString() << "\n";
        file << "created: " << std::chrono::duration_cast<std::chrono::seconds>(note.metadata().created().time_since_epoch()).count() << "\n";
        file << "modified: " << std::chrono::duration_cast<std::chrono::seconds>(note.metadata().updated().time_since_epoch()).count() << "\n";
        if (!notebook.empty()) {
            file << "notebook: " << notebook << "\n";
        }
        file << "---\n\n";
        file << "# " << title << "\n\n";
        file << content << "\n";
    }

    std::unique_ptr<nx::test::TempDirectory> temp_dir_;
    std::filesystem::path notes_dir_;
    std::unique_ptr<Application> app_;
    std::string test_suffix_;
};

// Test notebook list command
TEST_F(NotebookCLITest, ListNotebooks) {
    // Initially no notebooks
    auto [result1, output1] = runCommand({"notebook", "list"});
    EXPECT_EQ(result1, 0);
    EXPECT_TRUE(output1.find("No notebooks found") != std::string::npos);
    
    // Create some notebooks
    auto [result2, output2] = runCommand({"notebook", "create", "work"});
    EXPECT_EQ(result2, 0);
    
    auto [result3, output3] = runCommand({"notebook", "create", "personal"});
    EXPECT_EQ(result3, 0);
    
    // List notebooks
    auto [result4, output4] = runCommand({"notebook", "list"});
    EXPECT_EQ(result4, 0);
    EXPECT_TRUE(output4.find("work") != std::string::npos);
    EXPECT_TRUE(output4.find("personal") != std::string::npos);
    EXPECT_TRUE(output4.find("Total: 2 notebooks") != std::string::npos);
}

// Test notebook creation
TEST_F(NotebookCLITest, CreateNotebook) {
    std::string notebook_name = "test-notebook-" + test_suffix_;
    auto [result, output] = runCommand({"notebook", "create", notebook_name});
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(output.find("Created notebook: " + notebook_name) != std::string::npos);
    
    // Verify notebook exists
    auto [result2, output2] = runCommand({"notebook", "list"});
    EXPECT_EQ(result2, 0);
    EXPECT_TRUE(output2.find(notebook_name) != std::string::npos);
}

// Test duplicate notebook creation
TEST_F(NotebookCLITest, CreateDuplicateNotebook) {
    std::string notebook_name = "work-" + test_suffix_;
    // Create first notebook
    auto [result1, output1] = runCommand({"notebook", "create", notebook_name});
    EXPECT_EQ(result1, 0);
    
    // Try to create duplicate
    auto [result2, output2] = runCommand({"notebook", "create", notebook_name});
    EXPECT_NE(result2, 0);
    EXPECT_TRUE(output2.find("Error") != std::string::npos);
}

// Test notebook renaming
TEST_F(NotebookCLITest, RenameNotebook) {
    std::string old_name = "old-name-" + test_suffix_;
    std::string new_name = "new-name-" + test_suffix_;
    
    // Create notebook
    auto [result1, output1] = runCommand({"notebook", "create", old_name});
    EXPECT_EQ(result1, 0);
    
    // Rename notebook
    auto [result2, output2] = runCommand({"notebook", "rename", old_name, new_name});
    EXPECT_EQ(result2, 0);
    EXPECT_TRUE(output2.find("Renamed notebook '" + old_name + "' to '" + new_name + "'") != std::string::npos);
    
    // Verify old name doesn't exist
    auto [result3, output3] = runCommand({"notebook", "info", old_name});
    EXPECT_NE(result3, 0);
    
    // Verify new name exists
    auto [result4, output4] = runCommand({"notebook", "info", new_name});
    EXPECT_EQ(result4, 0);
}

// Test notebook deletion
TEST_F(NotebookCLITest, DeleteNotebook) {
    // Create notebook
    auto [result1, output1] = runCommand({"notebook", "create", "temp-notebook"});
    EXPECT_EQ(result1, 0);
    
    // Delete notebook
    auto [result2, output2] = runCommand({"notebook", "delete", "temp-notebook"});
    EXPECT_EQ(result2, 0);
    EXPECT_TRUE(output2.find("Deleted notebook: temp-notebook") != std::string::npos);
    
    // Verify notebook is gone
    auto [result3, output3] = runCommand({"notebook", "info", "temp-notebook"});
    EXPECT_NE(result3, 0);
}

// Test force deletion
TEST_F(NotebookCLITest, ForceDeleteNotebook) {
    // Create notebook and add real note
    auto [result1, output1] = runCommand({"notebook", "create", "work"});
    EXPECT_EQ(result1, 0);
    
    createTestNote("Important Note", "Don't delete this", "work");
    
    // Try to delete without force (should fail)
    auto [result2, output2] = runCommand({"notebook", "delete", "work"});
    EXPECT_NE(result2, 0);
    
    // Delete with force (should succeed)
    auto [result3, output3] = runCommand({"notebook", "delete", "work", "--force"});
    EXPECT_EQ(result3, 0);
    EXPECT_TRUE(output3.find("Deleted notebook: work") != std::string::npos);
}

// Test notebook info
TEST_F(NotebookCLITest, NotebookInfo) {
    // Create notebook
    auto [result1, output1] = runCommand({"notebook", "create", "project"});
    EXPECT_EQ(result1, 0);
    
    // Get basic info
    auto [result2, output2] = runCommand({"notebook", "info", "project"});
    EXPECT_EQ(result2, 0);
    EXPECT_TRUE(output2.find("Notebook: project") != std::string::npos);
    EXPECT_TRUE(output2.find("Notes:") != std::string::npos);
    
    // Get info with stats
    auto [result3, output3] = runCommand({"notebook", "info", "project", "--stats"});
    EXPECT_EQ(result3, 0);
    EXPECT_TRUE(output3.find("Total size:") != std::string::npos);
    EXPECT_TRUE(output3.find("Created:") != std::string::npos);
    EXPECT_TRUE(output3.find("Last modified:") != std::string::npos);
}

// Test JSON output
TEST_F(NotebookCLITest, JSONOutput) {
    std::string notebook_name = "test-" + test_suffix_;
    
    // Create notebook
    auto [result1, output1] = runCommand({"notebook", "create", notebook_name});
    EXPECT_EQ(result1, 0);
    
    // Get JSON list
    auto [result2, output2] = runCommand({"notebook", "list", "--json"});
    EXPECT_EQ(result2, 0);
    
    // Parse JSON
    EXPECT_NO_THROW({
        auto json = nlohmann::json::parse(output2);
        EXPECT_TRUE(json.is_array());
        EXPECT_GE(json.size(), 1);
        
        bool found = false;
        for (const auto& notebook : json) {
            if (notebook["name"] == notebook_name) {
                found = true;
                EXPECT_TRUE(notebook.contains("note_count"));
                break;
            }
        }
        EXPECT_TRUE(found);
    });
    
    // Get JSON info
    auto [result3, output3] = runCommand({"notebook", "info", notebook_name, "--json"});
    EXPECT_EQ(result3, 0);
    
    EXPECT_NO_THROW({
        auto json = nlohmann::json::parse(output3);
        EXPECT_EQ(json["name"], notebook_name);
        EXPECT_TRUE(json.contains("note_count"));
    });
}

// Test error handling
TEST_F(NotebookCLITest, ErrorHandling) {
    // Test operations on non-existent notebook
    auto [result1, output1] = runCommand({"notebook", "info", "nonexistent"});
    EXPECT_NE(result1, 0);
    EXPECT_TRUE(output1.find("Error") != std::string::npos);
    
    auto [result2, output2] = runCommand({"notebook", "rename", "nonexistent", "newname"});
    EXPECT_NE(result2, 0);
    EXPECT_TRUE(output2.find("Error") != std::string::npos);
    
    auto [result3, output3] = runCommand({"notebook", "delete", "nonexistent"});
    EXPECT_NE(result3, 0);
    EXPECT_TRUE(output3.find("Error") != std::string::npos);
}

// Test invalid notebook names
TEST_F(NotebookCLITest, InvalidNotebookNames) {
    // Test empty name
    auto [result1, output1] = runCommand({"notebook", "create", ""});
    EXPECT_NE(result1, 0);
    
    // Test names with invalid characters
    auto [result2, output2] = runCommand({"notebook", "create", "invalid/name"});
    EXPECT_NE(result2, 0);
    
    auto [result3, output3] = runCommand({"notebook", "create", "invalid\\name"});
    EXPECT_NE(result3, 0);
}

// Test command help
TEST_F(NotebookCLITest, CommandHelp) {
    auto [result, output] = runCommand({"notebook", "--help"});
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(output.find("Manage notebooks") != std::string::npos);
    EXPECT_TRUE(output.find("list") != std::string::npos);
    EXPECT_TRUE(output.find("create") != std::string::npos);
    EXPECT_TRUE(output.find("rename") != std::string::npos);
    EXPECT_TRUE(output.find("delete") != std::string::npos);
    EXPECT_TRUE(output.find("info") != std::string::npos);
}

} // namespace nx::cli