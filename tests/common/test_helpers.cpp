#include "test_helpers.hpp"

#include <random>
#include <fstream>

namespace nx::test {

void TempDirTest::SetUp() {
  temp_dir_ = std::filesystem::temp_directory_path() / "nx_test";
  temp_dir_ /= randomString(8);
  std::filesystem::create_directories(temp_dir_);
}

void TempDirTest::TearDown() {
  if (std::filesystem::exists(temp_dir_)) {
    std::filesystem::remove_all(temp_dir_);
  }
}

nx::core::Note createTestNote(const std::string& title, const std::string& content,
                              const std::vector<std::string>& tags) {
  auto note = nx::core::Note::create(title, content);
  if (!tags.empty()) {
    note.setTags(tags);
  }
  return note;
}

std::vector<nx::core::Note> createTestCorpus(size_t count) {
  std::vector<nx::core::Note> notes;
  notes.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    std::string title = "Test Note " + std::to_string(i + 1);
    std::string content = "This is test content for note " + std::to_string(i + 1) + ".\n";
    content += "It contains some sample text to test search functionality.\n";
    
    std::vector<std::string> tags;
    if (i % 3 == 0) tags.push_back("important");
    if (i % 5 == 0) tags.push_back("work");
    if (i % 7 == 0) tags.push_back("personal");
    
    notes.push_back(createTestNote(title, content, tags));
  }

  return notes;
}

bool notesEqual(const nx::core::Note& a, const nx::core::Note& b) {
  return a.id() == b.id() && 
         a.title() == b.title() && 
         a.content() == b.content() &&
         a.tags() == b.tags() &&
         a.notebook() == b.notebook();
}

std::string randomString(size_t length) {
  static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
  
  std::string result;
  result.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    result += charset[dis(gen)];
  }
  return result;
}

std::string randomUlid() {
  // Generate a ULID-like string (26 characters, base32)
  static const char base32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 31);
  
  std::string result;
  result.reserve(26);
  for (int i = 0; i < 26; ++i) {
    result += base32[dis(gen)];
  }
  return result;
}

std::filesystem::path testDataDir() {
  // Assuming this is called from build directory
  return std::filesystem::current_path() / "tests" / "data";
}

std::string loadTestFile(const std::string& filename) {
  std::filesystem::path path = testDataDir() / filename;
  std::ifstream file(path);
  if (!file) {
    return "";
  }
  
  std::string content;
  std::string line;
  while (std::getline(file, line)) {
    content += line + "\n";
  }
  
  return content;
}

}  // namespace nx::test