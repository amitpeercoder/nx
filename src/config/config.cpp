#include "nx/config/config.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>

#include <toml++/toml.hpp>

#include "nx/util/xdg.hpp"
#include "nx/util/filesystem.hpp"

namespace nx::config {

Config::Config() {
  // Initialize defaults directly
  root = nx::util::Xdg::dataHome();
  data_dir = nx::util::Xdg::dataHome();
  notes_dir = nx::util::Xdg::notesDir();
  attachments_dir = nx::util::Xdg::attachmentsDir();
  trash_dir = nx::util::Xdg::trashDir();
  index_file = nx::util::Xdg::indexFile();
  
  // Set default editor
  editor = std::getenv("VISUAL") ? std::getenv("VISUAL") :
           (std::getenv("EDITOR") ? std::getenv("EDITOR") : "vi");
  
  // Set default values
  indexer = IndexerType::kFts;
  encryption = EncryptionType::kNone;
  sync = SyncType::kNone;
  
  // Try to load from default location
  auto default_path = defaultConfigPath();
  if (std::filesystem::exists(default_path)) {
    auto result = load(default_path);
    // If loading fails, silently continue with defaults
    (void)result;  // Suppress unused variable warning
  }
}

Config::Config(const std::filesystem::path& config_path) {
  // Initialize defaults directly instead of calling createDefault()
  root = nx::util::Xdg::dataHome();
  data_dir = nx::util::Xdg::dataHome();
  notes_dir = nx::util::Xdg::notesDir();
  attachments_dir = nx::util::Xdg::attachmentsDir();
  trash_dir = nx::util::Xdg::trashDir();
  index_file = nx::util::Xdg::indexFile();
  
  // Set default editor
  editor = std::getenv("VISUAL") ? std::getenv("VISUAL") :
           (std::getenv("EDITOR") ? std::getenv("EDITOR") : "vi");
  
  // Set default values
  indexer = IndexerType::kFts;
  encryption = EncryptionType::kNone;
  sync = SyncType::kNone;
  
  auto result = load(config_path);
  if (!result.has_value()) {
    // Config loading failed, continue with defaults
    // This allows the application to function even with missing/invalid config
  }
}

Result<void> Config::load(const std::filesystem::path& config_path) {
  config_path_ = config_path;
  
  if (!std::filesystem::exists(config_path)) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "Config file not found: " + config_path.string()));
  }
  
  try {
    auto config_data = toml::parse_file(config_path.string());
    
    // Core paths
    if (auto value = config_data["root"].value<std::string>()) {
      root = *value;
    }
    if (auto value = config_data["data_dir"].value<std::string>()) {
      data_dir = *value;
    }
    if (auto value = config_data["notes_dir"].value<std::string>()) {
      notes_dir = *value;
    }
    if (auto value = config_data["attachments_dir"].value<std::string>()) {
      attachments_dir = *value;
    }
    if (auto value = config_data["trash_dir"].value<std::string>()) {
      trash_dir = *value;
    }
    if (auto value = config_data["index_file"].value<std::string>()) {
      index_file = *value;
    }
    
    // Editor
    if (auto value = config_data["editor"].value<std::string>()) {
      editor = *value;
    }
    
    // Indexer
    if (auto value = config_data["indexer"].value<std::string>()) {
      indexer = stringToIndexerType(*value);
    }
    
    // Encryption
    if (auto value = config_data["encryption"].value<std::string>()) {
      encryption = stringToEncryptionType(*value);
    }
    if (auto value = config_data["age_recipient"].value<std::string>()) {
      age_recipient = resolveEnvVar(*value);
    }
    
    // Sync
    if (auto value = config_data["sync"].value<std::string>()) {
      sync = stringToSyncType(*value);
    }
    if (auto value = config_data["git_remote"].value<std::string>()) {
      git_remote = *value;
    }
    if (auto value = config_data["git_user_name"].value<std::string>()) {
      git_user_name = *value;
    }
    if (auto value = config_data["git_user_email"].value<std::string>()) {
      git_user_email = *value;
    }
    
    // Auto-sync configuration
    if (auto sync_table = config_data["auto_sync"].as_table()) {
      if (auto value = (*sync_table)["enabled"].value<bool>()) {
        auto_sync.enabled = *value;
      }
      if (auto value = (*sync_table)["auto_pull_on_startup"].value<bool>()) {
        auto_sync.auto_pull_on_startup = *value;
      }
      if (auto value = (*sync_table)["auto_push_on_changes"].value<bool>()) {
        auto_sync.auto_push_on_changes = *value;
      }
      if (auto value = (*sync_table)["auto_push_delay_seconds"].value<int64_t>()) {
        auto_sync.auto_push_delay_seconds = static_cast<int>(*value);
      }
      if (auto value = (*sync_table)["sync_interval_seconds"].value<int64_t>()) {
        auto_sync.sync_interval_seconds = static_cast<int>(*value);
      }
      if (auto value = (*sync_table)["conflict_strategy"].value<std::string>()) {
        auto_sync.conflict_strategy = *value;
      }
      if (auto value = (*sync_table)["max_auto_resolve_attempts"].value<int64_t>()) {
        auto_sync.max_auto_resolve_attempts = static_cast<int>(*value);
      }
      if (auto value = (*sync_table)["sync_on_shutdown"].value<bool>()) {
        auto_sync.sync_on_shutdown = *value;
      }
      if (auto value = (*sync_table)["show_sync_status"].value<bool>()) {
        auto_sync.show_sync_status = *value;
      }
    }
    
    // Defaults
    if (auto value = config_data["defaults"]["notebook"].value<std::string>()) {
      default_notebook = *value;
    }
    if (auto tags_array = config_data["defaults"]["tags"].as_array()) {
      default_tags.clear();
      for (const auto& tag : *tags_array) {
        if (auto tag_str = tag.value<std::string>()) {
          default_tags.push_back(*tag_str);
        }
      }
    }
    
    // AI configuration
    if (auto ai_table = config_data["ai"].as_table()) {
      AiConfig ai_config;
      
      if (auto value = (*ai_table)["provider"].value<std::string>()) {
        ai_config.provider = *value;
      }
      if (auto value = (*ai_table)["model"].value<std::string>()) {
        ai_config.model = *value;
      }
      if (auto value = (*ai_table)["api_key"].value<std::string>()) {
        ai_config.api_key = resolveEnvVar(*value);
      }
      if (auto value = (*ai_table)["max_tokens"].value<int>()) {
        ai_config.max_tokens = *value;
      }
      if (auto value = (*ai_table)["temperature"].value<double>()) {
        ai_config.temperature = *value;
      }
      if (auto value = (*ai_table)["rate_limit_qpm"].value<int>()) {
        ai_config.rate_limit_qpm = *value;
      }
      if (auto value = (*ai_table)["daily_usd_budget"].value<double>()) {
        ai_config.daily_usd_budget = *value;
      }
      if (auto value = (*ai_table)["enable_embeddings"].value<bool>()) {
        ai_config.enable_embeddings = *value;
      }
      if (auto value = (*ai_table)["embedding_model"].value<std::string>()) {
        ai_config.embedding_model = *value;
      }
      if (auto value = (*ai_table)["top_k"].value<int>()) {
        ai_config.top_k = *value;
      }
      
      // Redaction settings
      if (auto redaction_table = (*ai_table)["redaction"].as_table()) {
        if (auto value = (*redaction_table)["strip_emails"].value<bool>()) {
          ai_config.strip_emails = *value;
        }
        if (auto value = (*redaction_table)["strip_urls"].value<bool>()) {
          ai_config.strip_urls = *value;
        }
        if (auto value = (*redaction_table)["mask_numbers"].value<bool>()) {
          ai_config.mask_numbers = *value;
        }
      }
      
      // AI Explanations settings
      if (auto explanations_table = (*ai_table)["explanations"].as_table()) {
        if (auto value = (*explanations_table)["enabled"].value<bool>()) {
          ai_config.explanations.enabled = *value;
        }
        if (auto value = (*explanations_table)["brief_max_words"].value<int>()) {
          ai_config.explanations.brief_max_words = static_cast<size_t>(*value);
        }
        if (auto value = (*explanations_table)["expanded_max_words"].value<int>()) {
          ai_config.explanations.expanded_max_words = static_cast<size_t>(*value);
        }
        if (auto value = (*explanations_table)["timeout_ms"].value<int>()) {
          ai_config.explanations.timeout_ms = *value;
        }
        if (auto value = (*explanations_table)["cache_explanations"].value<bool>()) {
          ai_config.explanations.cache_explanations = *value;
        }
        if (auto value = (*explanations_table)["max_cache_size"].value<int>()) {
          ai_config.explanations.max_cache_size = static_cast<size_t>(*value);
        }
        if (auto value = (*explanations_table)["context_radius"].value<int>()) {
          ai_config.explanations.context_radius = static_cast<size_t>(*value);
        }
      }
      
      // AI Smart Completion settings
      if (auto completion_table = (*ai_table)["smart_completion"].as_table()) {
        if (auto value = (*completion_table)["enabled"].value<bool>()) {
          ai_config.smart_completion.enabled = *value;
        }
        if (auto value = (*completion_table)["max_tokens"].value<int>()) {
          ai_config.smart_completion.max_tokens = *value;
        }
        if (auto value = (*completion_table)["temperature"].value<double>()) {
          ai_config.smart_completion.temperature = *value;
        }
        if (auto value = (*completion_table)["max_completion_length"].value<int>()) {
          ai_config.smart_completion.max_completion_length = static_cast<size_t>(*value);
        }
        if (auto value = (*completion_table)["timeout_ms"].value<int>()) {
          ai_config.smart_completion.timeout_ms = *value;
        }
      }
      
      // AI Semantic Search settings
      if (auto semantic_search_table = (*ai_table)["semantic_search"].as_table()) {
        if (auto value = (*semantic_search_table)["enabled"].value<bool>()) {
          ai_config.semantic_search.enabled = *value;
        }
        if (auto value = (*semantic_search_table)["max_tokens"].value<int>()) {
          ai_config.semantic_search.max_tokens = *value;
        }
        if (auto value = (*semantic_search_table)["temperature"].value<double>()) {
          ai_config.semantic_search.temperature = *value;
        }
        if (auto value = (*semantic_search_table)["timeout_ms"].value<int>()) {
          ai_config.semantic_search.timeout_ms = *value;
        }
        if (auto value = (*semantic_search_table)["max_notes_per_query"].value<int>()) {
          ai_config.semantic_search.max_notes_per_query = static_cast<size_t>(*value);
        }
      }
      
      // AI Grammar & Style Check settings
      if (auto grammar_style_table = (*ai_table)["grammar_style_check"].as_table()) {
        if (auto value = (*grammar_style_table)["enabled"].value<bool>()) {
          ai_config.grammar_style_check.enabled = *value;
        }
        if (auto value = (*grammar_style_table)["max_tokens"].value<int>()) {
          ai_config.grammar_style_check.max_tokens = *value;
        }
        if (auto value = (*grammar_style_table)["temperature"].value<double>()) {
          ai_config.grammar_style_check.temperature = *value;
        }
        if (auto value = (*grammar_style_table)["timeout_ms"].value<int>()) {
          ai_config.grammar_style_check.timeout_ms = *value;
        }
        if (auto value = (*grammar_style_table)["max_text_length"].value<int>()) {
          ai_config.grammar_style_check.max_text_length = static_cast<size_t>(*value);
        }
        if (auto value = (*grammar_style_table)["style"].value<std::string>()) {
          ai_config.grammar_style_check.style = *value;
        }
      }
      
      // AI Smart Examples settings
      if (auto smart_examples_table = (*ai_table)["smart_examples"].as_table()) {
        if (auto value = (*smart_examples_table)["enabled"].value<bool>()) {
          ai_config.smart_examples.enabled = *value;
        }
        if (auto value = (*smart_examples_table)["max_tokens"].value<int>()) {
          ai_config.smart_examples.max_tokens = *value;
        }
        if (auto value = (*smart_examples_table)["temperature"].value<double>()) {
          ai_config.smart_examples.temperature = *value;
        }
        if (auto value = (*smart_examples_table)["timeout_ms"].value<int>()) {
          ai_config.smart_examples.timeout_ms = *value;
        }
        if (auto value = (*smart_examples_table)["max_examples"].value<int>()) {
          ai_config.smart_examples.max_examples = static_cast<size_t>(*value);
        }
        if (auto value = (*smart_examples_table)["example_type"].value<std::string>()) {
          ai_config.smart_examples.example_type = *value;
        }
      }
      
      // AI Code Generation settings
      if (auto code_generation_table = (*ai_table)["code_generation"].as_table()) {
        if (auto value = (*code_generation_table)["enabled"].value<bool>()) {
          ai_config.code_generation.enabled = *value;
        }
        if (auto value = (*code_generation_table)["max_tokens"].value<int>()) {
          ai_config.code_generation.max_tokens = *value;
        }
        if (auto value = (*code_generation_table)["temperature"].value<double>()) {
          ai_config.code_generation.temperature = *value;
        }
        if (auto value = (*code_generation_table)["timeout_ms"].value<int>()) {
          ai_config.code_generation.timeout_ms = *value;
        }
        if (auto value = (*code_generation_table)["language"].value<std::string>()) {
          ai_config.code_generation.language = *value;
        }
        if (auto value = (*code_generation_table)["style"].value<std::string>()) {
          ai_config.code_generation.style = *value;
        }
      }
      
      // AI Smart Summarization settings
      if (auto smart_summarization_table = (*ai_table)["smart_summarization"].as_table()) {
        if (auto value = (*smart_summarization_table)["enabled"].value<bool>()) {
          ai_config.smart_summarization.enabled = *value;
        }
        if (auto value = (*smart_summarization_table)["max_tokens"].value<int>()) {
          ai_config.smart_summarization.max_tokens = *value;
        }
        if (auto value = (*smart_summarization_table)["temperature"].value<double>()) {
          ai_config.smart_summarization.temperature = *value;
        }
        if (auto value = (*smart_summarization_table)["timeout_ms"].value<int>()) {
          ai_config.smart_summarization.timeout_ms = *value;
        }
        if (auto value = (*smart_summarization_table)["max_text_length"].value<int>()) {
          ai_config.smart_summarization.max_text_length = static_cast<size_t>(*value);
        }
        if (auto value = (*smart_summarization_table)["style"].value<std::string>()) {
          ai_config.smart_summarization.style = *value;
        }
        if (auto value = (*smart_summarization_table)["include_metadata"].value<bool>()) {
          ai_config.smart_summarization.include_metadata = *value;
        }
      }
      
      // AI Note Relationships settings
      if (auto note_relationships_table = (*ai_table)["note_relationships"].as_table()) {
        if (auto value = (*note_relationships_table)["enabled"].value<bool>()) {
          ai_config.note_relationships.enabled = *value;
        }
        if (auto value = (*note_relationships_table)["max_tokens"].value<int>()) {
          ai_config.note_relationships.max_tokens = *value;
        }
        if (auto value = (*note_relationships_table)["temperature"].value<double>()) {
          ai_config.note_relationships.temperature = *value;
        }
        if (auto value = (*note_relationships_table)["timeout_ms"].value<int>()) {
          ai_config.note_relationships.timeout_ms = *value;
        }
        if (auto value = (*note_relationships_table)["max_notes_to_analyze"].value<int>()) {
          ai_config.note_relationships.max_notes_to_analyze = static_cast<size_t>(*value);
        }
        if (auto value = (*note_relationships_table)["similarity_threshold"].value<double>()) {
          ai_config.note_relationships.similarity_threshold = *value;
        }
      }
      
      // AI Smart Organization settings
      if (auto smart_organization_table = (*ai_table)["smart_organization"].as_table()) {
        if (auto value = (*smart_organization_table)["enabled"].value<bool>()) {
          ai_config.smart_organization.enabled = *value;
        }
        if (auto value = (*smart_organization_table)["max_tokens"].value<int>()) {
          ai_config.smart_organization.max_tokens = *value;
        }
        if (auto value = (*smart_organization_table)["temperature"].value<double>()) {
          ai_config.smart_organization.temperature = *value;
        }
        if (auto value = (*smart_organization_table)["timeout_ms"].value<int>()) {
          ai_config.smart_organization.timeout_ms = *value;
        }
        if (auto value = (*smart_organization_table)["max_notes_per_batch"].value<int>()) {
          ai_config.smart_organization.max_notes_per_batch = static_cast<size_t>(*value);
        }
        if (auto value = (*smart_organization_table)["suggest_new_notebooks"].value<bool>()) {
          ai_config.smart_organization.suggest_new_notebooks = *value;
        }
      }
      
      // AI Content Enhancement settings
      if (auto content_enhancement_table = (*ai_table)["content_enhancement"].as_table()) {
        if (auto value = (*content_enhancement_table)["enabled"].value<bool>()) {
          ai_config.content_enhancement.enabled = *value;
        }
        if (auto value = (*content_enhancement_table)["max_tokens"].value<int>()) {
          ai_config.content_enhancement.max_tokens = *value;
        }
        if (auto value = (*content_enhancement_table)["temperature"].value<double>()) {
          ai_config.content_enhancement.temperature = *value;
        }
        if (auto value = (*content_enhancement_table)["timeout_ms"].value<int>()) {
          ai_config.content_enhancement.timeout_ms = *value;
        }
        if (auto value = (*content_enhancement_table)["max_text_length"].value<int>()) {
          ai_config.content_enhancement.max_text_length = static_cast<size_t>(*value);
        }
        if (auto value = (*content_enhancement_table)["enhancement_focus"].value<std::string>()) {
          ai_config.content_enhancement.enhancement_focus = *value;
        }
      }
      
      // AI Research Assistant settings
      if (auto research_assistant_table = (*ai_table)["research_assistant"].as_table()) {
        if (auto value = (*research_assistant_table)["enabled"].value<bool>()) {
          ai_config.research_assistant.enabled = *value;
        }
        if (auto value = (*research_assistant_table)["max_tokens"].value<int>()) {
          ai_config.research_assistant.max_tokens = *value;
        }
        if (auto value = (*research_assistant_table)["temperature"].value<double>()) {
          ai_config.research_assistant.temperature = *value;
        }
        if (auto value = (*research_assistant_table)["timeout_ms"].value<int>()) {
          ai_config.research_assistant.timeout_ms = *value;
        }
        if (auto value = (*research_assistant_table)["max_topics_generated"].value<int>()) {
          ai_config.research_assistant.max_topics_generated = static_cast<size_t>(*value);
        }
        if (auto value = (*research_assistant_table)["research_style"].value<std::string>()) {
          ai_config.research_assistant.research_style = *value;
        }
      }
      
      // AI Writing Coach settings
      if (auto writing_coach_table = (*ai_table)["writing_coach"].as_table()) {
        if (auto value = (*writing_coach_table)["enabled"].value<bool>()) {
          ai_config.writing_coach.enabled = *value;
        }
        if (auto value = (*writing_coach_table)["max_tokens"].value<int>()) {
          ai_config.writing_coach.max_tokens = *value;
        }
        if (auto value = (*writing_coach_table)["temperature"].value<double>()) {
          ai_config.writing_coach.temperature = *value;
        }
        if (auto value = (*writing_coach_table)["timeout_ms"].value<int>()) {
          ai_config.writing_coach.timeout_ms = *value;
        }
        if (auto value = (*writing_coach_table)["max_text_length"].value<int>()) {
          ai_config.writing_coach.max_text_length = static_cast<size_t>(*value);
        }
        if (auto value = (*writing_coach_table)["feedback_level"].value<std::string>()) {
          ai_config.writing_coach.feedback_level = *value;
        }
        if (auto value = (*writing_coach_table)["include_style_suggestions"].value<bool>()) {
          ai_config.writing_coach.include_style_suggestions = *value;
        }
      }
      
      // Smart Content Generation configuration
      if (auto content_gen_table = ai_table->get("smart_content_generation")->as_table()) {
        if (auto value = (*content_gen_table)["enabled"].value<bool>()) {
          ai_config.smart_content_generation.enabled = *value;
        }
        if (auto value = (*content_gen_table)["max_tokens"].value<int>()) {
          ai_config.smart_content_generation.max_tokens = *value;
        }
        if (auto value = (*content_gen_table)["temperature"].value<double>()) {
          ai_config.smart_content_generation.temperature = *value;
        }
        if (auto value = (*content_gen_table)["timeout_ms"].value<int>()) {
          ai_config.smart_content_generation.timeout_ms = *value;
        }
        if (auto value = (*content_gen_table)["content_style"].value<std::string>()) {
          ai_config.smart_content_generation.content_style = *value;
        }
        if (auto value = (*content_gen_table)["max_content_length"].value<int>()) {
          ai_config.smart_content_generation.max_content_length = static_cast<size_t>(*value);
        }
        if (auto value = (*content_gen_table)["include_outline"].value<bool>()) {
          ai_config.smart_content_generation.include_outline = *value;
        }
      }
      
      // Intelligent Templates configuration
      if (auto templates_table = ai_table->get("intelligent_templates")->as_table()) {
        if (auto value = (*templates_table)["enabled"].value<bool>()) {
          ai_config.intelligent_templates.enabled = *value;
        }
        if (auto value = (*templates_table)["max_tokens"].value<int>()) {
          ai_config.intelligent_templates.max_tokens = *value;
        }
        if (auto value = (*templates_table)["temperature"].value<double>()) {
          ai_config.intelligent_templates.temperature = *value;
        }
        if (auto value = (*templates_table)["timeout_ms"].value<int>()) {
          ai_config.intelligent_templates.timeout_ms = *value;
        }
        if (auto value = (*templates_table)["max_suggestions"].value<int>()) {
          ai_config.intelligent_templates.max_suggestions = static_cast<size_t>(*value);
        }
        if (auto value = (*templates_table)["analyze_existing_content"].value<bool>()) {
          ai_config.intelligent_templates.analyze_existing_content = *value;
        }
      }
      
      // Cross-Note Insights configuration
      if (auto insights_table = ai_table->get("cross_note_insights")->as_table()) {
        if (auto value = (*insights_table)["enabled"].value<bool>()) {
          ai_config.cross_note_insights.enabled = *value;
        }
        if (auto value = (*insights_table)["max_tokens"].value<int>()) {
          ai_config.cross_note_insights.max_tokens = *value;
        }
        if (auto value = (*insights_table)["temperature"].value<double>()) {
          ai_config.cross_note_insights.temperature = *value;
        }
        if (auto value = (*insights_table)["timeout_ms"].value<int>()) {
          ai_config.cross_note_insights.timeout_ms = *value;
        }
        if (auto value = (*insights_table)["max_notes_analyzed"].value<int>()) {
          ai_config.cross_note_insights.max_notes_analyzed = static_cast<size_t>(*value);
        }
        if (auto value = (*insights_table)["insight_focus"].value<std::string>()) {
          ai_config.cross_note_insights.insight_focus = *value;
        }
      }
      
      // Smart Search Enhancement configuration
      if (auto search_table = ai_table->get("smart_search_enhancement")->as_table()) {
        if (auto value = (*search_table)["enabled"].value<bool>()) {
          ai_config.smart_search_enhancement.enabled = *value;
        }
        if (auto value = (*search_table)["max_tokens"].value<int>()) {
          ai_config.smart_search_enhancement.max_tokens = *value;
        }
        if (auto value = (*search_table)["temperature"].value<double>()) {
          ai_config.smart_search_enhancement.temperature = *value;
        }
        if (auto value = (*search_table)["timeout_ms"].value<int>()) {
          ai_config.smart_search_enhancement.timeout_ms = *value;
        }
        if (auto value = (*search_table)["expand_synonyms"].value<bool>()) {
          ai_config.smart_search_enhancement.expand_synonyms = *value;
        }
        if (auto value = (*search_table)["analyze_intent"].value<bool>()) {
          ai_config.smart_search_enhancement.analyze_intent = *value;
        }
      }
      
      // Smart Note Merging configuration
      if (auto merging_table = ai_table->get("smart_note_merging")->as_table()) {
        if (auto value = (*merging_table)["enabled"].value<bool>()) {
          ai_config.smart_note_merging.enabled = *value;
        }
        if (auto value = (*merging_table)["max_tokens"].value<int>()) {
          ai_config.smart_note_merging.max_tokens = *value;
        }
        if (auto value = (*merging_table)["temperature"].value<double>()) {
          ai_config.smart_note_merging.temperature = *value;
        }
        if (auto value = (*merging_table)["timeout_ms"].value<int>()) {
          ai_config.smart_note_merging.timeout_ms = *value;
        }
        if (auto value = (*merging_table)["similarity_threshold"].value<double>()) {
          ai_config.smart_note_merging.similarity_threshold = *value;
        }
        if (auto value = (*merging_table)["max_merge_candidates"].value<int>()) {
          ai_config.smart_note_merging.max_merge_candidates = static_cast<size_t>(*value);
        }
      }
      
      // Phase 5 AI configurations
      
      // Workflow Orchestrator configuration
      if (auto orchestrator_table = (*ai_table)["workflow_orchestrator"].as_table()) {
        if (auto value = (*orchestrator_table)["enabled"].value<bool>()) {
          ai_config.workflow_orchestrator.enabled = *value;
        }
        if (auto value = (*orchestrator_table)["max_tokens"].value<int>()) {
          ai_config.workflow_orchestrator.max_tokens = *value;
        }
        if (auto value = (*orchestrator_table)["temperature"].value<double>()) {
          ai_config.workflow_orchestrator.temperature = *value;
        }
        if (auto value = (*orchestrator_table)["timeout_per_step_ms"].value<int>()) {
          ai_config.workflow_orchestrator.timeout_per_step_ms = *value;
        }
        if (auto value = (*orchestrator_table)["max_steps"].value<int>()) {
          ai_config.workflow_orchestrator.max_steps = *value;
        }
        if (auto value = (*orchestrator_table)["allow_recursive_calls"].value<bool>()) {
          ai_config.workflow_orchestrator.allow_recursive_calls = *value;
        }
        if (auto array = (*orchestrator_table)["allowed_operations"].as_array()) {
          std::vector<std::string> operations;
          for (const auto& item : *array) {
            if (auto str = item.value<std::string>()) {
              operations.push_back(*str);
            }
          }
          if (!operations.empty()) {
            ai_config.workflow_orchestrator.allowed_operations = std::move(operations);
          }
        }
      }
      
      // Project Assistant configuration  
      if (auto project_table = (*ai_table)["project_assistant"].as_table()) {
        if (auto value = (*project_table)["enabled"].value<bool>()) {
          ai_config.project_assistant.enabled = *value;
        }
        if (auto value = (*project_table)["max_tokens"].value<int>()) {
          ai_config.project_assistant.max_tokens = *value;
        }
        if (auto value = (*project_table)["temperature"].value<double>()) {
          ai_config.project_assistant.temperature = *value;
        }
        if (auto value = (*project_table)["timeout_ms"].value<int>()) {
          ai_config.project_assistant.timeout_ms = *value;
        }
        if (auto value = (*project_table)["project_scope"].value<std::string>()) {
          ai_config.project_assistant.project_scope = *value;
        }
        if (auto value = (*project_table)["auto_generate_milestones"].value<bool>()) {
          ai_config.project_assistant.auto_generate_milestones = *value;
        }
        if (auto value = (*project_table)["max_related_notes"].value<int>()) {
          ai_config.project_assistant.max_related_notes = *value;
        }
      }
      
      // Learning Path Generator configuration
      if (auto learning_table = (*ai_table)["learning_path_generator"].as_table()) {
        if (auto value = (*learning_table)["enabled"].value<bool>()) {
          ai_config.learning_path_generator.enabled = *value;
        }
        if (auto value = (*learning_table)["max_tokens"].value<int>()) {
          ai_config.learning_path_generator.max_tokens = *value;
        }
        if (auto value = (*learning_table)["temperature"].value<double>()) {
          ai_config.learning_path_generator.temperature = *value;
        }
        if (auto value = (*learning_table)["timeout_ms"].value<int>()) {
          ai_config.learning_path_generator.timeout_ms = *value;
        }
        if (auto value = (*learning_table)["max_prerequisites"].value<int>()) {
          ai_config.learning_path_generator.max_prerequisites = *value;
        }
        if (auto value = (*learning_table)["max_learning_steps"].value<int>()) {
          ai_config.learning_path_generator.max_learning_steps = *value;
        }
        if (auto value = (*learning_table)["include_resources"].value<bool>()) {
          ai_config.learning_path_generator.include_resources = *value;
        }
      }
      
      // Knowledge Synthesis configuration
      if (auto synthesis_table = (*ai_table)["knowledge_synthesis"].as_table()) {
        if (auto value = (*synthesis_table)["enabled"].value<bool>()) {
          ai_config.knowledge_synthesis.enabled = *value;
        }
        if (auto value = (*synthesis_table)["max_tokens"].value<int>()) {
          ai_config.knowledge_synthesis.max_tokens = *value;
        }
        if (auto value = (*synthesis_table)["temperature"].value<double>()) {
          ai_config.knowledge_synthesis.temperature = *value;
        }
        if (auto value = (*synthesis_table)["timeout_ms"].value<int>()) {
          ai_config.knowledge_synthesis.timeout_ms = *value;
        }
        if (auto value = (*synthesis_table)["max_source_notes"].value<int>()) {
          ai_config.knowledge_synthesis.max_source_notes = *value;
        }
        if (auto value = (*synthesis_table)["detect_contradictions"].value<bool>()) {
          ai_config.knowledge_synthesis.detect_contradictions = *value;
        }
        if (auto value = (*synthesis_table)["suggest_gaps"].value<bool>()) {
          ai_config.knowledge_synthesis.suggest_gaps = *value;
        }
      }
      
      // Journal Insights configuration  
      if (auto journal_table = (*ai_table)["journal_insights"].as_table()) {
        if (auto value = (*journal_table)["enabled"].value<bool>()) {
          ai_config.journal_insights.enabled = *value;
        }
        if (auto value = (*journal_table)["max_tokens"].value<int>()) {
          ai_config.journal_insights.max_tokens = *value;
        }
        if (auto value = (*journal_table)["temperature"].value<double>()) {
          ai_config.journal_insights.temperature = *value;
        }
        if (auto value = (*journal_table)["timeout_ms"].value<int>()) {
          ai_config.journal_insights.timeout_ms = *value;
        }
        if (auto value = (*journal_table)["analysis_window_days"].value<int>()) {
          ai_config.journal_insights.analysis_window_days = *value;
        }
        if (auto value = (*journal_table)["track_mood_patterns"].value<bool>()) {
          ai_config.journal_insights.track_mood_patterns = *value;
        }
        if (auto value = (*journal_table)["track_productivity_patterns"].value<bool>()) {
          ai_config.journal_insights.track_productivity_patterns = *value;
        }
        if (auto value = (*journal_table)["suggest_habit_changes"].value<bool>()) {
          ai_config.journal_insights.suggest_habit_changes = *value;
        }
      }

      // Phase 6 AI configurations
      
      // Multi-modal AI configuration (Phase 6)
      if (auto multi_modal_table = ai_table->get("multi_modal")->as_table()) {
        if (auto value = (*multi_modal_table)["enabled"].value<bool>()) {
          ai_config.multi_modal.enabled = *value;
        }
        if (auto value = (*multi_modal_table)["max_tokens"].value<int>()) {
          ai_config.multi_modal.max_tokens = *value;
        }
        if (auto value = (*multi_modal_table)["temperature"].value<double>()) {
          ai_config.multi_modal.temperature = *value;
        }
        if (auto value = (*multi_modal_table)["timeout_ms"].value<int>()) {
          ai_config.multi_modal.timeout_ms = *value;
        }
        if (auto value = (*multi_modal_table)["analyze_images"].value<bool>()) {
          ai_config.multi_modal.analyze_images = *value;
        }
        if (auto value = (*multi_modal_table)["generate_alt_text"].value<bool>()) {
          ai_config.multi_modal.generate_alt_text = *value;
        }
        if (auto value = (*multi_modal_table)["extract_text_from_images"].value<bool>()) {
          ai_config.multi_modal.extract_text_from_images = *value;
        }
        if (auto value = (*multi_modal_table)["analyze_document_structure"].value<bool>()) {
          ai_config.multi_modal.analyze_document_structure = *value;
        }
      }
      
      // Voice integration configuration (Phase 6)
      if (auto voice_table = ai_table->get("voice_integration")->as_table()) {
        if (auto value = (*voice_table)["enabled"].value<bool>()) {
          ai_config.voice_integration.enabled = *value;
        }
        if (auto value = (*voice_table)["max_tokens"].value<int>()) {
          ai_config.voice_integration.max_tokens = *value;
        }
        if (auto value = (*voice_table)["temperature"].value<double>()) {
          ai_config.voice_integration.temperature = *value;
        }
        if (auto value = (*voice_table)["timeout_ms"].value<int>()) {
          ai_config.voice_integration.timeout_ms = *value;
        }
        if (auto value = (*voice_table)["tts_voice"].value<std::string>()) {
          ai_config.voice_integration.tts_voice = *value;
        }
        if (auto value = (*voice_table)["speech_language"].value<std::string>()) {
          ai_config.voice_integration.speech_language = *value;
        }
        if (auto value = (*voice_table)["auto_punctuation"].value<bool>()) {
          ai_config.voice_integration.auto_punctuation = *value;
        }
        if (auto value = (*voice_table)["background_listening"].value<bool>()) {
          ai_config.voice_integration.background_listening = *value;
        }
      }
      
      // Context awareness configuration (Phase 6)
      if (auto context_table = ai_table->get("context_awareness")->as_table()) {
        if (auto value = (*context_table)["enabled"].value<bool>()) {
          ai_config.context_awareness.enabled = *value;
        }
        if (auto value = (*context_table)["max_tokens"].value<int>()) {
          ai_config.context_awareness.max_tokens = *value;
        }
        if (auto value = (*context_table)["temperature"].value<double>()) {
          ai_config.context_awareness.temperature = *value;
        }
        if (auto value = (*context_table)["timeout_ms"].value<int>()) {
          ai_config.context_awareness.timeout_ms = *value;
        }
        if (auto value = (*context_table)["context_window_notes"].value<int>()) {
          ai_config.context_awareness.context_window_notes = *value;
        }
        if (auto value = (*context_table)["track_reading_patterns"].value<bool>()) {
          ai_config.context_awareness.track_reading_patterns = *value;
        }
        if (auto value = (*context_table)["predict_next_actions"].value<bool>()) {
          ai_config.context_awareness.predict_next_actions = *value;
        }
        if (auto value = (*context_table)["suggest_related_content"].value<bool>()) {
          ai_config.context_awareness.suggest_related_content = *value;
        }
        if (auto value = (*context_table)["adaptive_interface"].value<bool>()) {
          ai_config.context_awareness.adaptive_interface = *value;
        }
      }
      
      // Workspace AI configuration (Phase 6)
      if (auto workspace_table = ai_table->get("workspace_ai")->as_table()) {
        if (auto value = (*workspace_table)["enabled"].value<bool>()) {
          ai_config.workspace_ai.enabled = *value;
        }
        if (auto value = (*workspace_table)["max_tokens"].value<int>()) {
          ai_config.workspace_ai.max_tokens = *value;
        }
        if (auto value = (*workspace_table)["temperature"].value<double>()) {
          ai_config.workspace_ai.temperature = *value;
        }
        if (auto value = (*workspace_table)["timeout_ms"].value<int>()) {
          ai_config.workspace_ai.timeout_ms = *value;
        }
        if (auto value = (*workspace_table)["smart_folder_suggestions"].value<bool>()) {
          ai_config.workspace_ai.smart_folder_suggestions = *value;
        }
        if (auto value = (*workspace_table)["auto_tag_relationships"].value<bool>()) {
          ai_config.workspace_ai.auto_tag_relationships = *value;
        }
        if (auto value = (*workspace_table)["workspace_health_monitoring"].value<bool>()) {
          ai_config.workspace_ai.workspace_health_monitoring = *value;
        }
        if (auto value = (*workspace_table)["smart_archive_suggestions"].value<bool>()) {
          ai_config.workspace_ai.smart_archive_suggestions = *value;
        }
      }
      
      // Predictive AI configuration (Phase 6)
      if (auto predictive_table = ai_table->get("predictive_ai")->as_table()) {
        if (auto value = (*predictive_table)["enabled"].value<bool>()) {
          ai_config.predictive_ai.enabled = *value;
        }
        if (auto value = (*predictive_table)["max_tokens"].value<int>()) {
          ai_config.predictive_ai.max_tokens = *value;
        }
        if (auto value = (*predictive_table)["temperature"].value<double>()) {
          ai_config.predictive_ai.temperature = *value;
        }
        if (auto value = (*predictive_table)["timeout_ms"].value<int>()) {
          ai_config.predictive_ai.timeout_ms = *value;
        }
        if (auto value = (*predictive_table)["predict_note_needs"].value<bool>()) {
          ai_config.predictive_ai.predict_note_needs = *value;
        }
        if (auto value = (*predictive_table)["suggest_meeting_prep"].value<bool>()) {
          ai_config.predictive_ai.suggest_meeting_prep = *value;
        }
        if (auto value = (*predictive_table)["proactive_reminders"].value<bool>()) {
          ai_config.predictive_ai.proactive_reminders = *value;
        }
        if (auto value = (*predictive_table)["learning_path_optimization"].value<bool>()) {
          ai_config.predictive_ai.learning_path_optimization = *value;
        }
      }

      // Phase 7 AI configuration loading
      
      // Collaborative AI configuration (Phase 7)
      if (auto collaborative_table = ai_table->get("collaborative_ai")->as_table()) {
        if (auto value = (*collaborative_table)["enabled"].value<bool>()) {
          ai_config.collaborative_ai.enabled = *value;
        }
        if (auto value = (*collaborative_table)["max_tokens"].value<int>()) {
          ai_config.collaborative_ai.max_tokens = *value;
        }
        if (auto value = (*collaborative_table)["temperature"].value<double>()) {
          ai_config.collaborative_ai.temperature = *value;
        }
        if (auto value = (*collaborative_table)["timeout_ms"].value<int>()) {
          ai_config.collaborative_ai.timeout_ms = *value;
        }
        if (auto value = (*collaborative_table)["enable_shared_sessions"].value<bool>()) {
          ai_config.collaborative_ai.enable_shared_sessions = *value;
        }
        if (auto value = (*collaborative_table)["cross_reference_analysis"].value<bool>()) {
          ai_config.collaborative_ai.cross_reference_analysis = *value;
        }
        if (auto value = (*collaborative_table)["collaborative_editing"].value<bool>()) {
          ai_config.collaborative_ai.collaborative_editing = *value;
        }
        if (auto value = (*collaborative_table)["consensus_building"].value<bool>()) {
          ai_config.collaborative_ai.consensus_building = *value;
        }
      }
      
      // Knowledge graph configuration (Phase 7)
      if (auto graph_table = ai_table->get("knowledge_graph")->as_table()) {
        if (auto value = (*graph_table)["enabled"].value<bool>()) {
          ai_config.knowledge_graph.enabled = *value;
        }
        if (auto value = (*graph_table)["max_tokens"].value<int>()) {
          ai_config.knowledge_graph.max_tokens = *value;
        }
        if (auto value = (*graph_table)["temperature"].value<double>()) {
          ai_config.knowledge_graph.temperature = *value;
        }
        if (auto value = (*graph_table)["timeout_ms"].value<int>()) {
          ai_config.knowledge_graph.timeout_ms = *value;
        }
        if (auto value = (*graph_table)["auto_generate_graphs"].value<bool>()) {
          ai_config.knowledge_graph.auto_generate_graphs = *value;
        }
        if (auto value = (*graph_table)["semantic_clustering"].value<bool>()) {
          ai_config.knowledge_graph.semantic_clustering = *value;
        }
        if (auto value = (*graph_table)["relationship_inference"].value<bool>()) {
          ai_config.knowledge_graph.relationship_inference = *value;
        }
        if (auto value = (*graph_table)["visual_graph_export"].value<bool>()) {
          ai_config.knowledge_graph.visual_graph_export = *value;
        }
      }
      
      // Expert systems configuration (Phase 7)
      if (auto expert_table = ai_table->get("expert_systems")->as_table()) {
        if (auto value = (*expert_table)["enabled"].value<bool>()) {
          ai_config.expert_systems.enabled = *value;
        }
        if (auto value = (*expert_table)["max_tokens"].value<int>()) {
          ai_config.expert_systems.max_tokens = *value;
        }
        if (auto value = (*expert_table)["temperature"].value<double>()) {
          ai_config.expert_systems.temperature = *value;
        }
        if (auto value = (*expert_table)["timeout_ms"].value<int>()) {
          ai_config.expert_systems.timeout_ms = *value;
        }
        if (auto value = (*expert_table)["primary_domain"].value<std::string>()) {
          ai_config.expert_systems.primary_domain = *value;
        }
        if (auto value = (*expert_table)["multi_domain_support"].value<bool>()) {
          ai_config.expert_systems.multi_domain_support = *value;
        }
        if (auto value = (*expert_table)["adaptive_expertise"].value<bool>()) {
          ai_config.expert_systems.adaptive_expertise = *value;
        }
        if (auto value = (*expert_table)["citation_generation"].value<bool>()) {
          ai_config.expert_systems.citation_generation = *value;
        }
      }
      
      // Intelligent workflows configuration (Phase 7)
      if (auto workflow_table = ai_table->get("intelligent_workflows")->as_table()) {
        if (auto value = (*workflow_table)["enabled"].value<bool>()) {
          ai_config.intelligent_workflows.enabled = *value;
        }
        if (auto value = (*workflow_table)["max_tokens"].value<int>()) {
          ai_config.intelligent_workflows.max_tokens = *value;
        }
        if (auto value = (*workflow_table)["temperature"].value<double>()) {
          ai_config.intelligent_workflows.temperature = *value;
        }
        if (auto value = (*workflow_table)["timeout_ms"].value<int>()) {
          ai_config.intelligent_workflows.timeout_ms = *value;
        }
        if (auto value = (*workflow_table)["auto_workflow_detection"].value<bool>()) {
          ai_config.intelligent_workflows.auto_workflow_detection = *value;
        }
        if (auto value = (*workflow_table)["process_optimization"].value<bool>()) {
          ai_config.intelligent_workflows.process_optimization = *value;
        }
        if (auto value = (*workflow_table)["deadline_management"].value<bool>()) {
          ai_config.intelligent_workflows.deadline_management = *value;
        }
        if (auto value = (*workflow_table)["resource_allocation"].value<bool>()) {
          ai_config.intelligent_workflows.resource_allocation = *value;
        }
      }
      
      // Meta-learning configuration (Phase 7)
      if (auto meta_table = ai_table->get("meta_learning")->as_table()) {
        if (auto value = (*meta_table)["enabled"].value<bool>()) {
          ai_config.meta_learning.enabled = *value;
        }
        if (auto value = (*meta_table)["max_tokens"].value<int>()) {
          ai_config.meta_learning.max_tokens = *value;
        }
        if (auto value = (*meta_table)["temperature"].value<double>()) {
          ai_config.meta_learning.temperature = *value;
        }
        if (auto value = (*meta_table)["timeout_ms"].value<int>()) {
          ai_config.meta_learning.timeout_ms = *value;
        }
        if (auto value = (*meta_table)["user_pattern_learning"].value<bool>()) {
          ai_config.meta_learning.user_pattern_learning = *value;
        }
        if (auto value = (*meta_table)["adaptive_assistance"].value<bool>()) {
          ai_config.meta_learning.adaptive_assistance = *value;
        }
        if (auto value = (*meta_table)["personalization"].value<bool>()) {
          ai_config.meta_learning.personalization = *value;
        }
        if (auto value = (*meta_table)["learning_analytics"].value<bool>()) {
          ai_config.meta_learning.learning_analytics = *value;
        }
      }
      
      ai = ai_config;
    }
    
    // Performance configuration
    if (auto perf_table = config_data["performance"].as_table()) {
      if (auto value = (*perf_table)["cache_size_mb"].value<int>()) {
        performance.cache_size_mb = static_cast<size_t>(*value);
      }
      if (auto value = (*perf_table)["max_file_size_mb"].value<int>()) {
        performance.max_file_size_mb = static_cast<size_t>(*value);
      }
      if (auto value = (*perf_table)["sqlite_cache_size"].value<int>()) {
        performance.sqlite_cache_size = *value;
      }
      if (auto value = (*perf_table)["sqlite_journal_mode"].value<std::string>()) {
        performance.sqlite_journal_mode = *value;
      }
      if (auto value = (*perf_table)["sqlite_synchronous"].value<std::string>()) {
        performance.sqlite_synchronous = *value;
      }
      if (auto value = (*perf_table)["sqlite_temp_store"].value<std::string>()) {
        performance.sqlite_temp_store = *value;
      }
    }
    
    return {};
    
  } catch (const toml::parse_error& e) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "TOML parse error: " + std::string(e.what())));
  }
}

