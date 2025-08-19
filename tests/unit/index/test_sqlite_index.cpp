#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "nx/index/sqlite_index.hpp"
#include "test_helpers.hpp"

using namespace nx::index;
using namespace nx::core;
using namespace nx::test;
using nx::ErrorCode;

class SqliteIndexTest : public TempDirTest {
protected:
  void SetUp() override {
    TempDirTest::SetUp();
    db_path_ = temp_dir_ / "test_index.db";
    index_ = std::make_unique<SqliteIndex>(db_path_);
    
    auto result = index_->initialize();
    ASSERT_OK(result);
  }
  
  void TearDown() override {
    index_.reset();
    TempDirTest::TearDown();
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
  
  std::filesystem::path db_path_;
  std::unique_ptr<SqliteIndex> index_;
};

TEST_F(SqliteIndexTest, InitializeCreatesDatabase) {
  EXPECT_TRUE(std::filesystem::exists(db_path_));
  
  auto health_result = index_->isHealthy();
  ASSERT_OK(health_result);
  EXPECT_TRUE(*health_result);
}

TEST_F(SqliteIndexTest, AddAndSearchNote) {
  auto note = createTestNote("Test Note", "This is test content with keywords", {"test", "content"});
  
  auto add_result = index_->addNote(note);
  ASSERT_OK(add_result);
  
  SearchQuery query;
  query.text = "keywords";
  
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 1);
  
  const auto& result = search_result->front();
  EXPECT_EQ(result.id, note.id());
  // Title is now derived from content first line
  EXPECT_EQ(result.title, "This is test content with keywords");
  EXPECT_FALSE(result.snippet.empty());
  EXPECT_GT(result.score, 0.0);
}

TEST_F(SqliteIndexTest, SearchMultipleNotes) {
  auto note1 = createTestNote("First Note", "Content about programming in C++", {"programming", "cpp"});
  auto note2 = createTestNote("Second Note", "Content about web development", {"web", "development"});
  auto note3 = createTestNote("Third Note", "More programming content in Python", {"programming", "python"});
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  ASSERT_OK(index_->addNote(note3));
  
  SearchQuery query;
  query.text = "programming";
  
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 2);
  
  // Results should be sorted by relevance
  for (const auto& result : *search_result) {
    EXPECT_GT(result.score, 0.0);
    EXPECT_TRUE(result.id == note1.id() || result.id == note3.id());
  }
}

TEST_F(SqliteIndexTest, SearchWithTagFilter) {
  auto note1 = createTestNote("Note 1", "Content", {"tag1", "common"});
  auto note2 = createTestNote("Note 2", "Content", {"tag2", "common"});
  auto note3 = createTestNote("Note 3", "Content", {"tag1", "unique"});
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  ASSERT_OK(index_->addNote(note3));
  
  SearchQuery query;
  query.text = "Content";
  query.tags = {"tag1"};
  
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 2);
  
  for (const auto& result : *search_result) {
    EXPECT_TRUE(result.id == note1.id() || result.id == note3.id());
  }
}

TEST_F(SqliteIndexTest, SearchWithNotebookFilter) {
  auto note1 = createTestNote("Note 1", "Content", {}, "work");
  auto note2 = createTestNote("Note 2", "Content", {}, "personal");
  auto note3 = createTestNote("Note 3", "Content", {}, "work");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  ASSERT_OK(index_->addNote(note3));
  
  SearchQuery query;
  query.text = "Content";
  query.notebook = "work";
  
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 2);
  
  for (const auto& result : *search_result) {
    EXPECT_TRUE(result.id == note1.id() || result.id == note3.id());
  }
}

TEST_F(SqliteIndexTest, SearchIds) {
  auto note1 = createTestNote("Note 1", "Test content");
  auto note2 = createTestNote("Note 2", "Different content");
  auto note3 = createTestNote("Note 3", "Test data");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  ASSERT_OK(index_->addNote(note3));
  
  SearchQuery query;
  query.text = "Test";
  
  auto ids_result = index_->searchIds(query);
  ASSERT_OK(ids_result);
  ASSERT_EQ(ids_result->size(), 2);
  
  EXPECT_TRUE(std::find(ids_result->begin(), ids_result->end(), note1.id()) != ids_result->end());
  EXPECT_TRUE(std::find(ids_result->begin(), ids_result->end(), note3.id()) != ids_result->end());
}

TEST_F(SqliteIndexTest, SearchCount) {
  auto note1 = createTestNote("Note 1", "Test content");
  auto note2 = createTestNote("Note 2", "Different content");
  auto note3 = createTestNote("Note 3", "Test data");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  ASSERT_OK(index_->addNote(note3));
  
  SearchQuery query;
  query.text = "Test";
  
  auto count_result = index_->searchCount(query);
  ASSERT_OK(count_result);
  EXPECT_EQ(*count_result, 2);
  
  query.text = "content";
  count_result = index_->searchCount(query);
  ASSERT_OK(count_result);
  EXPECT_EQ(*count_result, 2);
}

