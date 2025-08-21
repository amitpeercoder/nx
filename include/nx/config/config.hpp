#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

#include "nx/common.hpp"

namespace nx::config {

// Configuration for nx application
class Config {
 public:
  // Default constructor loads from default config file
  Config();
  
  // Load from specific file
  explicit Config(const std::filesystem::path& config_path);
  
  // Core paths
  std::filesystem::path root;
  std::filesystem::path data_dir;
  std::filesystem::path notes_dir;
  std::filesystem::path attachments_dir;
  std::filesystem::path trash_dir;
  std::filesystem::path index_file;
  
  // Editor configuration
  std::string editor;
  
  // Indexing configuration
  enum class IndexerType {
    kFts,      // SQLite FTS5
    kRipgrep   // Fallback to ripgrep
  };
  IndexerType indexer = IndexerType::kFts;
  
  // Encryption configuration
  enum class EncryptionType {
    kNone,
    kAge
  };
  EncryptionType encryption = EncryptionType::kNone;
  std::string age_recipient;
  
  // Sync configuration
  enum class SyncType {
    kNone,
    kGit
  };
  SyncType sync = SyncType::kNone;
  std::string git_remote;
  std::string git_user_name;
  std::string git_user_email;
  
  // Auto-sync configuration
  struct AutoSyncConfig {
    bool enabled = false;                    // Enable automatic sync
    bool auto_pull_on_startup = true;        // Pull on app startup
    bool auto_push_on_changes = true;        // Push after changes
    int auto_push_delay_seconds = 300;       // Delay before auto-push
    int sync_interval_seconds = 1800;        // Background sync interval
    std::string conflict_strategy = "manual"; // manual, ours, theirs, smart
    int max_auto_resolve_attempts = 3;       // Max attempts before manual
    bool sync_on_shutdown = true;            // Sync before app exit
    bool show_sync_status = true;            // Show sync status in TUI
  };
  AutoSyncConfig auto_sync;
  
  // Default values
  std::string default_notebook;
  std::vector<std::string> default_tags;
  
  // AI configuration (optional)
  struct AiConfig {
    std::string provider;          // "openai" or "anthropic"
    std::string model;
    std::string api_key;           // Can be "env:VARNAME" reference
    int max_tokens = 1200;
    double temperature = 0.2;
    int rate_limit_qpm = 20;
    double daily_usd_budget = 1.50;
    bool enable_embeddings = true;
    std::string embedding_model;
    int top_k = 6;
    
    // Redaction settings
    bool strip_emails = true;
    bool strip_urls = false;
    bool mask_numbers = true;
    
    // AI Explanation settings
    struct ExplanationConfig {
      bool enabled = true;                     // Enable AI explanations
      size_t brief_max_words = 10;            // Maximum words in brief explanation
      size_t expanded_max_words = 50;         // Maximum words in expanded explanation
      int timeout_ms = 3000;                  // Timeout for AI requests
      bool cache_explanations = true;         // Whether to cache explanations
      size_t max_cache_size = 1000;           // Maximum cached explanations
      size_t context_radius = 100;            // Characters around term for context
    };
    ExplanationConfig explanations;
    
    // AI Smart Completion settings
    struct SmartCompletionConfig {
      bool enabled = false;                     // Enable smart completion (disabled by default)
      int max_tokens = 150;                     // Maximum tokens for completion
      double temperature = 0.3;                // Temperature for completion (slightly higher for creativity)
      size_t max_completion_length = 300;      // Maximum completion length in characters
      int timeout_ms = 2000;                   // Timeout for completion requests (faster than explanations)
    };
    SmartCompletionConfig smart_completion;
    
    // AI Semantic Search settings
    struct SemanticSearchConfig {
      bool enabled = false;                     // Enable semantic search (disabled by default)
      int max_tokens = 500;                     // Maximum tokens for search analysis
      double temperature = 0.1;                // Low temperature for consistent results
      int timeout_ms = 5000;                   // Timeout for search requests
      size_t max_notes_per_query = 50;         // Maximum notes to include in search prompt
    };
    SemanticSearchConfig semantic_search;
    