Result<void> Config::save(const std::filesystem::path& config_path) const {
  std::filesystem::path save_path = config_path.empty() ? config_path_ : config_path;
  
  if (save_path.empty()) {
    save_path = defaultConfigPath();
  }
  
  try {
    toml::table config_data;
    
    // Core paths
    if (!root.empty()) config_data.insert_or_assign("root", root.string());
    if (!notes_dir.empty()) config_data.insert_or_assign("notes_dir", notes_dir.string());
    if (!attachments_dir.empty()) config_data.insert_or_assign("attachments_dir", attachments_dir.string());
    if (!trash_dir.empty()) config_data.insert_or_assign("trash_dir", trash_dir.string());
    if (!index_file.empty()) config_data.insert_or_assign("index_file", index_file.string());
    
    // Editor
    if (!editor.empty()) config_data.insert_or_assign("editor", editor);
    
    // Indexer
    config_data.insert_or_assign("indexer", indexerTypeToString(indexer));
    
    // Encryption
    config_data.insert_or_assign("encryption", encryptionTypeToString(encryption));
    if (!age_recipient.empty()) config_data.insert_or_assign("age_recipient", age_recipient);
    
    // Sync
    config_data.insert_or_assign("sync", syncTypeToString(sync));
    if (!git_remote.empty()) config_data.insert_or_assign("git_remote", git_remote);
    if (!git_user_name.empty()) config_data.insert_or_assign("git_user_name", git_user_name);
    if (!git_user_email.empty()) config_data.insert_or_assign("git_user_email", git_user_email);
    
    // Auto-sync configuration
    auto auto_sync_table = toml::table{};
    auto_sync_table.insert_or_assign("enabled", auto_sync.enabled);
    auto_sync_table.insert_or_assign("auto_pull_on_startup", auto_sync.auto_pull_on_startup);
    auto_sync_table.insert_or_assign("auto_push_on_changes", auto_sync.auto_push_on_changes);
    auto_sync_table.insert_or_assign("auto_push_delay_seconds", auto_sync.auto_push_delay_seconds);
    auto_sync_table.insert_or_assign("sync_interval_seconds", auto_sync.sync_interval_seconds);
    auto_sync_table.insert_or_assign("conflict_strategy", auto_sync.conflict_strategy);
    auto_sync_table.insert_or_assign("max_auto_resolve_attempts", auto_sync.max_auto_resolve_attempts);
    auto_sync_table.insert_or_assign("sync_on_shutdown", auto_sync.sync_on_shutdown);
    auto_sync_table.insert_or_assign("show_sync_status", auto_sync.show_sync_status);
    config_data.insert_or_assign("auto_sync", auto_sync_table);
    
    // Defaults
    auto defaults_table = toml::table{};
    if (!default_notebook.empty()) defaults_table.insert_or_assign("notebook", default_notebook);
    if (!default_tags.empty()) {
      auto tags_array = toml::array{};
      for (const auto& tag : default_tags) {
        tags_array.push_back(tag);
      }
      defaults_table.insert_or_assign("tags", tags_array);
    }
    config_data.insert_or_assign("defaults", defaults_table);
    
    // AI configuration
    if (ai.has_value()) {
      auto ai_table = toml::table{};
      ai_table.insert_or_assign("provider", ai->provider);
      ai_table.insert_or_assign("model", ai->model);
      ai_table.insert_or_assign("api_key", ai->api_key);
      ai_table.insert_or_assign("max_tokens", ai->max_tokens);
      ai_table.insert_or_assign("temperature", ai->temperature);
      ai_table.insert_or_assign("rate_limit_qpm", ai->rate_limit_qpm);
      ai_table.insert_or_assign("daily_usd_budget", ai->daily_usd_budget);
      ai_table.insert_or_assign("enable_embeddings", ai->enable_embeddings);
      ai_table.insert_or_assign("embedding_model", ai->embedding_model);
      ai_table.insert_or_assign("top_k", ai->top_k);
      
      auto redaction_table = toml::table{};
      redaction_table.insert_or_assign("strip_emails", ai->strip_emails);
      redaction_table.insert_or_assign("strip_urls", ai->strip_urls);
      redaction_table.insert_or_assign("mask_numbers", ai->mask_numbers);
      ai_table.insert_or_assign("redaction", redaction_table);
      
      // AI Explanations configuration
      auto explanations_table = toml::table{};
      explanations_table.insert_or_assign("enabled", ai->explanations.enabled);
      explanations_table.insert_or_assign("brief_max_words", static_cast<int>(ai->explanations.brief_max_words));
      explanations_table.insert_or_assign("expanded_max_words", static_cast<int>(ai->explanations.expanded_max_words));
      explanations_table.insert_or_assign("timeout_ms", ai->explanations.timeout_ms);
      explanations_table.insert_or_assign("cache_explanations", ai->explanations.cache_explanations);
      explanations_table.insert_or_assign("max_cache_size", static_cast<int>(ai->explanations.max_cache_size));
      explanations_table.insert_or_assign("context_radius", static_cast<int>(ai->explanations.context_radius));
      ai_table.insert_or_assign("explanations", explanations_table);
      
      // AI Smart Completion configuration
      auto smart_completion_table = toml::table{};
      smart_completion_table.insert_or_assign("enabled", ai->smart_completion.enabled);
      smart_completion_table.insert_or_assign("max_tokens", ai->smart_completion.max_tokens);
      smart_completion_table.insert_or_assign("temperature", ai->smart_completion.temperature);
      smart_completion_table.insert_or_assign("max_completion_length", static_cast<int>(ai->smart_completion.max_completion_length));
      smart_completion_table.insert_or_assign("timeout_ms", ai->smart_completion.timeout_ms);
      ai_table.insert_or_assign("smart_completion", smart_completion_table);
      
      // AI Semantic Search configuration
      auto semantic_search_table = toml::table{};
      semantic_search_table.insert_or_assign("enabled", ai->semantic_search.enabled);
      semantic_search_table.insert_or_assign("max_tokens", ai->semantic_search.max_tokens);
      semantic_search_table.insert_or_assign("temperature", ai->semantic_search.temperature);
      semantic_search_table.insert_or_assign("timeout_ms", ai->semantic_search.timeout_ms);
      semantic_search_table.insert_or_assign("max_notes_per_query", static_cast<int>(ai->semantic_search.max_notes_per_query));
      ai_table.insert_or_assign("semantic_search", semantic_search_table);
      
      // AI Grammar & Style Check configuration
      auto grammar_style_table = toml::table{};
      grammar_style_table.insert_or_assign("enabled", ai->grammar_style_check.enabled);
      grammar_style_table.insert_or_assign("max_tokens", ai->grammar_style_check.max_tokens);
      grammar_style_table.insert_or_assign("temperature", ai->grammar_style_check.temperature);
      grammar_style_table.insert_or_assign("timeout_ms", ai->grammar_style_check.timeout_ms);
      grammar_style_table.insert_or_assign("max_text_length", static_cast<int>(ai->grammar_style_check.max_text_length));
      grammar_style_table.insert_or_assign("style", ai->grammar_style_check.style);
      ai_table.insert_or_assign("grammar_style_check", grammar_style_table);
      
      // AI Smart Examples configuration
      auto smart_examples_table = toml::table{};
      smart_examples_table.insert_or_assign("enabled", ai->smart_examples.enabled);
      smart_examples_table.insert_or_assign("max_tokens", ai->smart_examples.max_tokens);
      smart_examples_table.insert_or_assign("temperature", ai->smart_examples.temperature);
      smart_examples_table.insert_or_assign("timeout_ms", ai->smart_examples.timeout_ms);
      smart_examples_table.insert_or_assign("max_examples", static_cast<int>(ai->smart_examples.max_examples));
      smart_examples_table.insert_or_assign("example_type", ai->smart_examples.example_type);
      ai_table.insert_or_assign("smart_examples", smart_examples_table);
      
      // AI Code Generation configuration
      auto code_generation_table = toml::table{};
      code_generation_table.insert_or_assign("enabled", ai->code_generation.enabled);
      code_generation_table.insert_or_assign("max_tokens", ai->code_generation.max_tokens);
      code_generation_table.insert_or_assign("temperature", ai->code_generation.temperature);
      code_generation_table.insert_or_assign("timeout_ms", ai->code_generation.timeout_ms);
      code_generation_table.insert_or_assign("language", ai->code_generation.language);
      code_generation_table.insert_or_assign("style", ai->code_generation.style);
      ai_table.insert_or_assign("code_generation", code_generation_table);
      
      // AI Smart Summarization configuration
      auto smart_summarization_table = toml::table{};
      smart_summarization_table.insert_or_assign("enabled", ai->smart_summarization.enabled);
      smart_summarization_table.insert_or_assign("max_tokens", ai->smart_summarization.max_tokens);
      smart_summarization_table.insert_or_assign("temperature", ai->smart_summarization.temperature);
      smart_summarization_table.insert_or_assign("timeout_ms", ai->smart_summarization.timeout_ms);
      smart_summarization_table.insert_or_assign("max_text_length", static_cast<int>(ai->smart_summarization.max_text_length));
      smart_summarization_table.insert_or_assign("style", ai->smart_summarization.style);
      smart_summarization_table.insert_or_assign("include_metadata", ai->smart_summarization.include_metadata);
      ai_table.insert_or_assign("smart_summarization", smart_summarization_table);
      
      // AI Note Relationships configuration
      auto note_relationships_table = toml::table{};
      note_relationships_table.insert_or_assign("enabled", ai->note_relationships.enabled);
      note_relationships_table.insert_or_assign("max_tokens", ai->note_relationships.max_tokens);
      note_relationships_table.insert_or_assign("temperature", ai->note_relationships.temperature);
      note_relationships_table.insert_or_assign("timeout_ms", ai->note_relationships.timeout_ms);
      note_relationships_table.insert_or_assign("max_notes_to_analyze", static_cast<int>(ai->note_relationships.max_notes_to_analyze));
      note_relationships_table.insert_or_assign("similarity_threshold", ai->note_relationships.similarity_threshold);
      ai_table.insert_or_assign("note_relationships", note_relationships_table);
      
      // AI Smart Organization configuration
      auto smart_organization_table = toml::table{};
      smart_organization_table.insert_or_assign("enabled", ai->smart_organization.enabled);
      smart_organization_table.insert_or_assign("max_tokens", ai->smart_organization.max_tokens);
      smart_organization_table.insert_or_assign("temperature", ai->smart_organization.temperature);
      smart_organization_table.insert_or_assign("timeout_ms", ai->smart_organization.timeout_ms);
      smart_organization_table.insert_or_assign("max_notes_per_batch", static_cast<int>(ai->smart_organization.max_notes_per_batch));
      smart_organization_table.insert_or_assign("suggest_new_notebooks", ai->smart_organization.suggest_new_notebooks);
      ai_table.insert_or_assign("smart_organization", smart_organization_table);
      
      // AI Content Enhancement configuration
      auto content_enhancement_table = toml::table{};
      content_enhancement_table.insert_or_assign("enabled", ai->content_enhancement.enabled);
      content_enhancement_table.insert_or_assign("max_tokens", ai->content_enhancement.max_tokens);
      content_enhancement_table.insert_or_assign("temperature", ai->content_enhancement.temperature);
      content_enhancement_table.insert_or_assign("timeout_ms", ai->content_enhancement.timeout_ms);
      content_enhancement_table.insert_or_assign("max_text_length", static_cast<int>(ai->content_enhancement.max_text_length));
      content_enhancement_table.insert_or_assign("enhancement_focus", ai->content_enhancement.enhancement_focus);
      ai_table.insert_or_assign("content_enhancement", content_enhancement_table);
      
      // AI Research Assistant configuration
      auto research_assistant_table = toml::table{};
      research_assistant_table.insert_or_assign("enabled", ai->research_assistant.enabled);
      research_assistant_table.insert_or_assign("max_tokens", ai->research_assistant.max_tokens);
      research_assistant_table.insert_or_assign("temperature", ai->research_assistant.temperature);
      research_assistant_table.insert_or_assign("timeout_ms", ai->research_assistant.timeout_ms);
      research_assistant_table.insert_or_assign("max_topics_generated", static_cast<int>(ai->research_assistant.max_topics_generated));
      research_assistant_table.insert_or_assign("research_style", ai->research_assistant.research_style);
      ai_table.insert_or_assign("research_assistant", research_assistant_table);
      
      // AI Writing Coach configuration
      auto writing_coach_table = toml::table{};
      writing_coach_table.insert_or_assign("enabled", ai->writing_coach.enabled);
      writing_coach_table.insert_or_assign("max_tokens", ai->writing_coach.max_tokens);
      writing_coach_table.insert_or_assign("temperature", ai->writing_coach.temperature);
      writing_coach_table.insert_or_assign("timeout_ms", ai->writing_coach.timeout_ms);
      writing_coach_table.insert_or_assign("max_text_length", static_cast<int>(ai->writing_coach.max_text_length));
      writing_coach_table.insert_or_assign("feedback_level", ai->writing_coach.feedback_level);
      writing_coach_table.insert_or_assign("include_style_suggestions", ai->writing_coach.include_style_suggestions);
      ai_table.insert_or_assign("writing_coach", writing_coach_table);
      
      // AI Smart Content Generation configuration (Phase 4)
      auto smart_content_generation_table = toml::table{};
      smart_content_generation_table.insert_or_assign("enabled", ai->smart_content_generation.enabled);
      smart_content_generation_table.insert_or_assign("max_tokens", ai->smart_content_generation.max_tokens);
      smart_content_generation_table.insert_or_assign("temperature", ai->smart_content_generation.temperature);
      smart_content_generation_table.insert_or_assign("timeout_ms", ai->smart_content_generation.timeout_ms);
      smart_content_generation_table.insert_or_assign("content_style", ai->smart_content_generation.content_style);
      smart_content_generation_table.insert_or_assign("max_content_length", static_cast<int>(ai->smart_content_generation.max_content_length));
      smart_content_generation_table.insert_or_assign("include_outline", ai->smart_content_generation.include_outline);
      ai_table.insert_or_assign("smart_content_generation", smart_content_generation_table);
      
      // AI Intelligent Templates configuration (Phase 4)
      auto intelligent_templates_table = toml::table{};
      intelligent_templates_table.insert_or_assign("enabled", ai->intelligent_templates.enabled);
      intelligent_templates_table.insert_or_assign("max_tokens", ai->intelligent_templates.max_tokens);
      intelligent_templates_table.insert_or_assign("temperature", ai->intelligent_templates.temperature);
      intelligent_templates_table.insert_or_assign("timeout_ms", ai->intelligent_templates.timeout_ms);
      intelligent_templates_table.insert_or_assign("max_suggestions", static_cast<int>(ai->intelligent_templates.max_suggestions));
      intelligent_templates_table.insert_or_assign("analyze_existing_content", ai->intelligent_templates.analyze_existing_content);
      ai_table.insert_or_assign("intelligent_templates", intelligent_templates_table);
      
      // AI Cross-Note Insights configuration (Phase 4)
      auto cross_note_insights_table = toml::table{};
      cross_note_insights_table.insert_or_assign("enabled", ai->cross_note_insights.enabled);
      cross_note_insights_table.insert_or_assign("max_tokens", ai->cross_note_insights.max_tokens);
      cross_note_insights_table.insert_or_assign("temperature", ai->cross_note_insights.temperature);
      cross_note_insights_table.insert_or_assign("timeout_ms", ai->cross_note_insights.timeout_ms);
      cross_note_insights_table.insert_or_assign("max_notes_analyzed", static_cast<int>(ai->cross_note_insights.max_notes_analyzed));
      cross_note_insights_table.insert_or_assign("insight_focus", ai->cross_note_insights.insight_focus);
      ai_table.insert_or_assign("cross_note_insights", cross_note_insights_table);
      
      // AI Smart Search Enhancement configuration (Phase 4)
      auto smart_search_enhancement_table = toml::table{};
      smart_search_enhancement_table.insert_or_assign("enabled", ai->smart_search_enhancement.enabled);
      smart_search_enhancement_table.insert_or_assign("max_tokens", ai->smart_search_enhancement.max_tokens);
      smart_search_enhancement_table.insert_or_assign("temperature", ai->smart_search_enhancement.temperature);
      smart_search_enhancement_table.insert_or_assign("timeout_ms", ai->smart_search_enhancement.timeout_ms);
      smart_search_enhancement_table.insert_or_assign("expand_synonyms", ai->smart_search_enhancement.expand_synonyms);
      smart_search_enhancement_table.insert_or_assign("analyze_intent", ai->smart_search_enhancement.analyze_intent);
      ai_table.insert_or_assign("smart_search_enhancement", smart_search_enhancement_table);
      
      // AI Smart Note Merging configuration (Phase 4)
      auto smart_note_merging_table = toml::table{};
      smart_note_merging_table.insert_or_assign("enabled", ai->smart_note_merging.enabled);
      smart_note_merging_table.insert_or_assign("max_tokens", ai->smart_note_merging.max_tokens);
      smart_note_merging_table.insert_or_assign("temperature", ai->smart_note_merging.temperature);
      smart_note_merging_table.insert_or_assign("timeout_ms", ai->smart_note_merging.timeout_ms);
      smart_note_merging_table.insert_or_assign("similarity_threshold", ai->smart_note_merging.similarity_threshold);
      smart_note_merging_table.insert_or_assign("max_merge_candidates", static_cast<int>(ai->smart_note_merging.max_merge_candidates));
      ai_table.insert_or_assign("smart_note_merging", smart_note_merging_table);
      
      // Phase 5 AI configuration serialization
      
      // AI Workflow Orchestrator configuration (Phase 5)
      auto workflow_orchestrator_table = toml::table{};
      workflow_orchestrator_table.insert_or_assign("enabled", ai->workflow_orchestrator.enabled);
      workflow_orchestrator_table.insert_or_assign("max_tokens", ai->workflow_orchestrator.max_tokens);
      workflow_orchestrator_table.insert_or_assign("temperature", ai->workflow_orchestrator.temperature);
      workflow_orchestrator_table.insert_or_assign("timeout_per_step_ms", ai->workflow_orchestrator.timeout_per_step_ms);
      workflow_orchestrator_table.insert_or_assign("max_steps", ai->workflow_orchestrator.max_steps);
      workflow_orchestrator_table.insert_or_assign("allow_recursive_calls", ai->workflow_orchestrator.allow_recursive_calls);
      auto allowed_operations_array = toml::array{};
      for (const auto& op : ai->workflow_orchestrator.allowed_operations) {
        allowed_operations_array.push_back(op);
      }
      workflow_orchestrator_table.insert_or_assign("allowed_operations", allowed_operations_array);
      ai_table.insert_or_assign("workflow_orchestrator", workflow_orchestrator_table);
      
      // AI Project Assistant configuration (Phase 5)  
      auto project_assistant_table = toml::table{};
      project_assistant_table.insert_or_assign("enabled", ai->project_assistant.enabled);
      project_assistant_table.insert_or_assign("max_tokens", ai->project_assistant.max_tokens);
      project_assistant_table.insert_or_assign("temperature", ai->project_assistant.temperature);
      project_assistant_table.insert_or_assign("timeout_ms", ai->project_assistant.timeout_ms);
      project_assistant_table.insert_or_assign("project_scope", ai->project_assistant.project_scope);
      project_assistant_table.insert_or_assign("auto_generate_milestones", ai->project_assistant.auto_generate_milestones);
      project_assistant_table.insert_or_assign("max_related_notes", ai->project_assistant.max_related_notes);
      ai_table.insert_or_assign("project_assistant", project_assistant_table);
      
      // AI Learning Path Generator configuration (Phase 5)
      auto learning_path_generator_table = toml::table{};
      learning_path_generator_table.insert_or_assign("enabled", ai->learning_path_generator.enabled);
      learning_path_generator_table.insert_or_assign("max_tokens", ai->learning_path_generator.max_tokens);
      learning_path_generator_table.insert_or_assign("temperature", ai->learning_path_generator.temperature);
      learning_path_generator_table.insert_or_assign("timeout_ms", ai->learning_path_generator.timeout_ms);
      learning_path_generator_table.insert_or_assign("max_prerequisites", ai->learning_path_generator.max_prerequisites);
      learning_path_generator_table.insert_or_assign("max_learning_steps", ai->learning_path_generator.max_learning_steps);
      learning_path_generator_table.insert_or_assign("include_resources", ai->learning_path_generator.include_resources);
      ai_table.insert_or_assign("learning_path_generator", learning_path_generator_table);
      
      // AI Knowledge Synthesis configuration (Phase 5)
      auto knowledge_synthesis_table = toml::table{};
      knowledge_synthesis_table.insert_or_assign("enabled", ai->knowledge_synthesis.enabled);
      knowledge_synthesis_table.insert_or_assign("max_tokens", ai->knowledge_synthesis.max_tokens);
      knowledge_synthesis_table.insert_or_assign("temperature", ai->knowledge_synthesis.temperature);
      knowledge_synthesis_table.insert_or_assign("timeout_ms", ai->knowledge_synthesis.timeout_ms);
      knowledge_synthesis_table.insert_or_assign("max_source_notes", ai->knowledge_synthesis.max_source_notes);
      knowledge_synthesis_table.insert_or_assign("detect_contradictions", ai->knowledge_synthesis.detect_contradictions);
      knowledge_synthesis_table.insert_or_assign("suggest_gaps", ai->knowledge_synthesis.suggest_gaps);
      ai_table.insert_or_assign("knowledge_synthesis", knowledge_synthesis_table);
      
      // AI Journal Insights configuration (Phase 5)
      auto journal_insights_table = toml::table{};
      journal_insights_table.insert_or_assign("enabled", ai->journal_insights.enabled);
      journal_insights_table.insert_or_assign("max_tokens", ai->journal_insights.max_tokens);
      journal_insights_table.insert_or_assign("temperature", ai->journal_insights.temperature);
      journal_insights_table.insert_or_assign("timeout_ms", ai->journal_insights.timeout_ms);
      journal_insights_table.insert_or_assign("analysis_window_days", ai->journal_insights.analysis_window_days);
      journal_insights_table.insert_or_assign("track_mood_patterns", ai->journal_insights.track_mood_patterns);
      journal_insights_table.insert_or_assign("track_productivity_patterns", ai->journal_insights.track_productivity_patterns);
      journal_insights_table.insert_or_assign("suggest_habit_changes", ai->journal_insights.suggest_habit_changes);
      ai_table.insert_or_assign("journal_insights", journal_insights_table);

      // Phase 6 AI configuration serialization
      
      // Multi-modal AI configuration (Phase 6)
      auto multi_modal_table = toml::table{};
      multi_modal_table.insert_or_assign("enabled", ai->multi_modal.enabled);
      multi_modal_table.insert_or_assign("max_tokens", ai->multi_modal.max_tokens);
      multi_modal_table.insert_or_assign("temperature", ai->multi_modal.temperature);
      multi_modal_table.insert_or_assign("timeout_ms", ai->multi_modal.timeout_ms);
      multi_modal_table.insert_or_assign("analyze_images", ai->multi_modal.analyze_images);
      multi_modal_table.insert_or_assign("generate_alt_text", ai->multi_modal.generate_alt_text);
      multi_modal_table.insert_or_assign("extract_text_from_images", ai->multi_modal.extract_text_from_images);
      multi_modal_table.insert_or_assign("analyze_document_structure", ai->multi_modal.analyze_document_structure);
      ai_table.insert_or_assign("multi_modal", multi_modal_table);
      
      // Voice integration configuration (Phase 6)
      auto voice_integration_table = toml::table{};
      voice_integration_table.insert_or_assign("enabled", ai->voice_integration.enabled);
      voice_integration_table.insert_or_assign("max_tokens", ai->voice_integration.max_tokens);
      voice_integration_table.insert_or_assign("temperature", ai->voice_integration.temperature);
      voice_integration_table.insert_or_assign("timeout_ms", ai->voice_integration.timeout_ms);
      voice_integration_table.insert_or_assign("tts_voice", ai->voice_integration.tts_voice);
      voice_integration_table.insert_or_assign("speech_language", ai->voice_integration.speech_language);
      voice_integration_table.insert_or_assign("auto_punctuation", ai->voice_integration.auto_punctuation);
      voice_integration_table.insert_or_assign("background_listening", ai->voice_integration.background_listening);
      ai_table.insert_or_assign("voice_integration", voice_integration_table);
      
      // Context awareness configuration (Phase 6)
      auto context_awareness_table = toml::table{};
      context_awareness_table.insert_or_assign("enabled", ai->context_awareness.enabled);
      context_awareness_table.insert_or_assign("max_tokens", ai->context_awareness.max_tokens);
      context_awareness_table.insert_or_assign("temperature", ai->context_awareness.temperature);
      context_awareness_table.insert_or_assign("timeout_ms", ai->context_awareness.timeout_ms);
      context_awareness_table.insert_or_assign("context_window_notes", ai->context_awareness.context_window_notes);
      context_awareness_table.insert_or_assign("track_reading_patterns", ai->context_awareness.track_reading_patterns);
      context_awareness_table.insert_or_assign("predict_next_actions", ai->context_awareness.predict_next_actions);
      context_awareness_table.insert_or_assign("suggest_related_content", ai->context_awareness.suggest_related_content);
      context_awareness_table.insert_or_assign("adaptive_interface", ai->context_awareness.adaptive_interface);
      ai_table.insert_or_assign("context_awareness", context_awareness_table);
      
      // Workspace AI configuration (Phase 6)
      auto workspace_ai_table = toml::table{};
      workspace_ai_table.insert_or_assign("enabled", ai->workspace_ai.enabled);
      workspace_ai_table.insert_or_assign("max_tokens", ai->workspace_ai.max_tokens);
      workspace_ai_table.insert_or_assign("temperature", ai->workspace_ai.temperature);
      workspace_ai_table.insert_or_assign("timeout_ms", ai->workspace_ai.timeout_ms);
      workspace_ai_table.insert_or_assign("smart_folder_suggestions", ai->workspace_ai.smart_folder_suggestions);
      workspace_ai_table.insert_or_assign("auto_tag_relationships", ai->workspace_ai.auto_tag_relationships);
      workspace_ai_table.insert_or_assign("workspace_health_monitoring", ai->workspace_ai.workspace_health_monitoring);
      workspace_ai_table.insert_or_assign("smart_archive_suggestions", ai->workspace_ai.smart_archive_suggestions);
      ai_table.insert_or_assign("workspace_ai", workspace_ai_table);
      
      // Predictive AI configuration (Phase 6)
      auto predictive_ai_table = toml::table{};
      predictive_ai_table.insert_or_assign("enabled", ai->predictive_ai.enabled);
      predictive_ai_table.insert_or_assign("max_tokens", ai->predictive_ai.max_tokens);
      predictive_ai_table.insert_or_assign("temperature", ai->predictive_ai.temperature);
      predictive_ai_table.insert_or_assign("timeout_ms", ai->predictive_ai.timeout_ms);
      predictive_ai_table.insert_or_assign("predict_note_needs", ai->predictive_ai.predict_note_needs);
      predictive_ai_table.insert_or_assign("suggest_meeting_prep", ai->predictive_ai.suggest_meeting_prep);
      predictive_ai_table.insert_or_assign("proactive_reminders", ai->predictive_ai.proactive_reminders);
      predictive_ai_table.insert_or_assign("learning_path_optimization", ai->predictive_ai.learning_path_optimization);
      ai_table.insert_or_assign("predictive_ai", predictive_ai_table);

      // Phase 7 AI configuration serialization
      
      // Collaborative AI configuration (Phase 7)
      auto collaborative_ai_table = toml::table{};
      collaborative_ai_table.insert_or_assign("enabled", ai->collaborative_ai.enabled);
      collaborative_ai_table.insert_or_assign("max_tokens", ai->collaborative_ai.max_tokens);
      collaborative_ai_table.insert_or_assign("temperature", ai->collaborative_ai.temperature);
      collaborative_ai_table.insert_or_assign("timeout_ms", ai->collaborative_ai.timeout_ms);
      collaborative_ai_table.insert_or_assign("enable_shared_sessions", ai->collaborative_ai.enable_shared_sessions);
      collaborative_ai_table.insert_or_assign("cross_reference_analysis", ai->collaborative_ai.cross_reference_analysis);
      collaborative_ai_table.insert_or_assign("collaborative_editing", ai->collaborative_ai.collaborative_editing);
      collaborative_ai_table.insert_or_assign("consensus_building", ai->collaborative_ai.consensus_building);
      ai_table.insert_or_assign("collaborative_ai", collaborative_ai_table);
      
      // Knowledge graph configuration (Phase 7)
      auto knowledge_graph_table = toml::table{};
      knowledge_graph_table.insert_or_assign("enabled", ai->knowledge_graph.enabled);
      knowledge_graph_table.insert_or_assign("max_tokens", ai->knowledge_graph.max_tokens);
      knowledge_graph_table.insert_or_assign("temperature", ai->knowledge_graph.temperature);
      knowledge_graph_table.insert_or_assign("timeout_ms", ai->knowledge_graph.timeout_ms);
      knowledge_graph_table.insert_or_assign("auto_generate_graphs", ai->knowledge_graph.auto_generate_graphs);
      knowledge_graph_table.insert_or_assign("semantic_clustering", ai->knowledge_graph.semantic_clustering);
      knowledge_graph_table.insert_or_assign("relationship_inference", ai->knowledge_graph.relationship_inference);
      knowledge_graph_table.insert_or_assign("visual_graph_export", ai->knowledge_graph.visual_graph_export);
      ai_table.insert_or_assign("knowledge_graph", knowledge_graph_table);
      
      // Expert systems configuration (Phase 7)
      auto expert_systems_table = toml::table{};
      expert_systems_table.insert_or_assign("enabled", ai->expert_systems.enabled);
      expert_systems_table.insert_or_assign("max_tokens", ai->expert_systems.max_tokens);
      expert_systems_table.insert_or_assign("temperature", ai->expert_systems.temperature);
      expert_systems_table.insert_or_assign("timeout_ms", ai->expert_systems.timeout_ms);
      expert_systems_table.insert_or_assign("primary_domain", ai->expert_systems.primary_domain);
      expert_systems_table.insert_or_assign("multi_domain_support", ai->expert_systems.multi_domain_support);
      expert_systems_table.insert_or_assign("adaptive_expertise", ai->expert_systems.adaptive_expertise);
      expert_systems_table.insert_or_assign("citation_generation", ai->expert_systems.citation_generation);
      ai_table.insert_or_assign("expert_systems", expert_systems_table);
      
      // Intelligent workflows configuration (Phase 7)
      auto intelligent_workflows_table = toml::table{};
      intelligent_workflows_table.insert_or_assign("enabled", ai->intelligent_workflows.enabled);
      intelligent_workflows_table.insert_or_assign("max_tokens", ai->intelligent_workflows.max_tokens);
      intelligent_workflows_table.insert_or_assign("temperature", ai->intelligent_workflows.temperature);
      intelligent_workflows_table.insert_or_assign("timeout_ms", ai->intelligent_workflows.timeout_ms);
      intelligent_workflows_table.insert_or_assign("auto_workflow_detection", ai->intelligent_workflows.auto_workflow_detection);
      intelligent_workflows_table.insert_or_assign("process_optimization", ai->intelligent_workflows.process_optimization);
      intelligent_workflows_table.insert_or_assign("deadline_management", ai->intelligent_workflows.deadline_management);
      intelligent_workflows_table.insert_or_assign("resource_allocation", ai->intelligent_workflows.resource_allocation);
      ai_table.insert_or_assign("intelligent_workflows", intelligent_workflows_table);
      
      // Meta-learning configuration (Phase 7)
      auto meta_learning_table = toml::table{};
      meta_learning_table.insert_or_assign("enabled", ai->meta_learning.enabled);
      meta_learning_table.insert_or_assign("max_tokens", ai->meta_learning.max_tokens);
      meta_learning_table.insert_or_assign("temperature", ai->meta_learning.temperature);
      meta_learning_table.insert_or_assign("timeout_ms", ai->meta_learning.timeout_ms);
      meta_learning_table.insert_or_assign("user_pattern_learning", ai->meta_learning.user_pattern_learning);
      meta_learning_table.insert_or_assign("adaptive_assistance", ai->meta_learning.adaptive_assistance);
      meta_learning_table.insert_or_assign("personalization", ai->meta_learning.personalization);
      meta_learning_table.insert_or_assign("learning_analytics", ai->meta_learning.learning_analytics);
      ai_table.insert_or_assign("meta_learning", meta_learning_table);
      
      config_data.insert_or_assign("ai", ai_table);
    }
    
    // Performance configuration
    auto perf_table = toml::table{};
    perf_table.insert_or_assign("cache_size_mb", static_cast<int>(performance.cache_size_mb));
    perf_table.insert_or_assign("max_file_size_mb", static_cast<int>(performance.max_file_size_mb));
    perf_table.insert_or_assign("sqlite_cache_size", performance.sqlite_cache_size);
    perf_table.insert_or_assign("sqlite_journal_mode", performance.sqlite_journal_mode);
    perf_table.insert_or_assign("sqlite_synchronous", performance.sqlite_synchronous);
    perf_table.insert_or_assign("sqlite_temp_store", performance.sqlite_temp_store);
    config_data.insert_or_assign("performance", perf_table);
    
    // Ensure parent directory exists
    auto parent = save_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
      std::filesystem::create_directories(parent);
    }
    
    // Write to file atomically
    std::stringstream ss;
    ss << config_data;
    auto write_result = nx::util::FileSystem::writeFileAtomic(save_path, ss.str());
    if (!write_result.has_value()) {
      return std::unexpected(makeError(ErrorCode::kConfigError, 
                                       "Cannot write config file: " + write_result.error().message()));
    }
    
    return {};
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "Config save error: " + std::string(e.what())));
  }
}

