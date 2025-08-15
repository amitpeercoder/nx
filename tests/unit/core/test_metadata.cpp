#include <gtest/gtest.h>

#include <thread>
#include <chrono>

#include "nx/core/metadata.hpp"
#include "test_helpers.hpp"

using namespace nx::core;
using namespace nx::test;
using nx::ErrorCode;

class MetadataTest : public ::testing::Test {
 protected:
  void SetUp() override {
    id_ = NoteId::generate();
    title_ = "Test Note";
  }

  NoteId id_;
  std::string title_;
};

TEST_F(MetadataTest, BasicConstruction) {
  Metadata metadata(id_, title_);
  
  EXPECT_EQ(metadata.id(), id_);
  EXPECT_EQ(metadata.title(), title_);
  EXPECT_FALSE(metadata.notebook().has_value());
  EXPECT_TRUE(metadata.tags().empty());
  EXPECT_TRUE(metadata.links().empty());
  EXPECT_TRUE(metadata.customFields().empty());
}

TEST_F(MetadataTest, SetAndGetTitle) {
  Metadata metadata(id_, title_);
  
  std::string new_title = "Updated Title";
  metadata.setTitle(new_title);
  
  EXPECT_EQ(metadata.title(), new_title);
}

TEST_F(MetadataTest, TagOperations) {
  Metadata metadata(id_, title_);
  
  // Add tags
  metadata.addTag("work");
  metadata.addTag("important");
  metadata.addTag("work");  // Duplicate should be ignored
  
  auto tags = metadata.tags();
  EXPECT_EQ(tags.size(), 2);
  EXPECT_TRUE(metadata.hasTag("work"));
  EXPECT_TRUE(metadata.hasTag("important"));
  EXPECT_FALSE(metadata.hasTag("personal"));
  
  // Remove tag
  metadata.removeTag("work");
  EXPECT_FALSE(metadata.hasTag("work"));
  EXPECT_TRUE(metadata.hasTag("important"));
  
  // Set tags
  std::vector<std::string> new_tags = {"tag1", "tag2", "tag3"};
  metadata.setTags(new_tags);
  
  auto final_tags = metadata.tags();
  EXPECT_EQ(final_tags.size(), 3);
  EXPECT_TRUE(metadata.hasTag("tag1"));
  EXPECT_TRUE(metadata.hasTag("tag2"));
  EXPECT_TRUE(metadata.hasTag("tag3"));
}

TEST_F(MetadataTest, NotebookOperations) {
  Metadata metadata(id_, title_);
  
  EXPECT_FALSE(metadata.notebook().has_value());
  
  metadata.setNotebook(std::string("work"));
  EXPECT_TRUE(metadata.notebook().has_value());
  EXPECT_EQ(*metadata.notebook(), "work");
  
  metadata.setNotebook(std::string(""));  // Empty string should clear notebook
  EXPECT_FALSE(metadata.notebook().has_value());
  
  metadata.setNotebook(std::make_optional<std::string>("personal"));
  EXPECT_TRUE(metadata.notebook().has_value());
  EXPECT_EQ(*metadata.notebook(), "personal");
  
  metadata.setNotebook(std::nullopt);
  EXPECT_FALSE(metadata.notebook().has_value());
}

TEST_F(MetadataTest, LinkOperations) {
  Metadata metadata(id_, title_);
  
  auto id1 = NoteId::generate();
  auto id2 = NoteId::generate();
  
  // Add links
  metadata.addLink(id1);
  metadata.addLink(id2);
  metadata.addLink(id1);  // Duplicate should be ignored
  
  auto links = metadata.links();
  EXPECT_EQ(links.size(), 2);
  EXPECT_TRUE(metadata.hasLink(id1));
  EXPECT_TRUE(metadata.hasLink(id2));
  
  // Remove link
  metadata.removeLink(id1);
  EXPECT_FALSE(metadata.hasLink(id1));
  EXPECT_TRUE(metadata.hasLink(id2));
  
  // Set links
  auto id3 = NoteId::generate();
  std::vector<NoteId> new_links = {id1, id3};
  metadata.setLinks(new_links);
  
  auto final_links = metadata.links();
  EXPECT_EQ(final_links.size(), 2);
  EXPECT_TRUE(metadata.hasLink(id1));
  EXPECT_TRUE(metadata.hasLink(id3));
  EXPECT_FALSE(metadata.hasLink(id2));
}

