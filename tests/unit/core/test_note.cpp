#include <gtest/gtest.h>

#include <thread>
#include <chrono>

#include "nx/core/note.hpp"
#include "test_helpers.hpp"

using namespace nx::core;
using namespace nx::test;
using nx::ErrorCode;

class NoteTest : public ::testing::Test {};

TEST_F(NoteTest, CreateNote) {
  std::string title = "Test Note";
  std::string content = "This is test content.";
  
  auto note = Note::create(title, content);
  
  EXPECT_EQ(note.title(), title);
  EXPECT_EQ(note.content(), content);
  EXPECT_TRUE(note.id().isValid());
  EXPECT_TRUE(note.tags().empty());
  EXPECT_FALSE(note.notebook().has_value());
}

TEST_F(NoteTest, ContentOperations) {
  auto note = Note::create("Test", "Initial content");
  
  // Set content
  std::string new_content = "New content";
  note.setContent(new_content);
  EXPECT_EQ(note.content(), new_content);
  
  // Append content
  note.appendContent("Additional content");
  EXPECT_EQ(note.content(), "New content\nAdditional content");
  
  // Prepend content
  note.prependContent("Prepended content");
  EXPECT_EQ(note.content(), "Prepended content\nNew content\nAdditional content");
}

TEST_F(NoteTest, MetadataConvenience) {
  auto note = Note::create("Original Title", "Content");
  
  // Title operations
  note.setTitle("New Title");
  EXPECT_EQ(note.title(), "New Title");
  
  // Tag operations
  note.addTag("work");
  note.addTag("important");
  EXPECT_TRUE(note.metadata().hasTag("work"));
  EXPECT_TRUE(note.metadata().hasTag("important"));
  
  std::vector<std::string> new_tags = {"personal", "urgent"};
  note.setTags(new_tags);
  EXPECT_FALSE(note.metadata().hasTag("work"));
  EXPECT_TRUE(note.metadata().hasTag("personal"));
  EXPECT_TRUE(note.metadata().hasTag("urgent"));
  
  // Notebook operations
  note.setNotebook("projects");
  EXPECT_EQ(*note.notebook(), "projects");
}

TEST_F(NoteTest, FileFormatSerialization) {
  auto note = Note::create("Test Note", "This is the content of the note.\n\nWith multiple paragraphs.");
  note.addTag("test");
  note.addTag("example");
  note.setNotebook("samples");
  
  std::string file_format = note.toFileFormat();
  
  // Should start with YAML front-matter
  EXPECT_EQ(file_format.substr(0, 4), "---\n");
  EXPECT_NE(file_format.find("\n---\n"), std::string::npos);
  
  // Should contain metadata
  EXPECT_NE(file_format.find("id:"), std::string::npos);
  EXPECT_NE(file_format.find("title:"), std::string::npos);
  EXPECT_NE(file_format.find("tags:"), std::string::npos);
  EXPECT_NE(file_format.find("notebook:"), std::string::npos);
  
  // Should contain content
  EXPECT_NE(file_format.find("This is the content"), std::string::npos);
}

TEST_F(NoteTest, FileFormatDeserialization) {
  std::string file_content = R"(---
id: 01J8Y4N9W8K6W3K4T4S0S3QF4N
title: "Sample Note"
created: 2024-01-15T10:30:00.000Z
updated: 2024-01-15T11:00:00.000Z
tags:
  - example
  - test
notebook: samples
---

# Sample Note

This is the content of the note.

It has multiple paragraphs and some **markdown** formatting.

- List item 1
- List item 2
)";
  
  auto result = Note::fromFileFormat(file_content);
  ASSERT_OK(result);
  
  auto note = *result;
  EXPECT_EQ(note.title(), "Sample Note");
  EXPECT_TRUE(note.metadata().hasTag("example"));
  EXPECT_TRUE(note.metadata().hasTag("test"));
  EXPECT_EQ(*note.notebook(), "samples");
  EXPECT_NE(note.content().find("# Sample Note"), std::string::npos);
  EXPECT_NE(note.content().find("multiple paragraphs"), std::string::npos);
}

TEST_F(NoteTest, FileFormatRoundTrip) {
  auto original = Note::create("Round Trip Test", "Original content\n\nWith formatting.");
  original.addTag("roundtrip");
  original.setNotebook("test");
  
  std::string file_format = original.toFileFormat();
  auto result = Note::fromFileFormat(file_format);
  
  ASSERT_OK(result);
  auto restored = *result;
  
  EXPECT_EQ(restored.id(), original.id());
  EXPECT_EQ(restored.title(), original.title());
  EXPECT_EQ(restored.content(), original.content());
  EXPECT_EQ(restored.tags(), original.tags());
  EXPECT_EQ(restored.notebook(), original.notebook());
}