    // AI Grammar & Style Check settings
    struct GrammarStyleCheckConfig {
      bool enabled = false;                     // Enable grammar & style check (disabled by default)
      int max_tokens = 800;                     // Maximum tokens for style analysis
      double temperature = 0.2;                // Low temperature for consistent suggestions
      int timeout_ms = 4000;                   // Timeout for grammar check requests
      size_t max_text_length = 2000;           // Maximum text length to analyze
      std::string style = "clear";             // Style preference: "clear", "formal", "casual", "academic"
    };
    GrammarStyleCheckConfig grammar_style_check;
    
    // AI Smart Examples settings
    struct SmartExamplesConfig {
      bool enabled = false;                     // Enable smart examples (disabled by default)
      int max_tokens = 600;                     // Maximum tokens for example generation
      double temperature = 0.4;                // Moderate temperature for creative examples
      int timeout_ms = 3500;                   // Timeout for example generation
      size_t max_examples = 3;                 // Maximum number of examples to generate
      std::string example_type = "practical";  // Example type: "practical", "simple", "advanced", "real-world"
    };
    SmartExamplesConfig smart_examples;
    
    // AI Code Generation settings
    struct CodeGenerationConfig {
      bool enabled = false;                     // Enable code generation (disabled by default)
      int max_tokens = 1000;                    // Maximum tokens for code generation
      double temperature = 0.3;                // Moderate temperature for code creativity
      int timeout_ms = 5000;                   // Timeout for code generation requests
      std::string language = "auto";           // Programming language: "auto", "python", "javascript", "cpp", etc.
      std::string style = "clean";             // Code style: "clean", "commented", "minimal", "verbose"
    };
    CodeGenerationConfig code_generation;
    
    // AI Smart Summarization settings
    struct SmartSummarizationConfig {
      bool enabled = false;                     // Enable smart summarization (disabled by default)
      int max_tokens = 800;                     // Maximum tokens for summarization
      double temperature = 0.2;                // Low temperature for consistent summaries
      int timeout_ms = 6000;                   // Timeout for summarization requests
      size_t max_text_length = 5000;           // Maximum text length to summarize
      std::string style = "bullet";            // Summary style: "bullet", "paragraph", "outline", "key-points"
      bool include_metadata = true;            // Include note metadata in summary context
    };
    SmartSummarizationConfig smart_summarization;
    
    // AI Note Relationships settings
    struct NoteRelationshipsConfig {
      bool enabled = false;                     // Enable note relationships analysis (disabled by default)
      int max_tokens = 600;                     // Maximum tokens for relationship analysis
      double temperature = 0.1;                // Very low temperature for consistent analysis
      int timeout_ms = 4000;                   // Timeout for relationship requests
      size_t max_notes_to_analyze = 20;        // Maximum notes to consider for relationships
      double similarity_threshold = 0.7;       // Minimum similarity score for relationships
    };
    NoteRelationshipsConfig note_relationships;
    
    // AI Smart Organization settings  
    struct SmartOrganizationConfig {
      bool enabled = false;                     // Enable smart organization (disabled by default)
      int max_tokens = 800;                     // Maximum tokens for organization analysis
      double temperature = 0.2;                // Low temperature for consistent categorization
      int timeout_ms = 5000;                   // Timeout for organization requests
      size_t max_notes_per_batch = 30;         // Maximum notes to analyze per batch
      bool suggest_new_notebooks = true;       // Whether to suggest new notebook creation
    };
    SmartOrganizationConfig smart_organization;
    
    // AI Content Enhancement settings
    struct ContentEnhancementConfig {
      bool enabled = false;                     // Enable content enhancement (disabled by default)
      int max_tokens = 900;                     // Maximum tokens for enhancement suggestions
      double temperature = 0.3;                // Moderate temperature for creative suggestions
      int timeout_ms = 6000;                   // Timeout for enhancement requests
      size_t max_text_length = 3000;           // Maximum text length to enhance
      std::string enhancement_focus = "clarity"; // Focus: "clarity", "depth", "structure", "engagement"
    };
    ContentEnhancementConfig content_enhancement;
    
    // AI Research Assistant settings
    struct ResearchAssistantConfig {
      bool enabled = false;                     // Enable research assistant (disabled by default)
      int max_tokens = 700;                     // Maximum tokens for research suggestions
      double temperature = 0.4;                // Higher temperature for creative research ideas
      int timeout_ms = 5000;                   // Timeout for research requests
      size_t max_topics_generated = 5;         // Maximum research topics to generate
      std::string research_style = "academic"; // Style: "academic", "practical", "exploratory", "critical"
    };
    ResearchAssistantConfig research_assistant;
    
