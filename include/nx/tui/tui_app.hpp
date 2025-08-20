#pragma once

#include <memory>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <functional>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "nx/common.hpp"
#include "nx/config/config.hpp"
#include "nx/store/note_store.hpp"
#include "nx/store/notebook_manager.hpp"
#include "nx/index/index.hpp"
#include "nx/core/note.hpp"
#include "nx/core/note_id.hpp"
#include "nx/core/metadata.hpp"
#include "nx/tui/editor_buffer.hpp"
#include "nx/tui/editor_security.hpp"
#include "nx/tui/editor_commands.hpp"
#include "nx/tui/enhanced_cursor.hpp"
#include "nx/tui/editor_search.hpp"
#include "nx/tui/editor_dialogs.hpp"
#include "nx/tui/viewport_manager.hpp"
#include "nx/tui/markdown_highlighter.hpp"
#include "nx/tui/ai_explanation.hpp"
#include "nx/template/template_manager.hpp"

namespace nx::tui {

/**
 * @brief Enumeration for active pane focus
 */
enum class ActivePane {
  Navigation,    // Focus on hierarchical navigation panel (replaces Tags)
  TagFilters,    // Focus on active tag filters (legacy support)
  Notes,
  SearchBox,     // Focus on search input
  Preview
};

/**
 * @brief Enumeration for view modes based on terminal size
 */
enum class ViewMode {
  SinglePane,  // < 80 cols: notes only
  TwoPane,     // 80-120 cols: notes + preview
  ThreePane    // > 120 cols: tags + notes + preview
};

/**
 * @brief Enumeration for sort modes
 */
enum class SortMode {
  Modified,
  Created,
  Title,
  Relevance
};

/**
 * @brief Navigation item types for hierarchical navigation
 */
enum class NavItemType {
  Notebook,      // Top-level notebook entry
  NotebookTag,   // Tag within a specific notebook
  GlobalTag      // Tag in the "ALL TAGS" section
};

/**
 * @brief Individual navigation item in the flattened tree
 */
struct NavItem {
  NavItemType type;
  std::string name;
  std::string parent_notebook;  // Empty for notebooks/global tags
  size_t count = 0;            // Note count for this item
  bool selected = false;       // Currently selected for filtering
  bool expanded = false;       // Only relevant for notebooks
};

/**
 * @brief Notebook information for TUI display
 */
struct NotebookUIInfo {
  std::string name;
  size_t note_count = 0;
  std::vector<std::string> tags;                    // Tags within this notebook
  std::map<std::string, size_t> tag_counts;        // Per-tag counts
  bool expanded = false;
  bool selected = false;
};

/**
 * @brief Application state for the TUI
 */
struct AppState {
  // View state
  ActivePane current_pane = ActivePane::Notes;
  ViewMode view_mode = ViewMode::ThreePane;
  
  // Data state
  std::vector<nx::core::Note> all_notes;     // Complete unfiltered list
  std::vector<nx::core::Note> notes;        // Filtered/displayed list
  std::vector<std::string> tags;
  std::map<std::string, int> tag_counts;
  nx::core::NoteId selected_note_id;
  std::set<nx::core::NoteId> selected_notes;
  
  // Filter state
  std::string search_query;
  std::set<std::string> active_tag_filters;
  SortMode sort_mode = SortMode::Modified;
  
  // Notebook navigation state
  std::vector<NotebookUIInfo> notebooks;
  std::vector<NavItem> nav_items;                       // Flattened navigation tree
  int selected_nav_index = 0;                          // Index in nav_items
  std::set<std::string> active_notebooks;              // Selected notebook filters
  std::map<std::string, std::set<std::string>> active_notebook_tags;  // notebook -> tags
  std::set<std::string> active_global_tags;            // Global tag filters
  bool show_all_tags_section = true;
  
