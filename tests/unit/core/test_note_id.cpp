#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "nx/core/note_id.hpp"
#include "test_helpers.hpp"

using namespace nx::core;
using namespace nx::test;
using nx::ErrorCode;

class NoteIdTest : public ::testing::Test {};

TEST_F(NoteIdTest, GenerateValidUlid) {
  auto id = NoteId::generate();
  
  EXPECT_TRUE(id.isValid());
  EXPECT_EQ(id.toString().length(), 26);
}

TEST_F(NoteIdTest, GenerateWithTimestamp) {
  auto timestamp = std::chrono::system_clock::now();
  auto id = NoteId::generate(timestamp);
  
  EXPECT_TRUE(id.isValid());
  
  // Timestamp should be approximately equal (within 1 second)
  auto extracted_timestamp = id.timestamp();
  auto diff = std::chrono::abs(timestamp - extracted_timestamp);
  EXPECT_LT(diff, std::chrono::seconds(1));
}

TEST_F(NoteIdTest, FromStringValid) {
  std::string ulid_str = "01J8Y4N9W8K6W3K4T4S0S3QF4N";
  auto result = NoteId::fromString(ulid_str);
  
  ASSERT_OK(result);
  EXPECT_EQ(result->toString(), ulid_str);
  EXPECT_TRUE(result->isValid());
}

TEST_F(NoteIdTest, FromStringInvalid) {
  // Too short
  EXPECT_ERROR(NoteId::fromString("short"), ErrorCode::kInvalidArgument);
  
  // Too long
  EXPECT_ERROR(NoteId::fromString("01J8Y4N9W8K6W3K4T4S0S3QF4NTOOLONG"), ErrorCode::kInvalidArgument);
  
  // Invalid characters
  EXPECT_ERROR(NoteId::fromString("01J8Y4N9W8K6W3K4T4S0S3QF4I"), ErrorCode::kInvalidArgument);  // I is invalid
  EXPECT_ERROR(NoteId::fromString("01J8Y4N9W8K6W3K4T4S0S3QF4L"), ErrorCode::kInvalidArgument);  // L is invalid
}

TEST_F(NoteIdTest, Comparison) {
  auto id1 = NoteId::generate();
  
  // Sleep to ensure different timestamp
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  
  auto id2 = NoteId::generate();
  
  EXPECT_EQ(id1, id1);
  EXPECT_NE(id1, id2);
  
  // Since ULIDs are sortable by time, id2 should be greater than id1
  // (generated later)
  EXPECT_LT(id1, id2);
}

TEST_F(NoteIdTest, Sortability) {
  std::vector<NoteId> ids;
  
  // Generate IDs with known timestamps
  auto base_time = std::chrono::system_clock::now();
  for (int i = 0; i < 5; ++i) {
    auto timestamp = base_time + std::chrono::milliseconds(i * 100);
    ids.push_back(NoteId::generate(timestamp));
  }
  
  // They should already be sorted
  for (size_t i = 1; i < ids.size(); ++i) {
    EXPECT_LT(ids[i-1], ids[i]);
  }
}

TEST_F(NoteIdTest, Hash) {
  auto id1 = NoteId::generate();
  auto id2 = NoteId::generate();
  
  std::hash<NoteId> hasher;
  
  // Same ID should have same hash
  EXPECT_EQ(hasher(id1), hasher(id1));
  
  // Different IDs should (very likely) have different hashes
  EXPECT_NE(hasher(id1), hasher(id2));
}

TEST_F(NoteIdTest, UnorderedMapUsage) {
  std::unordered_map<NoteId, std::string> map;
  
  auto id1 = NoteId::generate();
  auto id2 = NoteId::generate();
  
  map[id1] = "note1";
  map[id2] = "note2";
  
  EXPECT_EQ(map[id1], "note1");
  EXPECT_EQ(map[id2], "note2");
  EXPECT_EQ(map.size(), 2);
}