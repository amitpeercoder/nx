#pragma once

#include <string>
#include <vector>
#include <random>

#include "nx/core/note.hpp"

namespace nx::test {

// Synthetic corpus generator for performance testing
class CorpusGenerator {
 public:
  struct Config {
    size_t note_count = 1000;
    size_t min_content_size = 100;
    size_t max_content_size = 2000;
    size_t avg_tags_per_note = 3;
    size_t max_tags_per_note = 8;
    size_t notebook_count = 10;
    double link_probability = 0.15;  // Probability of linking to another note
    size_t max_links_per_note = 5;
    bool use_realistic_content = true;
  };

  CorpusGenerator();
  explicit CorpusGenerator(Config config);

  // Generate corpus of notes
  std::vector<nx::core::Note> generateCorpus();

  // Generate single note with realistic content
  nx::core::Note generateNote(size_t index = 0);

  // Generate realistic content for notes
  std::string generateContent(size_t target_size);

  // Generate realistic title
  std::string generateTitle();

  // Generate tags from predefined set
  std::vector<std::string> generateTags();

  // Generate notebook name
  std::string generateNotebook();

 private:
  Config config_;
  mutable std::mt19937 rng_;
  
  // Predefined content templates for realistic notes
  std::vector<std::string> title_templates_;
  std::vector<std::string> content_templates_;
  std::vector<std::string> tag_pool_;
  std::vector<std::string> notebook_names_;
  
  void initializeTemplates();
  std::string expandTemplate(const std::string& template_str);
};

// Specialized generators for different use cases
class TechnicalCorpusGenerator : public CorpusGenerator {
 public:
  TechnicalCorpusGenerator(size_t count);
};

class PersonalCorpusGenerator : public CorpusGenerator {
 public:
  PersonalCorpusGenerator(size_t count);
};

class MeetingNotesGenerator : public CorpusGenerator {
 public:
  MeetingNotesGenerator(size_t count);
};

}  // namespace nx::test