    // AI Writing Coach settings
    struct WritingCoachConfig {
      bool enabled = false;                     // Enable writing coach (disabled by default)
      int max_tokens = 1000;                    // Maximum tokens for writing analysis
      double temperature = 0.2;                // Low temperature for consistent feedback
      int timeout_ms = 7000;                   // Timeout for writing analysis
      size_t max_text_length = 4000;           // Maximum text length to analyze
      std::string feedback_level = "comprehensive"; // Level: "basic", "detailed", "comprehensive"
      bool include_style_suggestions = true;   // Include style and tone suggestions
    };
    WritingCoachConfig writing_coach;
    
    // AI Smart Content Generation settings (Phase 4)
    struct SmartContentGenerationConfig {
      bool enabled = false;                     // Enable smart content generation (disabled by default)
      int max_tokens = 1500;                    // Maximum tokens for content generation
      double temperature = 0.6;                // Higher temperature for creative content
      int timeout_ms = 8000;                   // Timeout for content generation
      std::string content_style = "informative"; // Style: "informative", "creative", "technical", "casual"
      size_t max_content_length = 2000;        // Maximum generated content length
      bool include_outline = true;             // Include outline in generated content
    };
    SmartContentGenerationConfig smart_content_generation;
    
    // AI Intelligent Template Suggestions settings (Phase 4)
    struct IntelligentTemplateConfig {
      bool enabled = false;                     // Enable intelligent template suggestions (disabled by default)
      int max_tokens = 400;                     // Maximum tokens for template analysis
      double temperature = 0.1;                // Very low temperature for consistent suggestions
      int timeout_ms = 3000;                   // Timeout for template suggestions
      size_t max_suggestions = 5;              // Maximum template suggestions to generate
      bool analyze_existing_content = true;    // Analyze existing note content for suggestions
    };
    IntelligentTemplateConfig intelligent_templates;
    
    // AI Cross-Note Insights settings (Phase 4)
    struct CrossNoteInsightsConfig {
      bool enabled = false;                     // Enable cross-note insights (disabled by default)
      int max_tokens = 1200;                    // Maximum tokens for insights analysis
      double temperature = 0.3;                // Moderate temperature for balanced insights
      int timeout_ms = 10000;                  // Timeout for cross-note analysis
      size_t max_notes_analyzed = 50;          // Maximum notes to analyze for insights
      std::string insight_focus = "patterns";  // Focus: "patterns", "gaps", "connections", "themes"
    };
    CrossNoteInsightsConfig cross_note_insights;
    
    // AI Smart Search Enhancement settings (Phase 4)
    struct SmartSearchEnhancementConfig {
      bool enabled = false;                     // Enable smart search enhancement (disabled by default)
      int max_tokens = 300;                     // Maximum tokens for search query enhancement
      double temperature = 0.2;                // Low temperature for consistent query interpretation
      int timeout_ms = 2500;                   // Timeout for search enhancement
      bool expand_synonyms = true;             // Include synonyms in search expansion
      bool analyze_intent = true;              // Analyze search intent for better results
    };
    SmartSearchEnhancementConfig smart_search_enhancement;
    
    // AI Smart Note Merging settings (Phase 4)
    struct SmartNoteMergingConfig {
      bool enabled = false;                     // Enable smart note merging (disabled by default)
      int max_tokens = 800;                     // Maximum tokens for merge analysis
      double temperature = 0.1;                // Very low temperature for consistent analysis
      int timeout_ms = 6000;                   // Timeout for merge analysis
      double similarity_threshold = 0.8;       // Minimum similarity for merge suggestions
      size_t max_merge_candidates = 10;        // Maximum notes to consider for merging
    };
    SmartNoteMergingConfig smart_note_merging;
    