Result<std::string> Config::get(const std::string& key) const {
  auto path = splitPath(key);
  return getValueByPath(path);
}

Result<void> Config::set(const std::string& key, const std::string& value) {
  auto path = splitPath(key);
  return setValueByPath(path, value);
}

Result<void> Config::validate() const {
  // Validate paths exist if specified
  if (!notes_dir.empty() && !std::filesystem::exists(notes_dir)) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "Notes directory does not exist: " + notes_dir.string()));
  }
  
  // Validate AI configuration
  if (ai.has_value()) {
    if (ai->provider != "openai" && ai->provider != "anthropic") {
      return std::unexpected(makeError(ErrorCode::kConfigError, 
                                       "Invalid AI provider: " + ai->provider));
    }
    
    if (ai->api_key.empty()) {
      return std::unexpected(makeError(ErrorCode::kConfigError, 
                                       "AI API key not configured"));
    }
    
    if (ai->max_tokens <= 0 || ai->max_tokens > 32000) {
      return std::unexpected(makeError(ErrorCode::kConfigError, 
                                       "Invalid max_tokens value"));
    }
  }
  
  return {};
}

std::filesystem::path Config::defaultConfigPath() {
  return nx::util::Xdg::configFile();
}

Config Config::createDefault() {
  Config config{};  // Create with default initialization
  
  // Set default paths
  config.root = nx::util::Xdg::dataHome();
  config.notes_dir = nx::util::Xdg::notesDir();
  config.attachments_dir = nx::util::Xdg::attachmentsDir();
  config.trash_dir = nx::util::Xdg::trashDir();
  config.index_file = nx::util::Xdg::indexFile();
  
  // Set default editor
  config.editor = std::getenv("VISUAL") ? std::getenv("VISUAL") :
                  (std::getenv("EDITOR") ? std::getenv("EDITOR") : "vi");
  
  // Set default values
  config.indexer = IndexerType::kFts;
  config.encryption = EncryptionType::kNone;
  config.sync = SyncType::kNone;
  
  return config;
}