TEST_F(NoteTest, FilenameGeneration) {
  auto note = Note::create("Test Note Title", "Content");
  std::string filename = note.filename();
  
  // Should contain ULID
  EXPECT_NE(filename.find(note.id().toString()), std::string::npos);
  
  // Should contain slug
  EXPECT_NE(filename.find("test-note-title"), std::string::npos);
  
  // Should end with .md
  EXPECT_EQ(filename.substr(filename.length() - 3), ".md");
}

TEST_F(NoteTest, FilenameSpecialCharacters) {
  auto note = Note::create("Special!@# Characters & Spaces", "Content");
  std::string filename = note.filename();
  
  // Should contain sanitized slug
  EXPECT_NE(filename.find("special-characters-spaces"), std::string::npos);
  
  // Should not contain special characters
  EXPECT_EQ(filename.find("!"), std::string::npos);
  EXPECT_EQ(filename.find("@"), std::string::npos);
  EXPECT_EQ(filename.find("#"), std::string::npos);
  EXPECT_EQ(filename.find("&"), std::string::npos);
}

TEST_F(NoteTest, ContentLinkExtraction) {
  std::string content = R"(
This note links to [another note](01J8Y4N9W8K6W3K4T4S0S3QF4N) and
also references [a second note](01J8Y4N9W8K6W3K4T4S0S3QF4M).

Here's a regular link: [external](https://example.com)
And an invalid ULID: [bad link](invalid-ulid)
)";
  
  auto note = Note::create("Link Test", content);
  auto links = note.extractContentLinks();
  
  EXPECT_EQ(links.size(), 2);
  
  // Verify the extracted links are valid ULIDs
  for (const auto& link : links) {
    EXPECT_TRUE(link.isValid());
  }
}

TEST_F(NoteTest, UpdateLinksFromContent) {
  std::string content = R"(
Referenced notes:
- [Note A](01J8Y4N9W8K6W3K4T4S0S3QF4A)
- [Note B](01J8Y4N9W8K6W3K4T4S0S3QF4B)
)";
  
  auto note = Note::create("Reference Test", content);
  note.updateLinksFromContent();
  
  auto links = note.metadata().links();
  EXPECT_EQ(links.size(), 2);
}

TEST_F(NoteTest, TextSearch) {
  auto note = Note::create("Search Test", "This is some sample content for testing search functionality.");
  
  // Case sensitive search
  EXPECT_TRUE(note.containsText("sample", true));
  EXPECT_FALSE(note.containsText("SAMPLE", true));
  
  // Case insensitive search
  EXPECT_TRUE(note.containsText("SAMPLE", false));
  EXPECT_TRUE(note.containsText("Sample", false));
  EXPECT_TRUE(note.containsText("search", false));
  
  // Search in title
  EXPECT_TRUE(note.containsText("Search", true));
  EXPECT_TRUE(note.containsText("test", false));
}

TEST_F(NoteTest, TextPositions) {
  auto note = Note::create("Test", "test content test more test");
  
  auto positions = note.findTextPositions("test", true);
  EXPECT_EQ(positions.size(), 3);
  EXPECT_EQ(positions[0], 0);
  EXPECT_EQ(positions[1], 13);
  EXPECT_EQ(positions[2], 23);
  
  // Case insensitive
  auto positions_insensitive = note.findTextPositions("TEST", false);
  EXPECT_EQ(positions_insensitive.size(), 3);
}

TEST_F(NoteTest, Validation) {
  auto note = Note::create("Valid Note", "Valid content");
  EXPECT_OK(note.validate());
  
  // Invalid title should fail
  note.setTitle("");
  EXPECT_ERROR(note.validate(), ErrorCode::kValidationError);
  
  // Reset to valid
  note.setTitle("Valid Title");
  EXPECT_OK(note.validate());
  
  // Extremely large content should fail
  std::string huge_content(20 * 1024 * 1024, 'a');  // 20MB
  note.setContent(huge_content);
  EXPECT_ERROR(note.validate(), ErrorCode::kValidationError);
}

TEST_F(NoteTest, Touch) {
  auto note = Note::create("Touch Test", "Content");
  
  auto initial_updated = note.metadata().updated();
  
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  
  note.touch();
  auto new_updated = note.metadata().updated();
  
  EXPECT_GT(new_updated, initial_updated);
}