    // AI Workflow Orchestrator settings (Phase 5)
    struct WorkflowOrchestratorConfig {
      bool enabled = false;                         // Enable workflow orchestration (disabled by default)
      int max_tokens = 1500;                       // Maximum tokens per workflow step
      double temperature = 0.2;                    // Low temperature for consistent workflows
      int timeout_per_step_ms = 15000;             // Timeout per workflow step
      int max_steps = 10;                          // Maximum steps in a workflow
      bool allow_recursive_calls = false;          // Allow workflows to call other workflows
      std::vector<std::string> allowed_operations = {"summarize", "tag", "title", "enhance"};  // Operations workflows can perform
    };
    WorkflowOrchestratorConfig workflow_orchestrator;
    
    // AI Project Assistant settings (Phase 5)
    struct ProjectAssistantConfig {
      bool enabled = false;                         // Enable project assistant (disabled by default)  
      int max_tokens = 2048;                       // Maximum tokens for project analysis
      double temperature = 0.3;                    // Medium temperature for creative project insights
      int timeout_ms = 30000;                      // Timeout for project analysis
      std::string project_scope = "current_notebook";  // Scope: "current_notebook", "all_notes", "selected_tags"
      bool auto_generate_milestones = true;        // Automatically generate project milestones
      int max_related_notes = 20;                  // Maximum notes to analyze for project context
    };
    ProjectAssistantConfig project_assistant;
    
    // AI Learning Path Generator settings (Phase 5) 
    struct LearningPathGeneratorConfig {
      bool enabled = false;                         // Enable learning path generation (disabled by default)
      int max_tokens = 1800;                       // Maximum tokens for learning path generation  
      double temperature = 0.4;                    // Medium-high temperature for creative learning paths
      int timeout_ms = 25000;                      // Timeout for learning path generation
      int max_prerequisites = 5;                   // Maximum prerequisite topics to suggest
      int max_learning_steps = 15;                 // Maximum steps in learning path
      bool include_resources = true;               // Include external resource suggestions
    };
    LearningPathGeneratorConfig learning_path_generator;
    
    // AI Knowledge Synthesis settings (Phase 5)
    struct KnowledgeSynthesisConfig {
      bool enabled = false;                         // Enable knowledge synthesis (disabled by default)
      int max_tokens = 3000;                       // Maximum tokens for knowledge synthesis
      double temperature = 0.25;                   // Low-medium temperature for accurate synthesis
      int timeout_ms = 35000;                      // Timeout for knowledge synthesis
      int max_source_notes = 25;                   // Maximum notes to synthesize
      bool detect_contradictions = true;           // Detect contradictions between sources
      bool suggest_gaps = true;                    // Suggest knowledge gaps
    };
    KnowledgeSynthesisConfig knowledge_synthesis;
    
    // AI Journal Insights settings (Phase 5)
    struct JournalInsightsConfig {
      bool enabled = false;                         // Enable journal insights (disabled by default)
      int max_tokens = 1200;                       // Maximum tokens for journal analysis
      double temperature = 0.3;                    // Medium temperature for insightful analysis
      int timeout_ms = 20000;                      // Timeout for journal analysis  
      int analysis_window_days = 30;               // Days to look back for patterns
      bool track_mood_patterns = true;             // Track mood and emotional patterns
      bool track_productivity_patterns = true;     // Track productivity patterns
      bool suggest_habit_changes = true;           // Suggest habit and routine improvements
    };
    JournalInsightsConfig journal_insights;

    // Phase 6 - Advanced AI Integration
    
    // Multi-modal AI settings (Phase 6) 
    struct MultiModalConfig {
      bool enabled = false;                         // Enable multi-modal AI (disabled by default)
      int max_tokens = 2000;                       // Maximum tokens for multi-modal analysis
      double temperature = 0.4;                    // Balanced temperature for creativity and accuracy
      int timeout_ms = 30000;                      // Timeout for multi-modal analysis
      bool analyze_images = true;                  // Analyze attached images
      bool generate_alt_text = true;               // Generate alt text for images
      bool extract_text_from_images = true;        // OCR text extraction from images
      bool analyze_document_structure = true;      // Analyze document layout and structure
    };
    MultiModalConfig multi_modal;
    