  // UI state
  bool show_help = false;
  bool command_palette_open = false;
  bool new_note_modal_open = false;
  bool search_mode_active = false;
  bool edit_mode_active = false;
  bool tag_edit_modal_open = false;
  bool notebook_modal_open = false;
  bool move_note_modal_open = false;
  std::string status_message;
  std::string tag_search_query;
  std::string command_palette_query;
  std::string tag_edit_input;
  nx::core::NoteId tag_edit_note_id;
  
  // Notebook modal state
  enum class NotebookModalMode {
    Create,
    Rename,
    Delete
  };
  NotebookModalMode notebook_modal_mode = NotebookModalMode::Create;
  std::string notebook_modal_input;
  std::string notebook_modal_target;  // For rename/delete operations
  bool notebook_modal_force = false;  // Force delete flag
  
  // Move note modal state
  std::vector<std::string> move_note_notebooks;  // Available notebooks
  int move_note_selected_index = 0;               // Selected notebook index
  nx::core::NoteId move_note_target_id;           // Note to move
  
  // Template modal state
  bool template_browser_open = false;
  bool template_variables_modal_open = false;
  bool new_note_template_mode = false;  // true = using template, false = blank note
  std::vector<nx::template_system::TemplateInfo> available_templates;
  int selected_template_index = 0;
  std::string selected_template_name;
  std::map<std::string, std::string> template_variables;
  std::string template_variable_input;
  std::string current_variable_name;
  std::vector<std::string> pending_variables;  // Variables still needing input
  
  // Edit mode state - Enhanced security architecture
  std::unique_ptr<EditorBuffer> editor_buffer;
  std::unique_ptr<EditorInputValidator> input_validator;
  std::unique_ptr<SecureClipboard> clipboard;
  std::unique_ptr<CommandHistory> command_history;
  std::unique_ptr<EnhancedCursor> enhanced_cursor;
  std::unique_ptr<EditorSearch> editor_search;
  std::unique_ptr<DialogManager> dialog_manager;
  std::unique_ptr<ViewportManager> editor_viewport;
  std::unique_ptr<ViewportManager> preview_viewport;
  std::unique_ptr<MarkdownHighlighter> markdown_highlighter;
  int edit_cursor_line = 0;
  int edit_cursor_col = 0;
  int edit_scroll_offset = 0;  // For editor scrolling (legacy - will be replaced)
  bool edit_has_changes = false;
  
  // Search state
  bool search_dialog_open = false;
  bool goto_line_dialog_open = false;
  bool replace_dialog_open = false;
  
  // AI Explanation state
  bool explanation_pending = false;               // AI request in progress
  bool has_pending_expansion = false;             // Has brief explanation that can be expanded
  size_t explanation_start_line = 0;              // Line where explanation starts
  size_t explanation_start_col = 0;               // Column where explanation starts
  size_t explanation_end_col = 0;                 // Column where explanation ends
  std::string original_term;                      // Original term being explained
  std::string brief_explanation;                  // Current brief explanation
  std::string expanded_explanation;               // Cached expanded explanation
  
  // Navigation state
  int selected_note_index = 0;
  int previous_note_index = 0;    // Remember note selection when focus moves away
  int preview_scroll_offset = 0;
  int navigation_scroll_offset = 0;     // For navigation panel scrolling
  int notes_scroll_offset = 0;          // For notes panel scrolling
};

/**
 * @brief Panel sizing configuration
 */
struct PanelSizing {
  int tags_width = 25;
  int notes_width = 50;
  int preview_width = 25;
  
  // Minimum widths (in percentage points)
  static constexpr int MIN_NOTES_WIDTH = 25;    // Notes panel needs at least 25% 
  static constexpr int MIN_PREVIEW_WIDTH = 15;  // Preview panel needs at least 15%
  static constexpr int RESIZE_STEP = 5;         // Resize in 5% increments
  
  // Validate proportions add up to 100
  void normalize() {
    int total = tags_width + notes_width + preview_width;
    if (total != 100) {
      // Proportionally adjust to sum to 100
      tags_width = (tags_width * 100) / total;
      notes_width = (notes_width * 100) / total;
      preview_width = 100 - tags_width - notes_width;
    }
  }
  
