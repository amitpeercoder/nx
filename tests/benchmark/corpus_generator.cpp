#include "corpus_generator.hpp"

#include <algorithm>
#include <sstream>

namespace nx::test {

CorpusGenerator::CorpusGenerator() : CorpusGenerator(Config{}) {
}

CorpusGenerator::CorpusGenerator(Config config) 
    : config_(config), rng_(std::random_device{}()) {
  initializeTemplates();
}

std::vector<nx::core::Note> CorpusGenerator::generateCorpus() {
  std::vector<nx::core::Note> notes;
  notes.reserve(config_.note_count);
  
  // Generate all notes first
  for (size_t i = 0; i < config_.note_count; ++i) {
    notes.push_back(generateNote(i));
  }
  
  // Add random links between notes
  if (config_.link_probability > 0.0) {
    std::uniform_real_distribution<> link_dist(0.0, 1.0);
    std::uniform_int_distribution<size_t> note_dist(0, notes.size() - 1);
    std::uniform_int_distribution<size_t> link_count_dist(1, config_.max_links_per_note);
    
    for (auto& note : notes) {
      if (link_dist(rng_) < config_.link_probability) {
        size_t link_count = link_count_dist(rng_);
        std::vector<nx::core::NoteId> links;
        
        for (size_t i = 0; i < link_count; ++i) {
          size_t target_idx = note_dist(rng_);
          if (notes[target_idx].id() != note.id()) {
            links.push_back(notes[target_idx].id());
          }
        }
        
        // Remove duplicates
        std::sort(links.begin(), links.end());
        links.erase(std::unique(links.begin(), links.end()), links.end());
        
        auto metadata = note.metadata();
        metadata.setLinks(links);
        note.setMetadata(metadata);
      }
    }
  }
  
  return notes;
}

nx::core::Note CorpusGenerator::generateNote(size_t index) {
  std::string title = generateTitle();
  std::string content = generateContent(
      std::uniform_int_distribution<size_t>(
          config_.min_content_size, config_.max_content_size)(rng_));
  
  auto note = nx::core::Note::create(title, content);
  
  // Add tags
  auto tags = generateTags();
  note.setTags(tags);
  
  // Add notebook
  if (config_.notebook_count > 0) {
    note.setNotebook(generateNotebook());
  }
  
  return note;
}

std::string CorpusGenerator::generateContent(size_t target_size) {
  if (!config_.use_realistic_content) {
    // Generate lorem ipsum style content
    std::string content;
    content.reserve(target_size);
    
    std::vector<std::string> words = {
        "lorem", "ipsum", "dolor", "sit", "amet", "consectetur", "adipiscing",
        "elit", "sed", "do", "eiusmod", "tempor", "incididunt", "ut", "labore",
        "et", "dolore", "magna", "aliqua", "enim", "ad", "minim", "veniam",
        "quis", "nostrud", "exercitation", "ullamco", "laboris", "nisi",
        "aliquip", "ex", "ea", "commodo", "consequat"
    };
    
    std::uniform_int_distribution<size_t> word_dist(0, words.size() - 1);
    
    while (content.size() < target_size) {
      if (!content.empty() && content.back() != ' ') {
        content += " ";
      }
      content += words[word_dist(rng_)];
      
      // Add occasional newlines
      if (std::uniform_real_distribution<>(0.0, 1.0)(rng_) < 0.1) {
        content += "\n\n";
      }
    }
    
    return content.substr(0, target_size);
  }
  
  // Use realistic content templates
  std::uniform_int_distribution<size_t> template_dist(0, content_templates_.size() - 1);
  std::string content = expandTemplate(content_templates_[template_dist(rng_)]);
  
  // Extend or trim to target size
  while (content.size() < target_size) {
    content += "\n\n";
    content += expandTemplate(content_templates_[template_dist(rng_)]);
  }
  
  return content.substr(0, target_size);
}

std::string CorpusGenerator::generateTitle() {
  std::uniform_int_distribution<size_t> template_dist(0, title_templates_.size() - 1);
  return expandTemplate(title_templates_[template_dist(rng_)]);
}

std::vector<std::string> CorpusGenerator::generateTags() {
  std::uniform_int_distribution<size_t> count_dist(0, config_.max_tags_per_note);
  size_t tag_count = std::min(count_dist(rng_), tag_pool_.size());
  
  if (tag_count == 0) {
    return {};
  }
  
  std::vector<std::string> available_tags = tag_pool_;
  std::shuffle(available_tags.begin(), available_tags.end(), rng_);
  
  std::vector<std::string> tags;
  tags.reserve(tag_count);
  
  for (size_t i = 0; i < tag_count; ++i) {
    tags.push_back(available_tags[i]);
  }
  
  return tags;
}

std::string CorpusGenerator::generateNotebook() {
  if (notebook_names_.empty()) {
    return "default";
  }
  
  std::uniform_int_distribution<size_t> dist(0, notebook_names_.size() - 1);
  return notebook_names_[dist(rng_)];
}

void CorpusGenerator::initializeTemplates() {
  title_templates_ = {
      "Meeting Notes: {PROJECT} Planning",
      "Weekly Review - {DATE}",
      "Technical Design: {FEATURE}",
      "Research Notes on {TOPIC}",
      "Project Update: {PROJECT}",
      "Bug Investigation: {ISSUE}",
      "Learning Notes: {TECHNOLOGY}",
      "Architecture Decision: {DECISION}",
      "Code Review: {COMPONENT}",
      "Performance Analysis: {SYSTEM}",
      "User Feedback: {FEATURE}",
      "Sprint Retrospective {DATE}",
      "Design Proposal: {FEATURE}",
      "Technical Debt: {AREA}",
      "Deployment Notes: {VERSION}",
      "Incident Report: {DATE}",
      "Feature Specification: {FEATURE}",
      "Team Meeting {DATE}",
      "Customer Interview: {CUSTOMER}",
      "Competitive Analysis: {COMPETITOR}"
  };
  
  content_templates_ = {
      "## Overview\n\nThis document outlines {TOPIC} and provides analysis of current implementation.\n\n## Key Points\n\n- Performance metrics show {METRIC}\n- Implementation requires {REQUIREMENT}\n- Timeline estimated at {TIMELINE}\n\n## Next Steps\n\n1. Review current approach\n2. Implement proposed changes\n3. Validate results",
      
      "# Problem Statement\n\n{PROBLEM} has been identified as a critical issue affecting {SYSTEM}.\n\n## Analysis\n\nRoot cause appears to be related to {CAUSE}. Investigation shows:\n\n- Symptom A: {SYMPTOM}\n- Symptom B: {SYMPTOM}\n- Impact: {IMPACT}\n\n## Proposed Solution\n\n{SOLUTION} should address the core issues while maintaining backward compatibility.",
      
      "## Meeting Attendees\n\n- {PERSON} (Lead)\n- {PERSON} (Engineer)\n- {PERSON} (Designer)\n\n## Agenda\n\n1. Review current progress\n2. Discuss blockers\n3. Plan next iteration\n\n## Decisions\n\n- Agreed to prioritize {FEATURE}\n- Will implement {APPROACH}\n- Timeline: {TIMELINE}\n\n## Action Items\n\n- [ ] {PERSON}: Implement {TASK}\n- [ ] {PERSON}: Review {DOCUMENT}\n- [ ] {PERSON}: Test {FEATURE}",
      
      "# Technical Specification\n\n## Requirements\n\n{FEATURE} must support the following capabilities:\n\n1. {REQUIREMENT_1}\n2. {REQUIREMENT_2}\n3. {REQUIREMENT_3}\n\n## Design\n\n### Architecture\n\nThe system will use {PATTERN} pattern with {TECHNOLOGY} as the primary implementation.\n\n### API Design\n\n```\n{API_EXAMPLE}\n```\n\n## Implementation Plan\n\nPhase 1: Core functionality\nPhase 2: Performance optimization\nPhase 3: Advanced features"
  };
  
  tag_pool_ = {
      "important", "urgent", "work", "personal", "meeting", "technical",
      "design", "bug", "feature", "performance", "security", "documentation",
      "review", "planning", "research", "learning", "architecture", "deployment",
      "testing", "monitoring", "customer", "feedback", "retrospective", "sprint",
      "roadmap", "technical-debt", "refactoring", "optimization", "scaling",
      "integration", "api", "database", "frontend", "backend", "mobile",
      "web", "infrastructure", "devops", "ci-cd", "automation"
  };
  
  notebook_names_ = {
      "work", "personal", "projects", "meetings", "research", "learning",
      "technical", "design", "planning", "reviews"
  };
}

std::string CorpusGenerator::expandTemplate(const std::string& template_str) {
  std::string result = template_str;
  
  // Simple template variable replacement
  std::vector<std::pair<std::string, std::vector<std::string>>> replacements = {
      {"{PROJECT}", {"Alpha", "Beta", "Gamma", "Phoenix", "Mercury", "Atlas"}},
      {"{FEATURE}", {"Authentication", "Search", "Dashboard", "Analytics", "Reporting", "Cache"}},
      {"{TECHNOLOGY}", {"Kubernetes", "React", "PostgreSQL", "Redis", "Docker", "GraphQL"}},
      {"{TOPIC}", {"performance optimization", "system architecture", "user experience", "data modeling"}},
      {"{DATE}", {"2024-01-15", "2024-02-20", "2024-03-10", "2024-04-05"}},
      {"{PERSON}", {"Alice", "Bob", "Charlie", "Diana", "Eve", "Frank"}},
      {"{SYSTEM}", {"payment service", "user management", "notification system", "data pipeline"}},
      {"{METRIC}", {"95th percentile under 200ms", "throughput of 1000 RPS", "memory usage under 100MB"}},
      {"{TIMELINE}", {"2 weeks", "1 month", "Q2 2024", "end of sprint"}}
  };
  
  for (const auto& [placeholder, options] : replacements) {
    size_t pos = result.find(placeholder);
    if (pos != std::string::npos) {
      std::uniform_int_distribution<size_t> dist(0, options.size() - 1);
      result.replace(pos, placeholder.length(), options[dist(rng_)]);
    }
  }
  
  return result;
}

// Specialized generators
TechnicalCorpusGenerator::TechnicalCorpusGenerator(size_t count) : CorpusGenerator({
  .note_count = count,
  .min_content_size = 500,
  .max_content_size = 3000,
  .avg_tags_per_note = 4,
  .max_tags_per_note = 8,
  .notebook_count = 5,
  .link_probability = 0.2,
  .max_links_per_note = 3,
  .use_realistic_content = true
}) {}

PersonalCorpusGenerator::PersonalCorpusGenerator(size_t count) : CorpusGenerator({
  .note_count = count,
  .min_content_size = 100,
  .max_content_size = 1000,
  .avg_tags_per_note = 2,
  .max_tags_per_note = 5,
  .notebook_count = 3,
  .link_probability = 0.1,
  .max_links_per_note = 2,
  .use_realistic_content = true
}) {}

MeetingNotesGenerator::MeetingNotesGenerator(size_t count) : CorpusGenerator({
  .note_count = count,
  .min_content_size = 800,
  .max_content_size = 2500,
  .avg_tags_per_note = 3,
  .max_tags_per_note = 6,
  .notebook_count = 2,
  .link_probability = 0.15,
  .max_links_per_note = 4,
  .use_realistic_content = true
}) {}

}  // namespace nx::test