    // Voice integration settings (Phase 6)
    struct VoiceIntegrationConfig {
      bool enabled = false;                         // Enable voice features (disabled by default)
      int max_tokens = 1500;                       // Maximum tokens for voice processing
      double temperature = 0.3;                    // Low temperature for accurate transcription
      int timeout_ms = 25000;                      // Timeout for voice processing
      std::string tts_voice = "default";           // Text-to-speech voice preference
      std::string speech_language = "en-US";       // Speech recognition language
      bool auto_punctuation = true;                // Auto-add punctuation to transcriptions
      bool background_listening = false;           // Enable background voice commands
    };
    VoiceIntegrationConfig voice_integration;
    
    // Context awareness settings (Phase 6)
    struct ContextAwarenessConfig {
      bool enabled = false;                         // Enable advanced context awareness (disabled by default)
      int max_tokens = 1800;                       // Maximum tokens for context analysis
      double temperature = 0.35;                   // Balanced temperature for context understanding
      int timeout_ms = 15000;                      // Timeout for context analysis
      int context_window_notes = 20;               // Number of recent notes to consider
      bool track_reading_patterns = true;          // Track what user reads/focuses on
      bool predict_next_actions = true;            // Predict likely next user actions
      bool suggest_related_content = true;         // Suggest related notes and topics
      bool adaptive_interface = true;              // Adapt interface based on usage patterns
    };
    ContextAwarenessConfig context_awareness;
    
    // AI workspace settings (Phase 6)
    struct WorkspaceAIConfig {
      bool enabled = false;                         // Enable AI workspace features (disabled by default)
      int max_tokens = 1600;                       // Maximum tokens for workspace analysis
      double temperature = 0.4;                    // Medium temperature for creative organization
      int timeout_ms = 20000;                      // Timeout for workspace operations
      bool smart_folder_suggestions = true;        // Suggest optimal folder organization
      bool auto_tag_relationships = true;          // Automatically tag note relationships
      bool workspace_health_monitoring = true;     // Monitor workspace organization health
      bool smart_archive_suggestions = true;       // Suggest notes to archive based on usage
    };
    WorkspaceAIConfig workspace_ai;
    
    // Predictive AI settings (Phase 6)
    struct PredictiveAIConfig {
      bool enabled = false;                         // Enable predictive AI features (disabled by default)
      int max_tokens = 1400;                       // Maximum tokens for prediction analysis
      double temperature = 0.3;                    // Lower temperature for accurate predictions
      int timeout_ms = 18000;                      // Timeout for predictive analysis
      bool predict_note_needs = true;              // Predict when user might need specific notes
      bool suggest_meeting_prep = true;            // Suggest relevant notes for upcoming meetings
      bool proactive_reminders = true;             // Proactively suggest action items and reminders
      bool learning_path_optimization = true;      // Optimize learning paths based on progress
    };
    PredictiveAIConfig predictive_ai;

    // Phase 7 - Collaborative Intelligence & Knowledge Networks
    
    // Collaborative AI settings (Phase 7)
    struct CollaborativeAIConfig {
      bool enabled = false;                         // Enable collaborative AI features (disabled by default)
      int max_tokens = 2500;                       // Maximum tokens for collaborative analysis
      double temperature = 0.5;                    // Balanced temperature for creativity in collaboration
      int timeout_ms = 45000;                      // Timeout for collaborative operations
      bool enable_shared_sessions = true;          // Enable shared AI sessions across notes
      bool cross_reference_analysis = true;        // Analyze cross-references between notes
      bool collaborative_editing = true;           // Enable collaborative editing suggestions
      bool consensus_building = true;              // Build consensus from multiple viewpoints
    };
    CollaborativeAIConfig collaborative_ai;
    
    // Knowledge graph settings (Phase 7)
    struct KnowledgeGraphConfig {
      bool enabled = false;                         // Enable knowledge graph features (disabled by default)
      int max_tokens = 2200;                       // Maximum tokens for graph generation
      double temperature = 0.3;                    // Lower temperature for accurate relationship mapping
      int timeout_ms = 35000;                      // Timeout for graph operations
      bool auto_generate_graphs = true;            // Automatically generate knowledge graphs
      bool semantic_clustering = true;             // Cluster related concepts semantically
      bool relationship_inference = true;          // Infer implicit relationships between concepts
      bool visual_graph_export = true;             // Export graphs in visual formats
    };
    KnowledgeGraphConfig knowledge_graph;
    
