#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>
#include <thread>

#include "nx/store/notebook_manager.hpp"
#include "nx/store/filesystem_store.hpp"
#include "nx/core/note.hpp"
#include "nx/core/note_id.hpp"
#include "temp_directory.hpp"

namespace nx::store {

class NotebookManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::make_unique<nx::test::TempDirectory>();
        
        // Setup filesystem store
        FilesystemStore::Config config;
        config.notes_dir = temp_dir_->path() / "notes";
        config.attachments_dir = temp_dir_->path() / "attachments"; 
        config.trash_dir = temp_dir_->path() / "trash";
        
        store_ = std::make_unique<FilesystemStore>(config);
        auto validation = store_->validate();
        ASSERT_TRUE(validation.has_value()) << "Store validation failed: " << validation.error().message();
        
        notebook_manager_ = std::make_unique<NotebookManager>(*store_);
    }

    void TearDown() override {
        notebook_manager_.reset();
        store_.reset();
        temp_dir_.reset();
    }
    
    // Helper to create a test note in a specific notebook
    nx::core::Note createTestNote(const std::string& title, const std::string& content, const std::string& notebook = "") {
        auto note = nx::core::Note::create(title, content);
        if (!notebook.empty()) {
            note.setNotebook(notebook);
        }
        return note;
    }

    std::unique_ptr<nx::test::TempDirectory> temp_dir_;
    std::unique_ptr<FilesystemStore> store_;
    std::unique_ptr<NotebookManager> notebook_manager_;
};

// Test notebook creation
TEST_F(NotebookManagerTest, CreateNotebook) {
    auto result = notebook_manager_->createNotebook("work");
    ASSERT_TRUE(result.has_value()) << "Failed to create notebook: " << result.error().message();
    
    // Verify notebook was created by listing notebooks
    auto notebooks_result = notebook_manager_->listNotebooks();
    ASSERT_TRUE(notebooks_result.has_value());
    
    bool found = false;
    for (const auto& notebook : notebooks_result.value()) {
        if (notebook.name == "work") {
            found = true;
            EXPECT_EQ(notebook.note_count, 1); // Should contain placeholder note
            break;
        }
    }
    EXPECT_TRUE(found) << "Created notebook not found in list";
}

// Test duplicate notebook creation
TEST_F(NotebookManagerTest, CreateDuplicateNotebook) {
    // Create first notebook
    auto result1 = notebook_manager_->createNotebook("work");
    ASSERT_TRUE(result1.has_value());
    
    // Try to create duplicate
    auto result2 = notebook_manager_->createNotebook("work");
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(result2.error().code(), ErrorCode::kValidationError);
}

// Test notebook listing
TEST_F(NotebookManagerTest, ListNotebooks) {
    // Create multiple notebooks
    ASSERT_TRUE(notebook_manager_->createNotebook("work").has_value());
    ASSERT_TRUE(notebook_manager_->createNotebook("personal").has_value());
    ASSERT_TRUE(notebook_manager_->createNotebook("projects").has_value());
    
    auto result = notebook_manager_->listNotebooks();
    ASSERT_TRUE(result.has_value());
    
    auto notebooks = result.value();
    EXPECT_EQ(notebooks.size(), 3);
    
    // Check that all notebooks are present
    std::set<std::string> notebook_names;
    for (const auto& notebook : notebooks) {
        notebook_names.insert(notebook.name);
        EXPECT_GT(notebook.note_count, 0); // Should have at least placeholder note
    }
    
    EXPECT_TRUE(notebook_names.count("work"));
    EXPECT_TRUE(notebook_names.count("personal"));
    EXPECT_TRUE(notebook_names.count("projects"));
}

// Test notebook renaming
TEST_F(NotebookManagerTest, RenameNotebook) {
    // Create notebook with some notes
    ASSERT_TRUE(notebook_manager_->createNotebook("work").has_value());
    
    // Add a real note to the notebook
    auto note = createTestNote("Meeting Notes", "Important discussion", "work");
    ASSERT_TRUE(store_->store(note).has_value());
    
    // Rename notebook
    auto result = notebook_manager_->renameNotebook("work", "office");
    ASSERT_TRUE(result.has_value()) << "Failed to rename notebook: " << result.error().message();
    
    // Verify old notebook doesn't exist
    auto old_info = notebook_manager_->getNotebookInfo("work");
    EXPECT_FALSE(old_info.has_value());
    
    // Verify new notebook exists with notes
    auto new_info = notebook_manager_->getNotebookInfo("office");
    ASSERT_TRUE(new_info.has_value());
    EXPECT_EQ(new_info->name, "office");
    EXPECT_GE(new_info->note_count, 1);
    
    // Verify note was moved to new notebook
    auto note_result = store_->load(note.id());
    ASSERT_TRUE(note_result.has_value());
    EXPECT_EQ(note_result->notebook(), "office");
}

// Test renaming non-existent notebook
TEST_F(NotebookManagerTest, RenameNonExistentNotebook) {
    auto result = notebook_manager_->renameNotebook("nonexistent", "newname");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kNotFound);
}

// Test notebook deletion
TEST_F(NotebookManagerTest, DeleteNotebook) {
    // Create notebook (using non-reserved name)
    ASSERT_TRUE(notebook_manager_->createNotebook("empty_notebook").has_value());
    
    // Delete notebook (only placeholder notes, should work without force)
    auto result = notebook_manager_->deleteNotebook("empty_notebook", false);
    ASSERT_TRUE(result.has_value()) << "Failed to delete notebook: " << result.error().message();
    
    // Verify notebook is gone
    auto info = notebook_manager_->getNotebookInfo("empty_notebook");
    EXPECT_FALSE(info.has_value());
}

