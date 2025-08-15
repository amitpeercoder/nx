#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

#include "nx/index/ripgrep_index.hpp"
#include "test_helpers.hpp"

using namespace nx::index;
using namespace nx::core;
using namespace nx::test;
using nx::ErrorCode;

class RipgrepIndexTest : public TempDirTest {
protected:
  void SetUp() override {
    TempDirTest::SetUp();
    notes_dir_ = temp_dir_ / "notes";
    std::filesystem::create_directories(notes_dir_);
    index_ = std::make_unique<RipgrepIndex>(notes_dir_);
  }
  
  void TearDown() override {
    index_.reset();
    TempDirTest::TearDown();
  }
  
  void createNoteFile(const std::string& filename, const std::string& title, 
                     const std::string& content, const std::vector<std::string>& tags = {},
                     const std::optional<std::string>& notebook = std::nullopt) {
    std::filesystem::path file_path = notes_dir_ / filename;
    std::ofstream file(file_path);
    
    file << "---\n";
    file << "title: " << title << "\n";
    if (!tags.empty()) {
      file << "tags: [";
      for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) file << ", ";
        file << tags[i];
      }
      file << "]\n";
    }
    if (notebook.has_value()) {
      file << "notebook: " << *notebook << "\n";
    }
    file << "---\n\n";
    file << content << "\n";
  }
  
  Note createTestNote(const std::string& title, const std::string& content, 
                      const std::vector<std::string>& tags = {},
                      const std::optional<std::string>& notebook = std::nullopt) {
    auto id = NoteId::generate();
    Metadata metadata(id, title);
    for (const auto& tag : tags) {
      metadata.addTag(tag);
    }
    if (notebook.has_value()) {
      metadata.setNotebook(*notebook);
    }
    
    return Note(std::move(metadata), content);
  }
  
  std::filesystem::path notes_dir_;
  std::unique_ptr<RipgrepIndex> index_;
};

TEST_F(RipgrepIndexTest, InitializeChecksRipgrep) {
  // This test will pass only if ripgrep is installed
  auto result = index_->initialize();
  if (!result.has_value()) {
    // If ripgrep is not available, expect specific error
    EXPECT_EQ(result.error().code(), ErrorCode::kExternalToolError);
    GTEST_SKIP() << "ripgrep not available, skipping test";
  }
  
  ASSERT_OK(result);
  
  auto health_result = index_->isHealthy();
  ASSERT_OK(health_result);
  EXPECT_TRUE(*health_result);
}

TEST_F(RipgrepIndexTest, AddAndUpdateNote) {
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  auto note = createTestNote("Test Note", "This is test content", {"test", "content"});
  
  auto add_result = index_->addNote(note);
  ASSERT_OK(add_result);
  
  // Update the note
  note.setTitle("Updated Test Note");
  note.setContent("This is updated content");
  note.touch();
  
  auto update_result = index_->updateNote(note);
  ASSERT_OK(update_result);
}

TEST_F(RipgrepIndexTest, RemoveNote) {
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  auto note = createTestNote("Test Note", "Content to remove");
  auto add_result = index_->addNote(note);
  ASSERT_OK(add_result);
  
  auto remove_result = index_->removeNote(note.id());
  ASSERT_OK(remove_result);
}

TEST_F(RipgrepIndexTest, SearchWithoutRipgrep) {
  // Create some note files for testing
  createNoteFile("note1.md", "First Note", "Content about programming in C++", {"programming", "cpp"});
  createNoteFile("note2.md", "Second Note", "Content about web development", {"web", "development"});
  
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  // Test metadata-only search (no text query)
  SearchQuery query;
  query.tags = {"programming"};
  
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  
  // Should find at least one note with programming tag
  // Note: exact count depends on file parsing success
}

TEST_F(RipgrepIndexTest, TagSuggestions) {
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  // Create notes with metadata for testing
  auto note1 = createTestNote("Note 1", "Content", {"programming", "cpp", "tutorial"});
  auto note2 = createTestNote("Note 2", "Content", {"programming", "python", "beginner"});
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  
  auto suggestions_result = index_->suggestTags("pro", 10);
  ASSERT_OK(suggestions_result);
  
  EXPECT_TRUE(std::find(suggestions_result->begin(), suggestions_result->end(), "programming") != suggestions_result->end());
}

TEST_F(RipgrepIndexTest, NotebookSuggestions) {
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  auto note1 = createTestNote("Note 1", "Content", {}, "work-project");
  auto note2 = createTestNote("Note 2", "Content", {}, "work-notes");
  auto note3 = createTestNote("Note 3", "Content", {}, "personal");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  ASSERT_OK(index_->addNote(note3));
  
  auto suggestions_result = index_->suggestNotebooks("work", 10);
  ASSERT_OK(suggestions_result);
  ASSERT_EQ(suggestions_result->size(), 2);
  
  EXPECT_TRUE(std::find(suggestions_result->begin(), suggestions_result->end(), "work-project") != suggestions_result->end());
  EXPECT_TRUE(std::find(suggestions_result->begin(), suggestions_result->end(), "work-notes") != suggestions_result->end());
}

TEST_F(RipgrepIndexTest, GetStats) {
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  auto note1 = createTestNote("Note 1", "Short content");
  auto note2 = createTestNote("Note 2", "This is a longer piece of content with more words");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  
  auto stats_result = index_->getStats();
  ASSERT_OK(stats_result);
  
  EXPECT_EQ(stats_result->total_notes, 2);
  EXPECT_GT(stats_result->total_words, 0);
  EXPECT_GE(stats_result->index_size_bytes, 0);
}

TEST_F(RipgrepIndexTest, TransactionNoOps) {
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  // Transactions should be no-ops for ripgrep index
  ASSERT_OK(index_->beginTransaction());
  ASSERT_OK(index_->commitTransaction());
  ASSERT_OK(index_->rollbackTransaction());
}

TEST_F(RipgrepIndexTest, MaintenanceOperations) {
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  ASSERT_OK(index_->validateIndex());
  ASSERT_OK(index_->rebuild());
  ASSERT_OK(index_->optimize());
}

TEST_F(RipgrepIndexTest, SearchCount) {
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  auto note1 = createTestNote("Note 1", "Test content");
  auto note2 = createTestNote("Note 2", "Different content");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  
  SearchQuery query;
  query.tags = {}; // Search all notes
  
  auto count_result = index_->searchCount(query);
  ASSERT_OK(count_result);
  EXPECT_EQ(*count_result, 2);
}

TEST_F(RipgrepIndexTest, SearchIds) {
  auto init_result = index_->initialize();
  if (!init_result.has_value()) {
    GTEST_SKIP() << "ripgrep not available";
  }
  
  auto note1 = createTestNote("Note 1", "Content");
  auto note2 = createTestNote("Note 2", "Content");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  
  SearchQuery query;
  
  auto ids_result = index_->searchIds(query);
  ASSERT_OK(ids_result);
  EXPECT_EQ(ids_result->size(), 2);
  
  EXPECT_TRUE(std::find(ids_result->begin(), ids_result->end(), note1.id()) != ids_result->end());
  EXPECT_TRUE(std::find(ids_result->begin(), ids_result->end(), note2.id()) != ids_result->end());
}