TEST_F(MetadataTest, CustomFields) {
  Metadata metadata(id_, title_);
  
  metadata.setCustomField("priority", "high");
  metadata.setCustomField("category", "technical");
  
  EXPECT_EQ(*metadata.getCustomField("priority"), "high");
  EXPECT_EQ(*metadata.getCustomField("category"), "technical");
  EXPECT_FALSE(metadata.getCustomField("nonexistent").has_value());
  
  metadata.removeCustomField("priority");
  EXPECT_FALSE(metadata.getCustomField("priority").has_value());
  EXPECT_EQ(*metadata.getCustomField("category"), "technical");
}

TEST_F(MetadataTest, Touch) {
  Metadata metadata(id_, title_);
  
  auto initial_updated = metadata.updated();
  
  // Small delay to ensure timestamp difference
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  
  metadata.touch();
  auto new_updated = metadata.updated();
  
  EXPECT_GT(new_updated, initial_updated);
}

TEST_F(MetadataTest, Validation) {
  Metadata metadata(id_, title_);
  
  // Valid metadata should pass
  EXPECT_OK(metadata.validate());
  
  // Empty title should fail
  metadata.setTitle("");
  EXPECT_ERROR(metadata.validate(), ErrorCode::kValidationError);
  
  // Reset to valid title
  metadata.setTitle("Valid Title");
  EXPECT_OK(metadata.validate());
  
  // Very long title should fail
  std::string long_title(300, 'a');
  metadata.setTitle(long_title);
  EXPECT_ERROR(metadata.validate(), ErrorCode::kValidationError);
  
  // Reset to valid title
  metadata.setTitle("Valid Title");
  
  // Tag with spaces should fail
  metadata.addTag("tag with spaces");
  EXPECT_ERROR(metadata.validate(), ErrorCode::kValidationError);
}

TEST_F(MetadataTest, YamlSerialization) {
  Metadata metadata(id_, title_);
  metadata.addTag("work");
  metadata.addTag("important");
  metadata.setNotebook(std::string("projects"));
  metadata.setCustomField("priority", "high");
  
  std::string yaml = metadata.toYaml();
  
  // Should contain key fields
  EXPECT_NE(yaml.find("id:"), std::string::npos);
  EXPECT_NE(yaml.find("title:"), std::string::npos);
  EXPECT_NE(yaml.find("created:"), std::string::npos);
  EXPECT_NE(yaml.find("updated:"), std::string::npos);
  EXPECT_NE(yaml.find("tags:"), std::string::npos);
  EXPECT_NE(yaml.find("notebook:"), std::string::npos);
  EXPECT_NE(yaml.find("priority:"), std::string::npos);
}

TEST_F(MetadataTest, YamlDeserialization) {
  std::string yaml = R"(
id: 01J8Y4N9W8K6W3K4T4S0S3QF4N
title: "Test Note"
created: 2024-01-15T10:30:00.000Z
updated: 2024-01-15T11:00:00.000Z
tags:
  - work
  - important
notebook: projects
priority: high
)";
  
  auto result = Metadata::fromYaml(yaml);
  ASSERT_OK(result);
  
  auto metadata = *result;
  EXPECT_EQ(metadata.title(), "Test Note");
  EXPECT_TRUE(metadata.hasTag("work"));
  EXPECT_TRUE(metadata.hasTag("important"));
  EXPECT_EQ(*metadata.notebook(), "projects");
  EXPECT_EQ(*metadata.getCustomField("priority"), "high");
}

TEST_F(MetadataTest, YamlRoundTrip) {
  Metadata original(id_, title_);
  original.addTag("work");
  original.addTag("personal");
  original.setNotebook(std::string("test"));
  original.setCustomField("status", "active");
  
  std::string yaml = original.toYaml();
  auto result = Metadata::fromYaml(yaml);
  
  ASSERT_OK(result);
  auto restored = *result;
  
  EXPECT_EQ(restored.id(), original.id());
  EXPECT_EQ(restored.title(), original.title());
  EXPECT_EQ(restored.tags(), original.tags());
  EXPECT_EQ(restored.notebook(), original.notebook());
  EXPECT_EQ(*restored.getCustomField("status"), *original.getCustomField("status"));
}