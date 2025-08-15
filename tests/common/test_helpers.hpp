#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "nx/core/note.hpp"
#include "nx/core/note_id.hpp"

namespace nx::test {

// Test fixture base class for tests that need temporary directories
class TempDirTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  std::filesystem::path temp_dir_;
};

// Create a test note with specified content
nx::core::Note createTestNote(const std::string& title, const std::string& content = "",
                              const std::vector<std::string>& tags = {});

// Create multiple test notes for corpus testing
std::vector<nx::core::Note> createTestCorpus(size_t count);

// Compare notes (for testing equality)
bool notesEqual(const nx::core::Note& a, const nx::core::Note& b);

// Generate random string for testing
std::string randomString(size_t length);

// Generate random ULID-like string
std::string randomUlid();

// Test data directory
std::filesystem::path testDataDir();

// Load test file content
std::string loadTestFile(const std::string& filename);

// Assertion helpers
#define EXPECT_OK(result)                                                                          \
  do {                                                                                             \
    auto&& r = (result);                                                                           \
    EXPECT_TRUE(r.has_value()) << "Expected success but got error: " << r.error().message();     \
  } while (0)

#define ASSERT_OK(result)                                                                          \
  do {                                                                                             \
    auto&& r = (result);                                                                           \
    ASSERT_TRUE(r.has_value()) << "Expected success but got error: " << r.error().message();     \
  } while (0)

#define EXPECT_ERROR(result, expected_code)                                                        \
  do {                                                                                             \
    auto&& r = (result);                                                                           \
    EXPECT_FALSE(r.has_value()) << "Expected error but got success";                              \
    if (!r.has_value()) {                                                                          \
      EXPECT_EQ(r.error().code(), expected_code);                                                  \
    }                                                                                              \
  } while (0)

}  // namespace nx::test