  // Resize notes panel (expand/contract against preview panel)
  bool resizeNotes(int delta) {
    int new_notes_width = notes_width + delta;
    int new_preview_width = preview_width - delta;
    
    // Check minimum constraints
    if (new_notes_width < MIN_NOTES_WIDTH || new_preview_width < MIN_PREVIEW_WIDTH) {
      return false; // Cannot resize
    }
    
    // Apply the resize
    notes_width = new_notes_width;
    preview_width = new_preview_width;
    normalize(); // Ensure totals are correct
    return true;
  }
};

/**
 * @brief Command structure for command palette
 */
struct TUICommand {
  std::string name;
  std::string description;
  std::string category;
  std::function<void()> action;
  std::string shortcut;
};

/**
 * @brief Main TUI application class
 */
class TUIApp {
public:
  /**
   * @brief Constructor
   * @param config Configuration object
   * @param note_store Note storage interface
   * @param notebook_manager Notebook management interface
   * @param search_index Search index interface
   * @param template_manager Template management interface
   */
  TUIApp(nx::config::Config& config, 
         nx::store::NoteStore& note_store,
         nx::store::NotebookManager& notebook_manager,
         nx::index::Index& search_index,
         nx::template_system::TemplateManager& template_manager);
  
  ~TUIApp();

  /**
   * @brief Run the TUI application
   * @return Exit code (0 = success)
   */
  int run();
  
  /**
   * @brief Check if TUI should be launched (based on arguments)
   * @param argc Argument count
   * @param argv Argument vector
   * @return true if TUI should be launched
   */
  static bool shouldLaunchTUI(int argc, char* argv[]);

private:
  // Core services
  nx::config::Config& config_;
  nx::store::NoteStore& note_store_;
  nx::store::NotebookManager& notebook_manager_;
  nx::index::Index& search_index_;
  nx::template_system::TemplateManager& template_manager_;
  
  // AI services
  std::unique_ptr<AiExplanationService> ai_explanation_service_;
  
  // Application state
  AppState state_;
  PanelSizing panel_sizing_;
  
  // FTXUI components
  ftxui::ScreenInteractive screen_;
  ftxui::Component main_component_;
  
  // Component creation
  ftxui::Component createMainComponent();
  ftxui::Component createNavigationPanel();
  ftxui::Component createNotesPanel();
  ftxui::Component createPreviewPane();
  ftxui::Component createStatusLine();
  ftxui::Component createCommandPalette();
  ftxui::Component createHelpModal();
  
  // Layout calculation
  ViewMode calculateViewMode(int terminal_width) const;
  PanelSizing calculatePanelSizing(int terminal_width) const;
  void updateLayout();
  
  // Data operations
  Result<void> loadNotes();
  Result<void> loadTags();
  Result<void> loadNotebooks();
  void refreshData();
  void buildNavigationItems();
  void applyFilters();
  void sortNotes();
  void performSearch(const std::string& query);
  void performSimpleFilter(const std::string& query);
  void performFullTextSearch(const std::string& query);
  void invalidateSearchCache();
  
  // Event handlers
  void onKeyPress(const ftxui::Event& event);
  void onNoteSelected(int index);
  void onTagToggled(const std::string& tag);
  void onNotebookToggled(const std::string& notebook);
  void onSearchInput(const std::string& query);
  
  // Note operations
  Result<void> createNote();
  Result<void> editNote(const nx::core::NoteId& note_id);
  Result<void> deleteNote(const nx::core::NoteId& note_id);
  
  // Tag operations
  Result<void> addTagsToNote(const nx::core::NoteId& note_id, const std::vector<std::string>& tags);
  Result<void> removeTagsFromNote(const nx::core::NoteId& note_id, const std::vector<std::string>& tags);
  Result<void> setTagsForNote(const nx::core::NoteId& note_id, const std::vector<std::string>& tags);
  void openTagEditModal(const nx::core::NoteId& note_id);
  