TEST_F(SqliteIndexTest, UpdateNote) {
  auto note = createTestNote("Original Title", "Original content");
  
  ASSERT_OK(index_->addNote(note));
  
  // Search for original content
  SearchQuery query;
  query.text = "Original";
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 1);
  
  // Update the note
  note.setTitle("Updated Title");
  note.setContent("Updated content");
  note.touch(); // Update timestamp
  
  ASSERT_OK(index_->updateNote(note));
  
  // Search for updated content
  query.text = "Updated";
  search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 1);
  // Title is now derived from content
  EXPECT_EQ(search_result->front().title, "Updated content");
  
  // Original content should not be found
  query.text = "Original";
  search_result = index_->search(query);
  ASSERT_OK(search_result);
  EXPECT_EQ(search_result->size(), 0);
}

TEST_F(SqliteIndexTest, RemoveNote) {
  auto note1 = createTestNote("Note 1", "Content to keep");
  auto note2 = createTestNote("Note 2", "Content to remove");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  
  SearchQuery query;
  query.text = "Content";
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 2);
  
  // Remove one note
  ASSERT_OK(index_->removeNote(note2.id()));
  
  search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 1);
  EXPECT_EQ(search_result->front().id, note1.id());
}

TEST_F(SqliteIndexTest, TagSuggestions) {
  auto note1 = createTestNote("Note 1", "Content", {"programming", "cpp", "tutorial"});
  auto note2 = createTestNote("Note 2", "Content", {"programming", "python", "beginner"});
  auto note3 = createTestNote("Note 3", "Content", {"project", "planning"});
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  ASSERT_OK(index_->addNote(note3));
  
  auto suggestions_result = index_->suggestTags("pro", 10);
  ASSERT_OK(suggestions_result);
  
  EXPECT_TRUE(std::find(suggestions_result->begin(), suggestions_result->end(), "programming") != suggestions_result->end());
  EXPECT_TRUE(std::find(suggestions_result->begin(), suggestions_result->end(), "project") != suggestions_result->end());
}

TEST_F(SqliteIndexTest, NotebookSuggestions) {
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

TEST_F(SqliteIndexTest, GetStats) {
  auto note1 = createTestNote("Note 1", "Short content");
  auto note2 = createTestNote("Note 2", "This is a longer piece of content with more words");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  
  auto stats_result = index_->getStats();
  ASSERT_OK(stats_result);
  
  EXPECT_EQ(stats_result->total_notes, 2);
  EXPECT_GT(stats_result->total_words, 0);
  EXPECT_GT(stats_result->index_size_bytes, 0);
}

TEST_F(SqliteIndexTest, PaginationAndLimits) {
  // Add multiple notes
  for (int i = 0; i < 10; ++i) {
    auto note = createTestNote("Note " + std::to_string(i), "Test content number " + std::to_string(i));
    ASSERT_OK(index_->addNote(note));
  }
  
  SearchQuery query;
  query.text = "Test";
  query.limit = 5;
  query.offset = 0;
  
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 5);
  
  // Test pagination
  query.offset = 5;
  search_result = index_->search(query);
  ASSERT_OK(search_result);
  ASSERT_EQ(search_result->size(), 5);
}

TEST_F(SqliteIndexTest, TransactionHandling) {
  ASSERT_OK(index_->beginTransaction());
  
  auto note1 = createTestNote("Note 1", "Content 1");
  auto note2 = createTestNote("Note 2", "Content 2");
  
  ASSERT_OK(index_->addNote(note1));
  ASSERT_OK(index_->addNote(note2));
  
  ASSERT_OK(index_->commitTransaction());
  
  SearchQuery query;
  query.text = "Content";
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  EXPECT_EQ(search_result->size(), 2);
}

TEST_F(SqliteIndexTest, TransactionRollback) {
  auto note1 = createTestNote("Note 1", "Content 1");
  ASSERT_OK(index_->addNote(note1));
  
  ASSERT_OK(index_->beginTransaction());
  
  auto note2 = createTestNote("Note 2", "Content 2");
  ASSERT_OK(index_->addNote(note2));
  
  ASSERT_OK(index_->rollbackTransaction());
  
  SearchQuery query;
  query.text = "Content";
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  EXPECT_EQ(search_result->size(), 1); // Only note1 should exist
}

TEST_F(SqliteIndexTest, IndexValidation) {
  auto note = createTestNote("Test Note", "Test content");
  ASSERT_OK(index_->addNote(note));
  
  auto validation_result = index_->validateIndex();
  ASSERT_OK(validation_result);
}

TEST_F(SqliteIndexTest, OptimizeAndRebuild) {
  auto note = createTestNote("Test Note", "Test content");
  ASSERT_OK(index_->addNote(note));
  
  ASSERT_OK(index_->optimize());
  ASSERT_OK(index_->rebuild());
  
  // Verify search still works after optimization
  SearchQuery query;
  query.text = "Test";
  auto search_result = index_->search(query);
  ASSERT_OK(search_result);
  EXPECT_EQ(search_result->size(), 1);
}