    // Expert systems settings (Phase 7)
    struct ExpertSystemsConfig {
      bool enabled = false;                         // Enable expert systems (disabled by default)
      int max_tokens = 2800;                       // Maximum tokens for expert analysis
      double temperature = 0.2;                    // Low temperature for expert-level accuracy
      int timeout_ms = 40000;                      // Timeout for expert system operations
      std::string primary_domain = "general";      // Primary domain of expertise
      bool multi_domain_support = true;            // Support multiple domains of expertise
      bool adaptive_expertise = true;              // Adapt expertise based on content
      bool citation_generation = true;             // Generate citations and references
    };
    ExpertSystemsConfig expert_systems;
    
    // Intelligent workflows settings (Phase 7)
    struct IntelligentWorkflowsConfig {
      bool enabled = false;                         // Enable intelligent workflows (disabled by default)
      int max_tokens = 2000;                       // Maximum tokens for workflow analysis
      double temperature = 0.4;                    // Medium temperature for workflow creativity
      int timeout_ms = 30000;                      // Timeout for workflow operations
      bool auto_workflow_detection = true;         // Automatically detect workflow patterns
      bool process_optimization = true;            // Optimize existing processes
      bool deadline_management = true;             // Intelligent deadline and priority management
      bool resource_allocation = true;             // Suggest optimal resource allocation
    };
    IntelligentWorkflowsConfig intelligent_workflows;
    
    // Meta-learning settings (Phase 7)
    struct MetaLearningConfig {
      bool enabled = false;                         // Enable meta-learning features (disabled by default)
      int max_tokens = 1800;                       // Maximum tokens for meta-learning analysis
      double temperature = 0.35;                   // Balanced temperature for learning adaptation
      int timeout_ms = 25000;                      // Timeout for meta-learning operations
      bool user_pattern_learning = true;           // Learn from user interaction patterns
      bool adaptive_assistance = true;             // Adapt assistance based on user behavior
      bool personalization = true;                 // Personalize AI responses over time
      bool learning_analytics = true;              // Provide analytics on learning progress
    };
    MetaLearningConfig meta_learning;
  };
  std::optional<AiConfig> ai;
  
  // TUI Editor configuration
  struct TuiEditorConfig {
    int tab_width = 4;                    // Tab width in spaces (default 4)
    bool use_tabs = false;                // Use tabs vs spaces
    bool show_whitespace = false;         // Show whitespace characters
    bool auto_indent = true;              // Auto-indent new lines
    bool rtl_support = true;              // Right-to-left language support
  };
  TuiEditorConfig tui_editor;
  
  // Performance tuning
  struct PerformanceConfig {
    size_t cache_size_mb = 50;
    size_t max_file_size_mb = 10;
    int sqlite_cache_size = -20000;  // 20k pages
    std::string sqlite_journal_mode = "WAL";
    std::string sqlite_synchronous = "NORMAL";
    std::string sqlite_temp_store = "MEMORY";
  };
  PerformanceConfig performance;
  
  // Load configuration from file
  Result<void> load(const std::filesystem::path& config_path);
  
  // Save configuration to file
  Result<void> save(const std::filesystem::path& config_path = {}) const;
  
  // Get/set configuration values using dot notation
  Result<std::string> get(const std::string& key) const;
  Result<void> set(const std::string& key, const std::string& value);
  
  // Validate configuration
  Result<void> validate() const;
  
  // Get default configuration file path
  static std::filesystem::path defaultConfigPath();
  
  // Create default configuration
  static Config createDefault();
  
  // Environment variable resolution
  std::string resolveEnvVar(const std::string& value) const;

 private:
  std::filesystem::path config_path_;
  
  // Convert enum values to/from strings
  static std::string indexerTypeToString(IndexerType type);
  static IndexerType stringToIndexerType(const std::string& str);
  
  static std::string encryptionTypeToString(EncryptionType type);
  static EncryptionType stringToEncryptionType(const std::string& str);
  
  static std::string syncTypeToString(SyncType type);
  static SyncType stringToSyncType(const std::string& str);
  
  // Dot notation helpers
  Result<std::string> getValueByPath(const std::vector<std::string>& path) const;
  Result<void> setValueByPath(const std::vector<std::string>& path, const std::string& value);
  
  std::vector<std::string> splitPath(const std::string& path) const;
};

}  // namespace nx::config