  // Notebook operations
  Result<void> createNotebook(const std::string& name);
  Result<void> renameNotebook(const std::string& old_name, const std::string& new_name);
  Result<void> deleteNotebook(const std::string& name, bool force = false);
  void openNotebookModal(AppState::NotebookModalMode mode, const std::string& target_notebook = "");
  void openMoveNoteModal();
  
  // Template operations
  void openTemplateBrowser();
  void openTemplateVariablesModal(const std::string& template_name);
  void closeTemplateBrowser();
  void closeTemplateVariablesModal();
  Result<void> createNoteFromTemplate(const std::string& template_name, 
                                      const std::map<std::string, std::string>& variables);
  Result<void> loadAvailableTemplates();
  void processTemplateVariableInput();
  void handleTemplateSelection();
  
  // AI operations
  void suggestTagsForAllNotes();
  void aiAutoTagSelectedNote();
  void aiAutoTitleSelectedNote();
  Result<std::vector<std::string>> suggestTagsForNote(const nx::core::Note& note, 
                                                     const nx::config::Config::AiConfig& ai_config);
  Result<std::string> suggestTitleForNote(const nx::core::Note& note,
                                         const nx::config::Config::AiConfig& ai_config);
  
  // Command palette
  void registerCommands();
  std::vector<TUICommand> commands_;
  std::vector<TUICommand> getFilteredCommands(const std::string& query) const;
  
  // Rendering helpers
  ftxui::Element renderNoteMetadata(const nx::core::Note& note, bool selected) const;
  ftxui::Element renderNavigationPanel() const;
  ftxui::Element renderNotePreview(const nx::core::NoteId& note_id) const;
  ftxui::Element renderNotesPanel() const;
  ftxui::Element renderPreviewPane() const;
  ftxui::Element renderStatusLine() const;
  ftxui::Element renderCommandPalette() const;
  ftxui::Element renderHelpModal() const;
  ftxui::Element renderNewNoteModal() const;
  ftxui::Element renderTagEditModal() const;
  ftxui::Element renderNotebookModal() const;
  ftxui::Element renderMoveNoteModal() const;
  ftxui::Element renderTemplateBrowser() const;
  ftxui::Element renderTemplateVariablesModal() const;
  ftxui::Element renderEditor() const;
  
  // Helper functions for markdown highlighting
  ftxui::Decorator textStyleToDecorator(const TextStyle& style) const;
  ftxui::Element createStyledLine(const std::string& line, const HighlightResult& highlight) const;
  ftxui::Element createStyledLineWithCursor(const std::string& line, const HighlightResult& highlight, size_t cursor_pos) const;
  ftxui::Element highlightSearchInLine(const std::string& line, const std::string& query) const;
  
  // Edit mode helpers
  void initializeEditor();
  void handleEditModeInput(const ftxui::Event& event);
  void saveEditedNote();
  
  // Status and error handling
  void setStatusMessage(const std::string& message);
  void handleError(const Error& error);
  
  // Navigation helpers
  void focusPane(ActivePane pane);
  void moveSelection(int delta);
  void pageUp();
  void pageDown();
  void scrollPanelUp();
  void scrollPanelDown();
  void followLinkInPreview();
  
  // Notebook navigation helpers
  void onNavigationItemSelected(int index);
  void toggleNotebookExpansion(const std::string& notebook);
  void toggleNavigationSelection(int index);
  void navigateToNotebook(const std::string& notebook);
  void clearAllFilters();
  
  // Panel resizing
  void resizeNotesPanel(int delta);
  
  // AI Explanation handlers
  void handleBriefExplanation();
  void handleExpandExplanation();
  void insertExplanationText(const std::string& explanation_text);
  void expandExistingExplanation();
  void clearExplanationState();
  AiExplanationService::Config createExplanationConfig() const;
  
  // Layout calculation helpers
  int calculateVisibleTagsCount() const;
  int calculateVisibleNavigationItemsCount() const;
  int calculateVisibleNotesCount() const;
  int calculateVisibleEditorLinesCount() const;
};

} // namespace nx::tui