// Test force deletion of notebook with real notes
TEST_F(NotebookManagerTest, ForceDeleteNotebookWithNotes) {
    // Create notebook with real notes (using non-reserved name)
    ASSERT_TRUE(notebook_manager_->createNotebook("temporary").has_value());
    
    auto note = createTestNote("Important Note", "Don't delete this", "temporary");
    ASSERT_TRUE(store_->store(note).has_value());
    
    // Try to delete without force (should fail)
    auto result1 = notebook_manager_->deleteNotebook("temporary", false);
    EXPECT_FALSE(result1.has_value());
    
    // Delete with force (should succeed)
    auto result2 = notebook_manager_->deleteNotebook("temporary", true);
    ASSERT_TRUE(result2.has_value()) << "Failed to force delete notebook: " << result2.error().message();
    
    // Verify notebook and notes are gone
    auto info = notebook_manager_->getNotebookInfo("temporary");
    EXPECT_FALSE(info.has_value());
    
    auto note_result = store_->load(note.id());
    EXPECT_FALSE(note_result.has_value());
}

// Test notebook info retrieval
TEST_F(NotebookManagerTest, GetNotebookInfo) {
    // Create notebook and add notes
    ASSERT_TRUE(notebook_manager_->createNotebook("project").has_value());
    
    auto note1 = createTestNote("Task 1", "First task", "project");
    note1.setTags({"urgent", "work"});
    ASSERT_TRUE(store_->store(note1).has_value());
    
    // Add small delay to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto note2 = createTestNote("Task 2", "Second task", "project");
    note2.setTags({"work", "planning"});
    ASSERT_TRUE(store_->store(note2).has_value());
    
    // Get notebook info with stats
    auto result = notebook_manager_->getNotebookInfo("project", true);
    ASSERT_TRUE(result.has_value());
    
    auto info = result.value();
    EXPECT_EQ(info.name, "project");
    EXPECT_GE(info.note_count, 2); // At least our 2 notes (plus placeholder)
    EXPECT_GE(info.total_size, 0);
    
    // Check tags are present
    EXPECT_TRUE(std::find(info.tags.begin(), info.tags.end(), "urgent") != info.tags.end());
    EXPECT_TRUE(std::find(info.tags.begin(), info.tags.end(), "work") != info.tags.end());
    EXPECT_TRUE(std::find(info.tags.begin(), info.tags.end(), "planning") != info.tags.end());
    
    // Check tag counts
    EXPECT_EQ(info.tag_counts.at("work"), 2);
    EXPECT_EQ(info.tag_counts.at("urgent"), 1);
    EXPECT_EQ(info.tag_counts.at("planning"), 1);
}

// Test notebook info for non-existent notebook
TEST_F(NotebookManagerTest, GetNonExistentNotebookInfo) {
    auto result = notebook_manager_->getNotebookInfo("nonexistent");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::kNotFound);
}

// Test listing notebooks by notebook
TEST_F(NotebookManagerTest, ListNotesByNotebook) {
    // Create notebooks and notes
    ASSERT_TRUE(notebook_manager_->createNotebook("list_test_work").has_value());
    ASSERT_TRUE(notebook_manager_->createNotebook("list_test_personal").has_value());
    
    auto work_note1 = createTestNote("Work Note 1", "Content 1", "list_test_work");
    auto work_note2 = createTestNote("Work Note 2", "Content 2", "list_test_work");
    auto personal_note = createTestNote("Personal Note", "Personal content", "list_test_personal");
    
    ASSERT_TRUE(store_->store(work_note1).has_value());
    ASSERT_TRUE(store_->store(work_note2).has_value());
    ASSERT_TRUE(store_->store(personal_note).has_value());
    
    // List notes by notebook
    auto work_note_ids = notebook_manager_->getNotesInNotebook("list_test_work");
    ASSERT_TRUE(work_note_ids.has_value());
    
    // Count non-placeholder notes
    int real_work_notes = 0;
    for (const auto& note_id : work_note_ids.value()) {
        auto note_result = store_->load(note_id);
        if (note_result.has_value() && !note_result->title().starts_with(".notebook_")) {
            real_work_notes++;
        }
    }
    
    EXPECT_EQ(real_work_notes, 2);
    
    auto personal_note_ids = notebook_manager_->getNotesInNotebook("list_test_personal");
    ASSERT_TRUE(personal_note_ids.has_value());
    
    int real_personal_notes = 0;
    for (const auto& note_id : personal_note_ids.value()) {
        auto note_result = store_->load(note_id);
        if (note_result.has_value() && !note_result->title().starts_with(".notebook_")) {
            real_personal_notes++;
        }
    }
    
    EXPECT_EQ(real_personal_notes, 1);
}

// Test empty notebook listing
TEST_F(NotebookManagerTest, EmptyNotebookList) {
    auto result = notebook_manager_->listNotebooks();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

// Test notebook validation
TEST_F(NotebookManagerTest, NotebookNameValidation) {
    // Test invalid names
    EXPECT_FALSE(notebook_manager_->createNotebook("").has_value());
    EXPECT_FALSE(notebook_manager_->createNotebook("   ").has_value());
    EXPECT_FALSE(notebook_manager_->createNotebook("invalid/name").has_value());
    EXPECT_FALSE(notebook_manager_->createNotebook("invalid\\name").has_value());
    
    // Test valid names
    EXPECT_TRUE(notebook_manager_->createNotebook("valid-name").has_value());
    EXPECT_TRUE(notebook_manager_->createNotebook("valid_name").has_value());
    EXPECT_TRUE(notebook_manager_->createNotebook("ValidName123").has_value());
}

} // namespace nx::store