std::string Config::resolveEnvVar(const std::string& value) const {
  if (value.substr(0, 4) == "env:") {
    std::string var_name = value.substr(4);
    const char* env_value = std::getenv(var_name.c_str());
    return env_value ? std::string(env_value) : "";
  }
  return value;
}

// Enum conversion methods
std::string Config::indexerTypeToString(IndexerType type) {
  switch (type) {
    case IndexerType::kFts: return "fts";
    case IndexerType::kRipgrep: return "ripgrep";
  }
  return "fts";
}

Config::IndexerType Config::stringToIndexerType(const std::string& str) {
  if (str == "ripgrep") return IndexerType::kRipgrep;
  return IndexerType::kFts;
}

std::string Config::encryptionTypeToString(EncryptionType type) {
  switch (type) {
    case EncryptionType::kNone: return "none";
    case EncryptionType::kAge: return "age";
  }
  return "none";
}

Config::EncryptionType Config::stringToEncryptionType(const std::string& str) {
  if (str == "age") return EncryptionType::kAge;
  return EncryptionType::kNone;
}

std::string Config::syncTypeToString(SyncType type) {
  switch (type) {
    case SyncType::kNone: return "none";
    case SyncType::kGit: return "git";
  }
  return "none";
}

Config::SyncType Config::stringToSyncType(const std::string& str) {
  if (str == "git") return SyncType::kGit;
  return SyncType::kNone;
}

Result<std::string> Config::getValueByPath(const std::vector<std::string>& path) const {
  if (path.empty()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, "Empty config path"));
  }
  
  // Handle specific paths
  if (path.size() == 1) {
    const std::string& key = path[0];
    
    if (key == "root") return root.string();
    if (key == "notes_dir") return notes_dir.string();
    if (key == "attachments_dir") return attachments_dir.string();
    if (key == "trash_dir") return trash_dir.string();
    if (key == "index_file") return index_file.string();
    if (key == "editor") return editor;
    if (key == "indexer") return indexerTypeToString(indexer);
    if (key == "encryption") return encryptionTypeToString(encryption);
    if (key == "age_recipient") return age_recipient;
    if (key == "sync") return syncTypeToString(sync);
    if (key == "git_remote") return git_remote;
    if (key == "git_user_name") return git_user_name;
    if (key == "git_user_email") return git_user_email;
  } else if (path.size() == 2) {
    if (path[0] == "defaults") {
      if (path[1] == "notebook") return default_notebook;
    }
  }
  
  return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Unknown config key: " + path[0]));
}

Result<void> Config::setValueByPath(const std::vector<std::string>& path, const std::string& value) {
  if (path.empty()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, "Empty config path"));
  }
  
  // Handle specific paths
  if (path.size() == 1) {
    const std::string& key = path[0];
    
    if (key == "root") { root = value; return {}; }
    if (key == "notes_dir") { notes_dir = value; return {}; }
    if (key == "attachments_dir") { attachments_dir = value; return {}; }
    if (key == "trash_dir") { trash_dir = value; return {}; }
    if (key == "index_file") { index_file = value; return {}; }
    if (key == "editor") { editor = value; return {}; }
    if (key == "indexer") { indexer = stringToIndexerType(value); return {}; }
    if (key == "encryption") { encryption = stringToEncryptionType(value); return {}; }
    if (key == "age_recipient") { age_recipient = value; return {}; }
    if (key == "sync") { sync = stringToSyncType(value); return {}; }
    if (key == "git_remote") { git_remote = value; return {}; }
    if (key == "git_user_name") { git_user_name = value; return {}; }
    if (key == "git_user_email") { git_user_email = value; return {}; }
  } else if (path.size() == 2) {
    if (path[0] == "defaults") {
      if (path[1] == "notebook") { default_notebook = value; return {}; }
    }
  }
  
  return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Unknown config key: " + path[0]));
}

std::vector<std::string> Config::splitPath(const std::string& path) const {
  std::vector<std::string> parts;
  std::istringstream stream(path);
  std::string part;
  
  while (std::getline(stream, part, '.')) {
    if (!part.empty()) {
      parts.push_back(part);
    }
  }
  
  return parts;
}

}  // namespace nx::config