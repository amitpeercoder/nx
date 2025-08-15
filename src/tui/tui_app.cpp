#include "nx/tui/tui_app.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <regex>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <cctype>
#include <atomic>
#include <thread>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include "nx/util/http_client.hpp"
#include <sys/wait.h>
#include <fcntl.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

#include "nx/store/filesystem_store.hpp"

using namespace ftxui;

namespace nx::tui {

// Global signal handling for emergency cleanup
static std::atomic<ScreenInteractive*> g_active_screen{nullptr};

static void signalHandler(int signal) {
  auto* screen = g_active_screen.load();
  if (screen) {
    screen->Exit();
  }
  std::exit(signal);
}

TUIApp::TUIApp(nx::config::Config& config, 
               nx::store::NoteStore& note_store,
               nx::index::Index& search_index)
    : config_(config)
    , note_store_(note_store)
    , search_index_(search_index)
    , screen_(ScreenInteractive::Fullscreen()) {
  
  registerCommands();
  main_component_ = createMainComponent();
}

TUIApp::~TUIApp() {
  // Ensure proper cleanup of terminal state
  try {
    screen_.Exit();
  } catch (...) {
    // Ignore errors during cleanup
  }
}

bool TUIApp::shouldLaunchTUI(int argc, char* argv[]) {
  if (argc == 1) {
    return true;
  }
  
  if (argc == 2 && std::string(argv[1]) == "ui") {
    return true;
  }
  
  return false;
}

int TUIApp::run() {
  // Install signal handlers for emergency cleanup
  g_active_screen.store(&screen_);
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  
  // Load initial data
  loadNotes();
  loadTags();
  
  // Set initial status
  setStatusMessage("nx notes - Press ? for help, : for commands, q to quit");
  
  // Run the main loop
  screen_.Loop(main_component_);
  
  // Clear signal handlers
  g_active_screen.store(nullptr);
  std::signal(SIGINT, SIG_DFL);
  std::signal(SIGTERM, SIG_DFL);
  
  return 0;
}

Component TUIApp::createMainComponent() {
  
  return Renderer([this] {
    // Calculate layout based on terminal size and smart sizing
    auto terminal_width = screen_.dimx();
    auto sizing = calculatePanelSizing(terminal_width);
    
    Elements main_content;
    
    // Header
    main_content.push_back(
      text("nx Notes") | bold | center | 
      bgcolor(Color::Blue) | color(Color::White)
    );
    
    // Main layout based on view mode
    Elements panes;
    
    switch (state_.view_mode) {
      case ViewMode::SinglePane:
        panes.push_back(renderNotesPanel() | flex);
        break;
        
      case ViewMode::TwoPane:
        panes = {
          renderNotesPanel() | size(WIDTH, EQUAL, sizing.notes_width + sizing.tags_width),
          separator(),
          renderPreviewPane() | size(WIDTH, EQUAL, sizing.preview_width)
        };
        break;
        
      case ViewMode::ThreePane:
        panes = {
          renderTagsPanel() | size(WIDTH, EQUAL, sizing.tags_width),
          separator(),
          renderNotesPanel() | size(WIDTH, EQUAL, sizing.notes_width),
          separator(), 
          renderPreviewPane() | flex
        };
        break;
    }
    
    main_content.push_back(hbox(panes) | flex);
    
    // Status line
    main_content.push_back(separator());
    main_content.push_back(renderStatusLine());
    
    auto main_view = vbox(main_content);
    
    // Overlay modals
    if (state_.command_palette_open) {
      main_view = dbox({
        main_view,
        renderCommandPalette() | center
      });
    }
    
    if (state_.show_help) {
      main_view = dbox({
        main_view,
        renderHelpModal() | center
      });
    }
    
    if (state_.new_note_modal_open) {
      main_view = dbox({
        main_view,
        renderNewNoteModal() | center
      });
    }
    
    if (state_.tag_edit_modal_open) {
      main_view = dbox({
        main_view,
        renderTagEditModal() | center
      });
    }
    
    return main_view;
  }) | CatchEvent([this](Event event) {
    onKeyPress(event);
    return true;
  });
}


Element TUIApp::renderTagList() const {
  Elements tag_elements;
  
  if (state_.tags.empty()) {
    tag_elements.push_back(text("No tags") | center | dim);
    return vbox(tag_elements);
  }
  
  int index = 0;
  for (const auto& tag : state_.tags) {
    auto count = state_.tag_counts.find(tag);
    int tag_count = count != state_.tag_counts.end() ? count->second : 0;
    
    std::string tag_display = "📂 " + tag + " (" + std::to_string(tag_count) + ")";
    auto tag_element = text(tag_display);
    
    // Highlight selected tag
    if (state_.current_pane == ActivePane::Tags && index == state_.selected_tag_index) {
      tag_element = tag_element | inverted;
    }
    
    // Show active filters
    if (state_.active_tag_filters.count(tag)) {
      tag_element = tag_element | bgcolor(Color::Green);
    }
    
    tag_elements.push_back(tag_element);
    index++;
  }
  
  return vbox(tag_elements);
}

ViewMode TUIApp::calculateViewMode(int terminal_width) const {
  if (terminal_width < 80) {
    return ViewMode::SinglePane;
  } else if (terminal_width < 120) {
    return ViewMode::TwoPane;
  } else {
    return ViewMode::ThreePane;
  }
}

PanelSizing TUIApp::calculatePanelSizing(int terminal_width) const {
  // Return the current panel sizing configuration
  // Panel sizes can be dynamically adjusted via keyboard shortcuts
  return panel_sizing_;
}

void TUIApp::updateLayout() {
  auto width = screen_.dimx();
  state_.view_mode = calculateViewMode(width);
  
  // Handle empty notes case
  if (state_.notes.empty()) {
    setStatusMessage("No notes found - Press 'n' to create your first note");
  }
  
  // Update component layouts based on view mode
  if (main_component_) {
    main_component_->OnEvent(ftxui::Event::Special({}));
  }
}

Result<void> TUIApp::loadNotes() {
  try {
    // Get all notes using the search method to get full notes
    nx::store::NoteQuery query;
    auto notes_result = note_store_.search(query);
    if (!notes_result) {
      return std::unexpected(notes_result.error());
    }
    
    // Convert notes to metadata and store in state
    state_.all_notes.clear();
    for (const auto& note : *notes_result) {
      state_.all_notes.push_back(note.metadata());
    }
    
    // Copy to filtered list and apply current sorting
    state_.notes = state_.all_notes;
    sortNotes();
    
    return Result<void>();
  } catch (const std::exception& e) {
    return std::unexpected(Error(ErrorCode::kFileError, "Failed to load notes: " + std::string(e.what())));
  }
}

Result<void> TUIApp::loadTags() {
  try {
    // Extract tags from all loaded notes
    std::map<std::string, int> tag_counts;
    
    for (const auto& metadata : state_.all_notes) {
      for (const auto& tag : metadata.tags()) {
        tag_counts[tag]++;
      }
    }
    
    // Update state
    state_.tag_counts = tag_counts;
    state_.tags.clear();
    
    for (const auto& [tag, count] : tag_counts) {
      state_.tags.push_back(tag);
    }
    
    // Sort tags alphabetically
    std::sort(state_.tags.begin(), state_.tags.end());
    
    return Result<void>();
  } catch (const std::exception& e) {
    return std::unexpected(Error(ErrorCode::kFileError, "Failed to load tags: " + std::string(e.what())));
  }
}

void TUIApp::refreshData() {
  // Reload notes and tags from storage
  auto notes_result = loadNotes();
  if (!notes_result) {
    setStatusMessage("Error loading notes: " + notes_result.error().message());
    return;
  }
  
  auto tags_result = loadTags();
  if (!tags_result) {
    setStatusMessage("Error loading tags: " + tags_result.error().message());
    return;
  }
  
  // Apply current filters and sorting
  applyFilters();
  
  setStatusMessage("Data refreshed");
}

void TUIApp::applyFilters() {
  // Start with all notes (unfiltered)
  std::vector<nx::core::Metadata> filtered_notes = state_.all_notes;
  
  // Apply search query filter
  if (!state_.search_query.empty()) {
    filtered_notes.erase(
      std::remove_if(filtered_notes.begin(), filtered_notes.end(),
        [this](const nx::core::Metadata& metadata) {
          // Search in title and content
          std::string query_lower = state_.search_query;
          std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
          
          std::string title_lower = metadata.title();
          std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(), ::tolower);
          
          return title_lower.find(query_lower) == std::string::npos;
        }),
      filtered_notes.end());
  }
  
  // Apply tag filters (AND logic)
  if (!state_.active_tag_filters.empty()) {
    filtered_notes.erase(
      std::remove_if(filtered_notes.begin(), filtered_notes.end(),
        [this](const nx::core::Metadata& metadata) {
          const auto& note_tags = metadata.tags();
          for (const auto& filter_tag : state_.active_tag_filters) {
            if (std::find(note_tags.begin(), note_tags.end(), filter_tag) == note_tags.end()) {
              return true; // Note doesn't have this required tag
            }
          }
          return false; // Note has all required tags
        }),
      filtered_notes.end());
  }
  
  // Update filtered results
  state_.notes = filtered_notes;
  
  // Reset selection if it's out of bounds
  if (state_.selected_note_index >= static_cast<int>(state_.notes.size())) {
    state_.selected_note_index = std::max(0, static_cast<int>(state_.notes.size()) - 1);
  }
}

void TUIApp::sortNotes() {
  switch (state_.sort_mode) {
    case SortMode::Modified:
      std::sort(state_.notes.begin(), state_.notes.end(),
        [](const nx::core::Metadata& a, const nx::core::Metadata& b) {
          return a.updated() > b.updated(); // Most recent first
        });
      break;
      
    case SortMode::Created:
      std::sort(state_.notes.begin(), state_.notes.end(),
        [](const nx::core::Metadata& a, const nx::core::Metadata& b) {
          return a.created() > b.created(); // Most recent first
        });
      break;
      
    case SortMode::Title:
      std::sort(state_.notes.begin(), state_.notes.end(),
        [](const nx::core::Metadata& a, const nx::core::Metadata& b) {
          return a.title() < b.title(); // Alphabetical
        });
      break;
      
    case SortMode::Relevance:
      // For relevance, keep current order (from search results)
      // or fall back to modified date if no search query
      if (state_.search_query.empty()) {
        std::sort(state_.notes.begin(), state_.notes.end(),
          [](const nx::core::Metadata& a, const nx::core::Metadata& b) {
            return a.updated() > b.updated();
          });
      }
      break;
  }
}

void TUIApp::onKeyPress(const ftxui::Event& event) {
  // Handle edit mode first
  if (state_.edit_mode_active) {
    
    if (event == ftxui::Event::Escape) {
      // Cancel edit mode
      state_.edit_mode_active = false;
      state_.edit_content.clear();
      state_.edit_has_changes = false;
      setStatusMessage("Edit cancelled");
      return;
    }
    if (event.character() == "\x13") { // Ctrl+S (ASCII 19)
      // Save the note
      saveEditedNote();
      return;
    }
    // Handle text input and cursor movement
    handleEditModeInput(event);
    return;
  }
  
  // Handle search mode first
  if (state_.search_mode_active) {
    if (event == ftxui::Event::Escape) {
      state_.search_mode_active = false;
      state_.search_query.clear();
      // Reload all notes
      loadNotes();
      loadTags();
      applyFilters();
      setStatusMessage("Search cancelled");
      return;
    }
    if (event == ftxui::Event::Return) {
      state_.search_mode_active = false;
      setStatusMessage("Search complete: " + std::to_string(state_.notes.size()) + " notes");
      return;
    }
    if (event == ftxui::Event::Backspace) {
      if (!state_.search_query.empty()) {
        state_.search_query.pop_back();
        // Perform real-time search
        performSearch(state_.search_query);
        setStatusMessage("Search: " + (state_.search_query.empty() ? "[cleared]" : state_.search_query));
      }
      return;
    }
    if (event == ftxui::Event::ArrowDown) {
      // Move focus to top note if notes exist
      if (!state_.notes.empty()) {
        state_.search_mode_active = false;
        focusPane(ActivePane::Notes);
        state_.selected_note_index = 0;
        if (static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
          state_.selected_note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
        }
        setStatusMessage("Moved to notes");
      }
      return;
    }
    if (event.is_character() && event.character().size() == 1) {
      char c = event.character()[0];
      if (c >= 32 && c <= 126) { // Printable ASCII
        state_.search_query += c;
        // Perform real-time search
        performSearch(state_.search_query);
        setStatusMessage("Search: " + state_.search_query);
      }
      return;
    }
    return;
  }
  
  // Handle modal states
  if (state_.new_note_modal_open) {
    if (event == ftxui::Event::Escape) {
      state_.new_note_modal_open = false;
      state_.rename_mode_active = false;
      state_.new_note_title.clear();
      return;
    }
    if (event == ftxui::Event::Return) {
      if (state_.rename_mode_active) {
        // Rename existing note
        if (!state_.notes.empty() && state_.selected_note_index >= 0 && 
            static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
          const auto& metadata = state_.notes[static_cast<size_t>(state_.selected_note_index)];
          std::string new_title = state_.new_note_title.empty() ? metadata.title() : state_.new_note_title;
          auto result = renameNote(metadata.id(), new_title);
          if (!result) {
            setStatusMessage("Error renaming note: " + result.error().message());
          }
        }
        state_.rename_mode_active = false;
      } else {
        // Create note with entered title
        std::string title = state_.new_note_title.empty() ? "New Note" : state_.new_note_title;
        auto result = createNote(title);
        if (!result) {
          setStatusMessage("Error creating note: " + result.error().message());
        }
      }
      state_.new_note_modal_open = false;
      state_.new_note_title.clear();
      return;
    }
    if (event == ftxui::Event::Backspace) {
      if (!state_.new_note_title.empty()) {
        state_.new_note_title.pop_back();
      }
      return;
    }
    if (event.is_character() && event.character().size() == 1) {
      char c = event.character()[0];
      if (c >= 32 && c <= 126) { // Printable ASCII
        state_.new_note_title += c;
      }
      return;
    }
    return;
  }
  
  if (state_.tag_edit_modal_open) {
    if (event == ftxui::Event::Escape) {
      state_.tag_edit_modal_open = false;
      state_.tag_edit_input.clear();
      state_.tag_edit_note_id = nx::core::NoteId();
      return;
    }
    if (event == ftxui::Event::Return) {
      // Parse tags and apply them
      std::vector<std::string> tags;
      std::stringstream ss(state_.tag_edit_input);
      std::string tag;
      while (std::getline(ss, tag, ',')) {
        // Trim whitespace
        tag.erase(0, tag.find_first_not_of(" \t"));
        tag.erase(tag.find_last_not_of(" \t") + 1);
        if (!tag.empty()) {
          tags.push_back(tag);
        }
      }
      
      auto result = setTagsForNote(state_.tag_edit_note_id, tags);
      if (!result) {
        setStatusMessage("Error setting tags: " + result.error().message());
      } else {
        setStatusMessage("Tags updated successfully");
        refreshData(); // Refresh to show updated tags
      }
      
      state_.tag_edit_modal_open = false;
      state_.tag_edit_input.clear();
      state_.tag_edit_note_id = nx::core::NoteId();
      return;
    }
    if (event == ftxui::Event::Backspace) {
      if (!state_.tag_edit_input.empty()) {
        state_.tag_edit_input.pop_back();
      }
      return;
    }
    if (event.is_character() && event.character().size() == 1) {
      char c = event.character()[0];
      if (c >= 32 && c <= 126) { // Printable ASCII
        state_.tag_edit_input += c;
      }
      return;
    }
    return;
  }
  
  if (state_.command_palette_open) {
    if (event == ftxui::Event::Escape || event == ftxui::Event::Character(':')) {
      state_.command_palette_open = false;
      state_.command_palette_query.clear();
      return;
    }
    if (event == ftxui::Event::Return) {
      // Execute the first matching command
      auto filtered_commands = getFilteredCommands(state_.command_palette_query);
      if (!filtered_commands.empty()) {
        filtered_commands[0].action();
        state_.command_palette_open = false;
        state_.command_palette_query.clear();
      }
      return;
    }
    if (event == ftxui::Event::Backspace) {
      if (!state_.command_palette_query.empty()) {
        state_.command_palette_query.pop_back();
      }
      return;
    }
    if (event.is_character() && event.character().size() == 1) {
      char c = event.character()[0];
      if (c >= 32 && c <= 126) { // Printable ASCII
        state_.command_palette_query += c;
      }
      return;
    }
    return;
  }
  
  if (state_.show_help) {
    if (event == ftxui::Event::Character('?') || event == ftxui::Event::Escape) {
      state_.show_help = false;
      return;
    }
    return;
  }
  
  // Global shortcuts
  if (event == ftxui::Event::Character('q')) {
    screen_.ExitLoopClosure()();
    return;
  }
  
  if (event == ftxui::Event::Character('?')) {
    state_.show_help = !state_.show_help;
    return;
  }
  
  if (event == ftxui::Event::Character(':')) {
    state_.command_palette_open = !state_.command_palette_open;
    return;
  }
  
  // AI tag all notes with Ctrl+T
  if (event.character() == "\x14") { // Ctrl+T (ASCII 20)
    suggestTagsForAllNotes();
    return;
  }
  
  // AI auto-tag selected note with 'a'
  if (event == ftxui::Event::Character('a')) {
    aiAutoTagSelectedNote();
    return;
  }
  
  // AI auto-title selected note with 'A' (Shift+A)
  if (event == ftxui::Event::Character('A')) {
    aiAutoTitleSelectedNote();
    return;
  }
  
  // Note operations
  if (event == ftxui::Event::Character('n')) {
    state_.new_note_modal_open = true;
    state_.new_note_title.clear();
    setStatusMessage("Enter note title (Enter to create, Esc to cancel)");
    return;
  }
  
  if (event == ftxui::Event::Character('e')) {
    if (!state_.notes.empty() && state_.selected_note_index >= 0 && 
        static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
      auto note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
      auto result = editNote(note_id);
      if (!result) {
        setStatusMessage("Error editing note: " + result.error().message());
      }
    }
    return;
  }
  
  if (event == ftxui::Event::Character('d')) {
    if (!state_.notes.empty() && state_.selected_note_index >= 0 && 
        static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
      auto note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
      auto result = deleteNote(note_id);
      if (!result) {
        setStatusMessage("Error deleting note: " + result.error().message());
      }
    }
    return;
  }
  
  if (event == ftxui::Event::Character('r')) {
    // If shift+r or nothing selected, refresh data
    if (state_.notes.empty() || state_.selected_note_index >= static_cast<int>(state_.notes.size())) {
      refreshData();
    } else {
      // Start rename operation - for simplicity, use the new note modal system
      // In a full implementation, this could have a dedicated rename modal
      state_.new_note_modal_open = true;
      state_.rename_mode_active = true;
      const auto& metadata = state_.notes[static_cast<size_t>(state_.selected_note_index)];
      state_.new_note_title = metadata.title();
      setStatusMessage("Enter new title (Enter to rename, Esc to cancel)");
    }
    return;
  }
  
  // Multi-select toggle
  if (event == ftxui::Event::Character(' ')) {
    if (state_.current_pane == ActivePane::Notes && !state_.notes.empty() && 
        state_.selected_note_index >= 0 && 
        static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
      auto note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
      if (state_.selected_notes.count(note_id)) {
        state_.selected_notes.erase(note_id);
        setStatusMessage("Deselected note");
      } else {
        state_.selected_notes.insert(note_id);
        setStatusMessage("Selected note");
      }
    }
    return;
  }
  
  // Tag operations  
  if (event == ftxui::Event::Character('t')) {
    if (state_.current_pane == ActivePane::Tags && !state_.tags.empty() &&
        state_.selected_tag_index >= 0 && 
        static_cast<size_t>(state_.selected_tag_index) < state_.tags.size()) {
      const auto& tag = state_.tags[static_cast<size_t>(state_.selected_tag_index)];
      onTagToggled(tag);
    } else if (state_.current_pane == ActivePane::Notes && !state_.notes.empty() && 
               state_.selected_note_index >= 0 && 
               static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
      // Edit tags for selected note
      auto note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
      openTagEditModal(note_id);
    }
    return;
  }
  
  // Search
  if (event == ftxui::Event::Character('/')) {
    state_.search_mode_active = true;
    state_.search_query.clear();
    setStatusMessage("Real-time search - type to filter, Enter to finish, Esc to cancel");
    return;
  }
  
  // Panel resizing when focused on Notes panel
  if (state_.current_pane == ActivePane::Notes && state_.view_mode == ViewMode::ThreePane) {
    if (event == ftxui::Event::Character('+') || event == ftxui::Event::Character('=')) {
      // + or =: Expand notes panel (shrink preview panel)
      resizeNotesPanel(PanelSizing::RESIZE_STEP);
      return;
    }
    if (event == ftxui::Event::Character('-') || event == ftxui::Event::Character('_')) {
      // - or _: Shrink notes panel (expand preview panel)
      resizeNotesPanel(-PanelSizing::RESIZE_STEP);
      return;
    }
  }
  
  // Navigation shortcuts - move between adjacent panes
  if (event == ftxui::Event::Character('h') || event == ftxui::Event::ArrowLeft) {
    switch (state_.current_pane) {
      case ActivePane::Notes:
      case ActivePane::SearchBox:
        if (state_.view_mode == ViewMode::ThreePane) {
          focusPane(ActivePane::Tags);
        }
        break;
      case ActivePane::Preview:
        focusPane(ActivePane::Notes);
        break;
      case ActivePane::Tags:
      case ActivePane::TagFilters:
        // Already at leftmost, no action
        break;
    }
    return;
  }
  
  if (event == ftxui::Event::Character('l') || event == ftxui::Event::ArrowRight) {
    switch (state_.current_pane) {
      case ActivePane::Tags:
      case ActivePane::TagFilters:
        focusPane(ActivePane::Notes);
        break;
      case ActivePane::Notes:
        // Remember current note selection when moving away from notes
        state_.previous_note_index = state_.selected_note_index;
        focusPane(ActivePane::Preview);
        break;
      case ActivePane::SearchBox:
        focusPane(ActivePane::Preview);
        break;
      case ActivePane::Preview:
        // Already at rightmost, no action
        break;
    }
    return;
  }
  
  if (event == ftxui::Event::Tab) {
    // Cycle through main panes (skip sub-panes for simplicity)
    switch (state_.current_pane) {
      case ActivePane::Tags:
      case ActivePane::TagFilters:
        focusPane(ActivePane::Notes);
        break;
      case ActivePane::Notes:
        // Remember current note selection when moving away from notes
        state_.previous_note_index = state_.selected_note_index;
        focusPane(ActivePane::Preview);
        break;
      case ActivePane::SearchBox:
        focusPane(ActivePane::Preview);
        break;
      case ActivePane::Preview:
        focusPane(ActivePane::Tags);
        break;
    }
    return;
  }
  
  // Pane-specific navigation
  if (event == ftxui::Event::Character('j') || event == ftxui::Event::ArrowDown) {
    moveSelection(1);
    return;
  }
  
  if (event == ftxui::Event::Character('k') || event == ftxui::Event::ArrowUp) {
    moveSelection(-1);
    return;
  }
  
  // Page navigation
  if (event == ftxui::Event::PageDown) {
    pageDown();
    return;
  }
  
  if (event == ftxui::Event::PageUp) {
    pageUp();
    return;
  }
  
  // Enter key - context dependent
  if (event == ftxui::Event::Return) {
    switch (state_.current_pane) {
      case ActivePane::Tags:
        if (!state_.tags.empty() && state_.selected_tag_index >= 0 && 
            static_cast<size_t>(state_.selected_tag_index) < state_.tags.size()) {
          const auto& tag = state_.tags[static_cast<size_t>(state_.selected_tag_index)];
          onTagToggled(tag);
        }
        break;
        
      case ActivePane::TagFilters:
        // Remove the selected filter
        if (!state_.active_tag_filters.empty() && state_.selected_filter_index >= 0) {
          std::vector<std::string> filter_list(state_.active_tag_filters.begin(), state_.active_tag_filters.end());
          if (static_cast<size_t>(state_.selected_filter_index) < filter_list.size()) {
            const auto& filter_to_remove = filter_list[static_cast<size_t>(state_.selected_filter_index)];
            state_.active_tag_filters.erase(filter_to_remove);
            
            // Adjust selection if we removed the last filter
            if (state_.selected_filter_index >= static_cast<int>(state_.active_tag_filters.size())) {
              state_.selected_filter_index = std::max(0, static_cast<int>(state_.active_tag_filters.size()) - 1);
            }
            
            // If no filters left, go back to tags
            if (state_.active_tag_filters.empty()) {
              focusPane(ActivePane::Tags);
            }
            
            // Reapply filters
            applyFilters();
            setStatusMessage("Removed tag filter: " + filter_to_remove);
          }
        }
        break;
        
      case ActivePane::SearchBox:
        // Start search mode
        state_.search_mode_active = true;
        setStatusMessage("Real-time search - type to filter, Enter to finish, Esc to cancel");
        break;
        
      case ActivePane::Notes:
        if (!state_.notes.empty() && state_.selected_note_index >= 0 && 
            static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
          auto note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
          auto result = editNote(note_id);
          if (!result) {
            setStatusMessage("Error editing note: " + result.error().message());
          }
        }
        break;
        
      case ActivePane::Preview:
        // Follow links in preview pane
        followLinkInPreview();
        break;
    }
    return;
  }
  
  // Sort mode cycling (s key)
  if (event == ftxui::Event::Character('s')) {
    switch (state_.sort_mode) {
      case SortMode::Modified:
        state_.sort_mode = SortMode::Created;
        setStatusMessage("Sorted by created date");
        break;
      case SortMode::Created:
        state_.sort_mode = SortMode::Title;
        setStatusMessage("Sorted by title");
        break;
      case SortMode::Title:
        state_.sort_mode = SortMode::Relevance;
        setStatusMessage("Sorted by relevance");
        break;
      case SortMode::Relevance:
        state_.sort_mode = SortMode::Modified;
        setStatusMessage("Sorted by modified date");
        break;
    }
    sortNotes();
    return;
  }
}

void TUIApp::onNoteSelected(int index) {
  state_.selected_note_index = index;
}

void TUIApp::onTagToggled(const std::string& tag) {
  if (state_.active_tag_filters.count(tag)) {
    // Remove filter
    state_.active_tag_filters.erase(tag);
    setStatusMessage("Removed tag filter: " + tag);
  } else {
    // Add filter
    state_.active_tag_filters.insert(tag);
    setStatusMessage("Added tag filter: " + tag);
  }
  
  // Reapply filters
  applyFilters();
}

void TUIApp::performSearch(const std::string& query) {
  if (query.empty()) {
    // Empty search - reload all notes
    loadNotes();
    loadTags();
    applyFilters();
    setStatusMessage("Showing all notes");
    return;
  }
  
  try {
    // Use consistent simple filtering for real-time search to avoid 
    // the jarring transition between different search methods
    performSimpleFilter(query);
    
  } catch (const std::exception& e) {
    setStatusMessage("Search error: " + std::string(e.what()));
  }
}

void TUIApp::performSimpleFilter(const std::string& query) {
  // Simple cache manager with proper invalidation support
  struct NotesCache {
    std::vector<nx::core::Metadata> notes;
    std::chrono::steady_clock::time_point timestamp;
    bool force_refresh = false;
    
    bool needsRefresh() const {
      auto now = std::chrono::steady_clock::now();
      constexpr auto CACHE_DURATION = std::chrono::seconds(30);
      return notes.empty() || 
             (now - timestamp) > CACHE_DURATION ||
             force_refresh;
    }
    
    void refresh(nx::store::NoteStore& store) {
      notes.clear();
      nx::store::NoteQuery note_query;
      auto notes_result = store.search(note_query);
      if (notes_result) {
        for (const auto& note : *notes_result) {
          notes.push_back(note.metadata());
        }
        timestamp = std::chrono::steady_clock::now();
        force_refresh = false;
      }
    }
    
    void invalidate() {
      force_refresh = true;
    }
  };
  
  static NotesCache cache;
  
  if (cache.needsRefresh()) {
    cache.refresh(note_store_);
  }
  
  // Simple case-insensitive filtering by title
  std::vector<nx::core::Metadata> filtered_notes;
  std::string query_lower = query;
  std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
  
  for (const auto& metadata : cache.notes) {
    bool matches = false;
    
    // Check content only
    auto note_result = note_store_.load(metadata.id());
    if (note_result.has_value()) {
      std::string content_lower = note_result->content();
      std::transform(content_lower.begin(), content_lower.end(), content_lower.begin(), ::tolower);
      if (content_lower.find(query_lower) != std::string::npos) {
        matches = true;
      }
    }
    
    if (matches) {
      filtered_notes.push_back(metadata);
    }
  }
  
  // Update state with filtered results
  state_.notes = filtered_notes;
  
  // Reset selection
  state_.selected_note_index = 0;
  state_.selected_notes.clear();
  
  // Update tags for the filtered results
  loadTags();
}

void TUIApp::performFullTextSearch(const std::string& query) {
  // Use the search index for full-text search functionality
  nx::index::SearchQuery search_query;
  search_query.text = query;
  
  auto search_result = search_index_.search(search_query);
  if (!search_result) {
    setStatusMessage("Search error: " + search_result.error().message());
    return;
  }
  
  // Extract note IDs from search results
  std::vector<nx::core::NoteId> note_ids;
  for (const auto& result : *search_result) {
    note_ids.push_back(result.id);
  }
  
  // Load the full notes for these IDs
  std::vector<nx::core::Metadata> search_notes;
  for (const auto& note_id : note_ids) {
    auto note_result = note_store_.load(note_id);
    if (note_result) {
      search_notes.push_back(note_result->metadata());
    }
  }
  
  // Update state with search results
  state_.notes = search_notes;
  state_.sort_mode = SortMode::Relevance; // Search results are already ranked
  
  // Reset selection
  state_.selected_note_index = 0;
  state_.selected_notes.clear();
  
  // Update tags for the filtered results
  loadTags();
}

void TUIApp::invalidateSearchCache() {
  // Access the same NotesCache instance from performSimpleFilter
  struct NotesCache {
    std::vector<nx::core::Metadata> notes;
    std::chrono::steady_clock::time_point timestamp;
    bool force_refresh = false;
    
    bool needsRefresh() const {
      auto now = std::chrono::steady_clock::now();
      constexpr auto CACHE_DURATION = std::chrono::seconds(30);
      return notes.empty() || 
             (now - timestamp) > CACHE_DURATION ||
             force_refresh;
    }
    
    void refresh(nx::store::NoteStore& store) {
      notes.clear();
      nx::store::NoteQuery note_query;
      auto notes_result = store.search(note_query);
      if (notes_result) {
        for (const auto& note : *notes_result) {
          notes.push_back(note.metadata());
        }
        timestamp = std::chrono::steady_clock::now();
        force_refresh = false;
      }
    }
    
    void invalidate() {
      force_refresh = true;
    }
  };
  
  static NotesCache cache;
  cache.invalidate();
}

void TUIApp::onSearchInput(const std::string& query) {
  performSearch(query);
}

Element TUIApp::renderNoteMetadata(const nx::core::Metadata& metadata, bool selected) const {
  // Create rich metadata display as per specification
  Elements content;
  
  // Primary: Note title with selection indicator
  std::string title_str = selected ? "▶ " + metadata.title() : "  " + metadata.title();
  auto title_element = text(title_str);
  if (selected) {
    title_element = title_element | inverted;
  }
  content.push_back(title_element);
  
  // Secondary: Last modified date/time
  auto modified_time = std::chrono::system_clock::to_time_t(metadata.updated());
  std::stringstream date_ss;
  date_ss << "  " << std::put_time(std::localtime(&modified_time), "%Y-%m-%d %H:%M");
  
  // Add note icon and tags
  std::string metadata_line = date_ss.str() + " 📝";
  
  // Add tags
  if (!metadata.tags().empty()) {
    metadata_line += " ";
    for (size_t i = 0; i < metadata.tags().size() && i < 3; ++i) { // Limit to 3 tags
      if (i > 0) metadata_line += ",";
      metadata_line += metadata.tags()[i];
    }
    if (metadata.tags().size() > 3) {
      metadata_line += ",+" + std::to_string(metadata.tags().size() - 3);
    }
  }
  
  auto metadata_element = text(metadata_line) | dim;
  if (selected) {
    metadata_element = metadata_element | inverted;
  }
  content.push_back(metadata_element);
  
  // Add empty line for spacing (except if selected to keep compact)
  if (!selected) {
    content.push_back(text(""));
  }
  
  return vbox(content);
}

Element TUIApp::renderNotePreview(const nx::core::NoteId& note_id) const {
  (void)note_id;
  return text("Note preview would appear here");
}

void TUIApp::registerCommands() {
  commands_.clear();
  
  // File operations
  commands_.push_back({
    "new", "Create new note", "File",
    [this]() { 
      auto result = createNote("New Note");
      if (!result) setStatusMessage("Error: " + result.error().message());
    },
    "n"
  });
  
  commands_.push_back({
    "edit", "Edit selected note", "File",
    [this]() {
      if (!state_.notes.empty() && state_.selected_note_index >= 0 && static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
        auto note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
        auto result = editNote(note_id);
        if (!result) setStatusMessage("Error: " + result.error().message());
      }
    },
    "e"
  });
  
  commands_.push_back({
    "delete", "Delete selected note", "File",
    [this]() {
      if (!state_.notes.empty() && state_.selected_note_index >= 0 && static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
        auto note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
        auto result = deleteNote(note_id);
        if (!result) setStatusMessage("Error: " + result.error().message());
      }
    },
    "d"
  });
  
  // View operations
  commands_.push_back({
    "refresh", "Refresh data", "View",
    [this]() { refreshData(); },
    "r"
  });
  
  commands_.push_back({
    "toggle-help", "Toggle help", "View",
    [this]() { state_.show_help = !state_.show_help; },
    "?"
  });
  
  // Sort operations
  commands_.push_back({
    "sort-modified", "Sort by modified date", "Sort",
    [this]() {
      state_.sort_mode = SortMode::Modified;
      sortNotes();
      setStatusMessage("Sorted by modified date");
    },
    ""
  });
  
  commands_.push_back({
    "sort-created", "Sort by created date", "Sort",
    [this]() {
      state_.sort_mode = SortMode::Created;
      sortNotes();
      setStatusMessage("Sorted by created date");
    },
    ""
  });
  
  commands_.push_back({
    "sort-title", "Sort by title", "Sort",
    [this]() {
      state_.sort_mode = SortMode::Title;
      sortNotes();
      setStatusMessage("Sorted by title");
    },
    ""
  });
}

Result<void> TUIApp::createNote(const std::string& title) {
  try {
    // Create new note using the note store
    auto note = nx::core::Note::create(title, "");
    auto result = note_store_.store(note);
    
    if (!result) {
      return std::unexpected(result.error());
    }
    
    // Refresh data to show the new note
    refreshData();
    
    // Select the new note
    for (size_t i = 0; i < state_.notes.size(); ++i) {
      if (state_.notes[i].id() == note.id()) {
        state_.selected_note_index = static_cast<int>(i);
        state_.selected_note_id = note.id();
        break;
      }
    }
    
    setStatusMessage("Created note: " + title);
    return Result<void>();
  } catch (const std::exception& e) {
    return std::unexpected(Error(ErrorCode::kFileError, "Failed to create note: " + std::string(e.what())));
  }
}

Result<void> TUIApp::editNote(const nx::core::NoteId& note_id) {
  try {
    // Load the note for editing
    auto note_result = note_store_.load(note_id);
    if (!note_result) {
      return std::unexpected(note_result.error());
    }
    
    // Enter edit mode
    state_.edit_mode_active = true;
    state_.edit_content = note_result->content();
    state_.edit_cursor_line = 0;
    state_.edit_cursor_col = 0;
    state_.edit_scroll_offset = 0;
    state_.edit_has_changes = false;
    
    // Focus the preview panel for editing
    state_.current_pane = ActivePane::Preview;
    
    setStatusMessage("Ctrl+S: Save | Esc: Cancel | ↓ on last line: new line | Enter on empty last line: new line");
    
    return Result<void>();
  } catch (const std::exception& e) {
    return std::unexpected(Error(ErrorCode::kFileError, "Failed to edit note: " + std::string(e.what())));
  }
}

Result<void> TUIApp::deleteNote(const nx::core::NoteId& note_id) {
  try {
    // Delete note from store
    auto result = note_store_.remove(note_id);
    if (!result) {
      return std::unexpected(result.error());
    }
    
    // Refresh data
    refreshData();
    
    setStatusMessage("Note deleted");
    return Result<void>();
  } catch (const std::exception& e) {
    return std::unexpected(Error(ErrorCode::kFileError, "Failed to delete note: " + std::string(e.what())));
  }
}

Result<void> TUIApp::renameNote(const nx::core::NoteId& note_id, const std::string& new_title) {
  try {
    // Get the note
    auto note_result = note_store_.load(note_id);
    if (!note_result) {
      return std::unexpected(note_result.error());
    }
    
    // Update title
    auto note = *note_result;
    note.setTitle(new_title);
    
    // Store updated note
    auto store_result = note_store_.store(note);
    if (!store_result) {
      return std::unexpected(store_result.error());
    }
    
    // Refresh data
    refreshData();
    
    setStatusMessage("Note renamed to: " + new_title);
    return Result<void>();
  } catch (const std::exception& e) {
    return std::unexpected(Error(ErrorCode::kFileError, "Failed to rename note: " + std::string(e.what())));
  }
}

void TUIApp::setStatusMessage(const std::string& message) {
  state_.status_message = message;
}

void TUIApp::handleError(const Error& error) {
  setStatusMessage("Error: " + error.message());
}

void TUIApp::focusPane(ActivePane pane) {
  state_.current_pane = pane;
  
  // Automatic edit mode when focusing preview panel
  if (pane == ActivePane::Preview && !state_.edit_mode_active && 
      !state_.notes.empty() && state_.selected_note_index >= 0 && 
      static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
    
    // Get the selected note and start editing
    const auto& metadata = state_.notes[static_cast<size_t>(state_.selected_note_index)];
    auto result = editNote(metadata.id());
    if (!result) {
      setStatusMessage("Error starting auto-edit mode: " + result.error().message());
    } else {
      setStatusMessage("Ctrl+S: Save | Esc: Cancel | ↓ on last line: new line | Enter on empty last line: new line");
    }
  }
}

void TUIApp::moveSelection(int delta) {
  switch (state_.current_pane) {
    case ActivePane::Notes:
      if (!state_.notes.empty()) {
        int new_index = state_.selected_note_index + delta;
        
        // Handle navigation to search box when going up from first note
        if (delta < 0 && state_.selected_note_index == 0) {
          // Remember current note selection
          state_.previous_note_index = state_.selected_note_index;
          focusPane(ActivePane::SearchBox);
          // Activate search mode (like pressing "/")
          state_.search_mode_active = true;
          state_.search_query.clear();
          setStatusMessage("Real-time search - type to filter, Enter to finish, Esc to cancel");
          return;
        }
        
        state_.selected_note_index = std::clamp(
          new_index,
          0,
          static_cast<int>(state_.notes.size()) - 1
        );
        
        // Update selected note ID
        if (state_.selected_note_index >= 0 && static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
          state_.selected_note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
        }
      } else if (delta < 0) {
        // No notes, go to search box
        focusPane(ActivePane::SearchBox);
      }
      break;
      
    case ActivePane::SearchBox:
      // From search box, only down arrow moves to notes
      if (delta > 0 && !state_.notes.empty()) {
        focusPane(ActivePane::Notes);
        
        // Restore previous note selection if valid, otherwise go to first note
        if (state_.previous_note_index >= 0 && 
            static_cast<size_t>(state_.previous_note_index) < state_.notes.size()) {
          state_.selected_note_index = state_.previous_note_index;
        } else {
          state_.selected_note_index = 0;
        }
        
        // Update selected note ID
        if (!state_.notes.empty() && state_.selected_note_index >= 0 && 
            static_cast<size_t>(state_.selected_note_index) < state_.notes.size()) {
          state_.selected_note_id = state_.notes[static_cast<size_t>(state_.selected_note_index)].id();
        }
      }
      break;
      
    case ActivePane::Tags:
      if (!state_.tags.empty()) {
        int new_index = state_.selected_tag_index + delta;
        
        // Handle navigation to filters when going up from first tag
        if (delta < 0 && state_.selected_tag_index == 0 && !state_.active_tag_filters.empty()) {
          focusPane(ActivePane::TagFilters);
          state_.selected_filter_index = static_cast<int>(state_.active_tag_filters.size()) - 1;
          return;
        }
        
        state_.selected_tag_index = std::clamp(
          new_index,
          0,
          static_cast<int>(state_.tags.size()) - 1
        );
        
        // Auto-scroll to keep selected tag visible
        // Calculate dynamic visible tags based on terminal height
        const int visible_tags = calculateVisibleTagsCount();
        
        // Scroll up if selection moved above visible area
        if (state_.selected_tag_index < state_.tags_scroll_offset) {
          state_.tags_scroll_offset = state_.selected_tag_index;
        }
        // Scroll down if selection moved below visible area
        else if (state_.selected_tag_index >= state_.tags_scroll_offset + visible_tags) {
          state_.tags_scroll_offset = state_.selected_tag_index - visible_tags + 1;
        }
        
        // Ensure scroll offset is within bounds
        state_.tags_scroll_offset = std::clamp(
          state_.tags_scroll_offset,
          0,
          std::max(0, static_cast<int>(state_.tags.size()) - visible_tags)
        );
      } else if (delta < 0 && !state_.active_tag_filters.empty()) {
        // No tags, but have filters - go to filters
        focusPane(ActivePane::TagFilters);
        state_.selected_filter_index = static_cast<int>(state_.active_tag_filters.size()) - 1;
      }
      break;
      
    case ActivePane::TagFilters:
      if (!state_.active_tag_filters.empty()) {
        int new_index = state_.selected_filter_index + delta;
        
        // Handle navigation back to tags when going down from last filter
        if (delta > 0 && state_.selected_filter_index == static_cast<int>(state_.active_tag_filters.size()) - 1) {
          focusPane(ActivePane::Tags);
          state_.selected_tag_index = 0;
          return;
        }
        
        state_.selected_filter_index = std::clamp(
          new_index,
          0,
          static_cast<int>(state_.active_tag_filters.size()) - 1
        );
      } else {
        // No filters, go back to tags
        focusPane(ActivePane::Tags);
      }
      break;
      
    case ActivePane::Preview:
      // Handle preview scrolling
      state_.preview_scroll_offset = std::max(0, state_.preview_scroll_offset + delta);
      break;
  }
}

void TUIApp::pageUp() {
  moveSelection(-10); // Move up by 10 items
}

void TUIApp::pageDown() {
  moveSelection(10); // Move down by 10 items
}

void TUIApp::followLinkInPreview() {
  if (state_.notes.empty() || state_.selected_note_index >= static_cast<int>(state_.notes.size())) {
    setStatusMessage("No note selected");
    return;
  }
  
  const auto& metadata = state_.notes[static_cast<size_t>(state_.selected_note_index)];
  
  // Load the current note to get links
  auto note_result = note_store_.load(metadata.id());
  if (!note_result) {
    setStatusMessage("Error loading note for link following");
    return;
  }
  
  // Extract links from the note
  auto links = note_result->extractContentLinks();
  
  if (links.empty()) {
    setStatusMessage("No links found in current note");
    return;
  }
  
  // For simplicity, follow the first link
  // In a full implementation, this could show a menu or use cursor position
  auto link_id = links[0];
  
  // Find the linked note in our current notes list
  bool found = false;
  for (int i = 0; i < static_cast<int>(state_.notes.size()); ++i) {
    if (state_.notes[static_cast<size_t>(i)].id() == link_id) {
      state_.selected_note_index = i;
      state_.selected_note_id = link_id;
      found = true;
      setStatusMessage("Followed link to: " + state_.notes[static_cast<size_t>(i)].title());
      break;
    }
  }
  
  if (!found) {
    // Try to load the note directly and add it to our view
    auto linked_note_result = note_store_.load(link_id);
    if (linked_note_result) {
      setStatusMessage("Following link to: " + linked_note_result->title() + " (note not in current view)");
      // Could potentially search for this note or load it
    } else {
      setStatusMessage("Link target not found: " + link_id.toString());
    }
  }
}

// Rendering methods
Element TUIApp::renderTagsPanel() const {
  Elements tags_content;
  
  // Header
  tags_content.push_back(
    text("Tags") | bold |
    (state_.current_pane == ActivePane::Tags ? bgcolor(Color::Blue) : nothing)
  );
  tags_content.push_back(separator());
  
  // Tag search box
  if (!state_.tag_search_query.empty()) {
    tags_content.push_back(
      text("🔍 " + state_.tag_search_query) | 
      bgcolor(Color::DarkBlue) | color(Color::White)
    );
    tags_content.push_back(separator());
  }
  
  // Active filters
  if (!state_.active_tag_filters.empty()) {
    tags_content.push_back(text("Active filters:") | dim);
    
    std::vector<std::string> filter_list(state_.active_tag_filters.begin(), state_.active_tag_filters.end());
    for (size_t i = 0; i < filter_list.size(); ++i) {
      const auto& filter = filter_list[i];
      auto filter_element = text("× " + filter);
      
      // Highlight selected filter when in TagFilters pane
      if (state_.current_pane == ActivePane::TagFilters && 
          static_cast<int>(i) == state_.selected_filter_index) {
        filter_element = filter_element | bgcolor(Color::Red) | color(Color::White) | inverted;
      } else {
        filter_element = filter_element | bgcolor(Color::Green) | color(Color::White);
      }
      
      tags_content.push_back(filter_element);
    }
    tags_content.push_back(separator());
  }
  
  // Tag list - apply scrolling
  const int visible_tags = calculateVisibleTagsCount();
  int visible_start = state_.tags_scroll_offset;
  int visible_end = std::min(visible_start + visible_tags, static_cast<int>(state_.tags.size()));
  
  for (int i = visible_start; i < visible_end; ++i) {
    const auto& tag = state_.tags[static_cast<size_t>(i)];
    auto count = state_.tag_counts.find(tag);
    int tag_count = count != state_.tag_counts.end() ? count->second : 0;
    
    auto tag_text = text("📂 " + tag + " (" + std::to_string(tag_count) + ")");
    
    if (state_.current_pane == ActivePane::Tags && i == state_.selected_tag_index) {
      tag_text = tag_text | inverted;
    }
    
    if (state_.active_tag_filters.count(tag)) {
      tag_text = tag_text | bgcolor(Color::Green);
    }
    
    tags_content.push_back(tag_text);
  }
  
  // Add scroll indicators if there are more tags above or below
  if (state_.tags_scroll_offset > 0) {
    // Insert at the beginning of the tag list (after headers/filters)
    auto insert_pos = static_cast<size_t>(static_cast<int>(tags_content.size()) - (visible_end - visible_start));
    tags_content.insert(tags_content.begin() + static_cast<ptrdiff_t>(insert_pos),
                       text("▲ More tags above...") | dim);
  }
  if (visible_end < static_cast<int>(state_.tags.size())) {
    tags_content.push_back(text("▼ More tags below...") | dim);
  }
  
  return vbox(tags_content) | border;
}

Element TUIApp::renderNotesPanel() const {
  Elements notes_content;
  
  // Header with search
  notes_content.push_back(
    text("Notes") | bold |
    (state_.current_pane == ActivePane::Notes ? bgcolor(Color::Blue) : nothing)
  );
  notes_content.push_back(separator());
  
  // Search box
  std::string search_display;
  Element search_element;
  
  if (state_.search_mode_active) {
    search_display = "🔍 " + state_.search_query + "_"; // Add cursor
    search_element = text(search_display) | bgcolor(Color::Yellow) | color(Color::Black);
  } else if (state_.current_pane == ActivePane::SearchBox) {
    // Show focus when SearchBox pane is active
    if (!state_.search_query.empty()) {
      search_display = "🔍 " + state_.search_query;
      search_element = text(search_display) | bgcolor(Color::Blue) | color(Color::White) | inverted;
    } else {
      search_display = "🔍 [Search focused - type / to search]";
      search_element = text(search_display) | bgcolor(Color::Blue) | color(Color::White);
    }
  } else if (!state_.search_query.empty()) {
    search_display = "🔍 " + state_.search_query;
    search_element = text(search_display) | bgcolor(Color::DarkBlue) | color(Color::White);
  } else {
    search_display = "🔍 [Search notes... press / to search]";
    search_element = text(search_display) | dim;
  }
  
  notes_content.push_back(search_element);
  notes_content.push_back(separator());
  
  // Notes list
  if (state_.notes.empty()) {
    notes_content.push_back(text("No notes found") | center | dim);
  } else {
    int index = 0;
    for (const auto& metadata : state_.notes) {
      auto note_element = renderNoteMetadata(metadata, 
        state_.current_pane == ActivePane::Notes && index == state_.selected_note_index);
      
      // Multi-select indicator
      if (state_.selected_notes.count(metadata.id())) {
        note_element = hbox({
          text("✓") | color(Color::Green),
          text(" "),
          note_element
        });
      }
      
      notes_content.push_back(note_element);
      index++;
    }
  }
  
  notes_content.push_back(separator());
  
  // Status info
  std::string sort_indicator;
  switch (state_.sort_mode) {
    case SortMode::Modified: sort_indicator = "↓ modified"; break;
    case SortMode::Created: sort_indicator = "↓ created"; break;
    case SortMode::Title: sort_indicator = "↓ title"; break;
    case SortMode::Relevance: sort_indicator = "↓ relevance"; break;
  }
  
  notes_content.push_back(
    text("📄 " + std::to_string(state_.notes.size()) + " notes | " +
         "🏷️ " + std::to_string(state_.tag_counts.size()) + " tags | " +
         sort_indicator) | dim
  );
  
  return vbox(notes_content) | border;
}

Element TUIApp::renderPreviewPane() const {
  Elements preview_content;
  
  // Header - change based on mode
  std::string header_text = state_.edit_mode_active ? "Editor" : "Preview";
  if (state_.edit_mode_active && state_.edit_has_changes) {
    header_text += " *";
  }
  
  preview_content.push_back(
    text(header_text) | bold |
    (state_.current_pane == ActivePane::Preview ? bgcolor(Color::Blue) | color(Color::White) : nothing)
  );
  preview_content.push_back(separator());
  
  if (state_.edit_mode_active) {
    // Render editor mode
    preview_content.push_back(renderEditor());
  } else if (state_.notes.empty() || state_.selected_note_index >= static_cast<int>(state_.notes.size())) {
    preview_content.push_back(text("No note selected") | center | dim);
  } else {
    const auto& metadata = state_.notes[static_cast<size_t>(state_.selected_note_index)];
    
    // Note title
    preview_content.push_back(text("# " + metadata.title()) | bold);
    
    // Metadata
    auto modified_time = std::chrono::system_clock::to_time_t(metadata.updated());
    
    std::stringstream ss;
    ss << "*Modified: " << std::put_time(std::localtime(&modified_time), "%Y-%m-%d %H:%M") << "*";
    preview_content.push_back(text(ss.str()) | dim);
    preview_content.push_back(text(""));
    
    // Try to load and render note content
    auto note_result = note_store_.load(metadata.id());
    if (note_result) {
      // Simple markdown-like rendering
      std::string content = note_result->content();
      std::istringstream stream(content);
      std::string line;
      
      int line_count = 0;
      while (std::getline(stream, line) && line_count < 20) { // Limit preview
        if (line_count >= state_.preview_scroll_offset) {
          Element line_element = text(line);
          
          // Basic markdown styling
          if (line.starts_with("# ")) {
            line_element = text(line.substr(2)) | bold;
          } else if (line.starts_with("## ")) {
            line_element = text(line.substr(3)) | bold | dim;
          } else if (line.starts_with("- ") || line.starts_with("* ")) {
            line_element = text("• " + line.substr(2));
          }
          
          // Highlight wiki links
          if (line.find("[[") != std::string::npos) {
            line_element = line_element | color(Color::Blue);
          }
          
          preview_content.push_back(line_element);
        }
        line_count++;
      }
    } else {
      preview_content.push_back(text("Error loading note content") | color(Color::Red));
    }
    
    preview_content.push_back(text(""));
    
    // Tags
    if (!metadata.tags().empty()) {
      std::string tags_str = "Tags: ";
      for (size_t i = 0; i < metadata.tags().size(); ++i) {
        if (i > 0) tags_str += " ";
        tags_str += "#" + metadata.tags()[i];
      }
      preview_content.push_back(text(tags_str) | dim);
    }
    
    // Links info - calculate real backlinks and outlinks
    std::string links_info = "Links: ";
    
    // Get backlinks
    auto backlinks_result = note_store_.getBacklinks(metadata.id());
    int backlinks_count = 0;
    if (backlinks_result) {
      backlinks_count = static_cast<int>(backlinks_result->size());
    }
    
    // Get outlinks from note content
    auto note_for_links = note_store_.load(metadata.id());
    int outlinks_count = 0;
    if (note_for_links) {
      auto outlinks = note_for_links->extractContentLinks();
      outlinks_count = static_cast<int>(outlinks.size());
    }
    
    links_info += std::to_string(backlinks_count) + " backlinks, " + 
                  std::to_string(outlinks_count) + " outlinks";
    
    preview_content.push_back(text(links_info) | dim);
  }
  
  return vbox(preview_content) | border;
}

Element TUIApp::renderStatusLine() const {
  return text(state_.status_message) | dim;
}

Element TUIApp::renderCommandPalette() const {
  if (!state_.command_palette_open) {
    return text("");
  }
  
  Elements palette_content;
  
  palette_content.push_back(text("Command Palette") | bold | center);
  palette_content.push_back(separator());
  
  // Show search input
  std::string query_display = "> " + state_.command_palette_query + "_";
  palette_content.push_back(text(query_display) | bgcolor(Color::White) | color(Color::Black));
  palette_content.push_back(separator());
  
  // Show filtered commands
  auto filtered_commands = getFilteredCommands(state_.command_palette_query);
  if (filtered_commands.empty()) {
    palette_content.push_back(text("No commands found") | dim | center);
  } else {
    for (size_t i = 0; i < std::min(filtered_commands.size(), size_t(8)); ++i) {
      const auto& cmd = filtered_commands[i];
      std::string cmd_text = cmd.name + " - " + cmd.description;
      if (!cmd.shortcut.empty()) {
        cmd_text += " (" + cmd.shortcut + ")";
      }
      
      // Highlight first command
      auto cmd_element = text(cmd_text);
      if (i == 0) {
        cmd_element = cmd_element | inverted;
      }
      palette_content.push_back(cmd_element);
    }
  }
  
  palette_content.push_back(separator());
  palette_content.push_back(text("Enter to execute, Esc to cancel") | center | dim);
  
  return vbox(palette_content) | 
         border | 
         size(WIDTH, GREATER_THAN, 50) |
         size(WIDTH, LESS_THAN, 70) |
         size(HEIGHT, GREATER_THAN, 8) |
         size(HEIGHT, LESS_THAN, 15) |
         bgcolor(Color::DarkBlue) |
         color(Color::White);
}

Element TUIApp::renderHelpModal() const {
  if (!state_.show_help) {
    return text("");
  }
  
  Elements help_content;
  
  help_content.push_back(text("nx Notes - Keyboard Shortcuts") | bold | center);
  help_content.push_back(separator());
  
  help_content.push_back(text("Navigation:") | bold);
  help_content.push_back(text("  h/←     Focus left pane (tags)"));
  help_content.push_back(text("  j/↓     Move down in current pane"));
  help_content.push_back(text("  k/↑     Move up in current pane"));
  help_content.push_back(text("  l/→     Focus right pane (auto-edit)"));
  help_content.push_back(text("  Tab     Cycle through panes"));
  help_content.push_back(text("  ↑ from first note → search box"));
  help_content.push_back(text("  ↓ from search box → first note"));
  help_content.push_back(text("  ↑ from first tag → active filters"));
  help_content.push_back(text("  ↓ from last filter → first tag"));
  help_content.push_back(text(""));
  
  help_content.push_back(text("Actions:") | bold);
  help_content.push_back(text("  n       New note"));
  help_content.push_back(text("  e       Edit selected note (built-in editor)"));
  help_content.push_back(text("  d       Delete selected note(s)"));
  help_content.push_back(text("  r       Rename selected note"));
  help_content.push_back(text("  /       Start real-time search"));
  help_content.push_back(text("  :       Open command palette"));
  help_content.push_back(text("  Space   Multi-select toggle"));
  help_content.push_back(text("  Enter   Activate/Remove filter/Edit note"));
  help_content.push_back(text(""));
  
  help_content.push_back(text("Panel Resizing (Notes panel):") | bold);
  help_content.push_back(text("  +/=     Expand notes panel (shrink preview)"));
  help_content.push_back(text("  -/_     Shrink notes panel (expand preview)"));
  help_content.push_back(text(""));
  
  help_content.push_back(text("Auto-Edit:") | bold);
  help_content.push_back(text("  Focusing preview panel → auto-edit mode"));
  help_content.push_back(text("  Use → key or Tab to auto-start editing"));
  help_content.push_back(text(""));
  
  help_content.push_back(text("Search Mode:") | bold);
  help_content.push_back(text("  Real-time filtering as you type"));
  help_content.push_back(text("  Searches note titles"));
  help_content.push_back(text("  Enter: finish search"));
  help_content.push_back(text("  Esc: cancel and show all"));
  help_content.push_back(text(""));
  
  help_content.push_back(text("Editor Mode:") | bold);
  help_content.push_back(text("  Ctrl+S  Save note"));
  help_content.push_back(text("  Esc     Cancel editing"));
  help_content.push_back(text("  Arrows  Move cursor (auto-scroll)"));
  help_content.push_back(text("  ↓ on last line: create new line"));
  help_content.push_back(text("  Enter   New line"));
  help_content.push_back(text("  Bksp    Delete character"));
  help_content.push_back(text(""));
  
  help_content.push_back(text("AI Features:") | bold);
  help_content.push_back(text("  Ctrl+T  Suggest tags for all notes (AI)"));
  help_content.push_back(text("  a       AI auto-tag selected note"));
  help_content.push_back(text("  A       AI auto-title selected note"));
  help_content.push_back(text(""));
  
  help_content.push_back(text("Other:") | bold);
  help_content.push_back(text("  ?       Toggle this help"));
  help_content.push_back(text("  q       Quit application"));
  help_content.push_back(text(""));
  
  help_content.push_back(text("Press ? to close") | center | dim);
  
  return vbox(help_content) |
         border |
         size(WIDTH, GREATER_THAN, 60) |
         size(WIDTH, LESS_THAN, 80) |
         size(HEIGHT, GREATER_THAN, 25) |
         size(HEIGHT, LESS_THAN, 35) |
         bgcolor(Color::DarkBlue) |
         color(Color::White);
}

std::vector<TUICommand> TUIApp::getFilteredCommands(const std::string& query) const {
  if (query.empty()) {
    return commands_;
  }
  
  std::vector<TUICommand> filtered;
  std::string query_lower = query;
  std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
  
  for (const auto& cmd : commands_) {
    std::string name_lower = cmd.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    
    if (name_lower.find(query_lower) != std::string::npos ||
        cmd.description.find(query_lower) != std::string::npos) {
      filtered.push_back(cmd);
    }
  }
  
  return filtered;
}

Element TUIApp::renderEditor() const {
  Elements editor_content;
  
  // Split content into lines
  std::vector<std::string> lines;
  std::istringstream stream(state_.edit_content);
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }
  
  // If empty, add a blank line
  if (lines.empty()) {
    lines.push_back("");
  }
  
  // Calculate visible range based on scroll offset
  const int visible_lines = 20; // Approximate visible lines in editor area
  int start_line = std::max(0, state_.edit_scroll_offset);
  int end_line = std::min(static_cast<int>(lines.size()), start_line + visible_lines);
  
  // Render visible lines with cursor indicator
  for (int i = start_line; i < end_line; ++i) {
    std::string display_line = lines[static_cast<size_t>(i)];
    
    // Show cursor position as a caret
    if (i == state_.edit_cursor_line) {
      // Insert cursor at current column - use a simple caret
      size_t cursor_pos = std::min(static_cast<size_t>(state_.edit_cursor_col), display_line.length());
      
      // Split line at cursor position for proper caret rendering
      if (cursor_pos < display_line.length()) {
        std::string before = display_line.substr(0, cursor_pos);
        std::string after = display_line.substr(cursor_pos);
        
        // Create line with cursor highlighting just the character at cursor
        auto line_elements = hbox({
          text(before),
          text(after.empty() ? " " : after.substr(0, 1)) | inverted,
          after.length() > 1 ? text(after.substr(1)) : text("")
        });
        editor_content.push_back(line_elements);
      } else {
        // Cursor at end of line - show a space with inverted background
        auto line_elements = hbox({
          text(display_line),
          text(" ") | inverted
        });
        editor_content.push_back(line_elements);
      }
    } else {
      editor_content.push_back(text(display_line));
    }
  }
  
  // Add scroll indicator if needed
  if (start_line > 0 || end_line < static_cast<int>(lines.size())) {
    editor_content.push_back(text(""));
    editor_content.push_back(text("↕ Line " + std::to_string(state_.edit_cursor_line + 1) + 
                                 "/" + std::to_string(lines.size())) | center | dim);
  }
  
  // Return just the editor content - hints will be handled at the main layout level
  return vbox(editor_content);
}

void TUIApp::handleEditModeInput(const ftxui::Event& event) {
  // Split content into lines for easier manipulation
  std::vector<std::string> lines;
  std::istringstream stream(state_.edit_content);
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }
  
  // Ensure we have at least one line
  if (lines.empty()) {
    lines.push_back("");
  }
  
  // Clamp cursor position
  state_.edit_cursor_line = std::clamp(state_.edit_cursor_line, 0, static_cast<int>(lines.size()) - 1);
  
  // Handle navigation
  if (event == ftxui::Event::ArrowUp && state_.edit_cursor_line > 0) {
    state_.edit_cursor_line--;
    state_.edit_cursor_col = std::min(state_.edit_cursor_col, static_cast<int>(lines[state_.edit_cursor_line].length()));
    
    // Scroll up if cursor moves above visible area
    if (state_.edit_cursor_line < state_.edit_scroll_offset) {
      state_.edit_scroll_offset = state_.edit_cursor_line;
    }
    return;
  }
  
  if (event == ftxui::Event::ArrowDown) {
    // Check if we're on the last line
    bool is_last_line = (state_.edit_cursor_line >= static_cast<int>(lines.size()) - 1);
    
    if (is_last_line) {
      // Simple approach: add newline to end of content and move cursor
      state_.edit_content += "\n";
      state_.edit_cursor_line++;
      state_.edit_cursor_col = 0;
      state_.edit_has_changes = true;
      
      // Scroll down to show new line
      const int visible_lines = 20;
      if (state_.edit_cursor_line >= state_.edit_scroll_offset + visible_lines) {
        state_.edit_scroll_offset = state_.edit_cursor_line - visible_lines + 1;
      }
      
      // Force screen refresh to show changes
      screen_.Post([](){});
    } else {
      // Normal down movement to next line
      state_.edit_cursor_line++;
      state_.edit_cursor_col = std::min(state_.edit_cursor_col, static_cast<int>(lines[state_.edit_cursor_line].length()));
      
      // Scroll down if cursor moves below visible area
      const int visible_lines = 20;
      if (state_.edit_cursor_line >= state_.edit_scroll_offset + visible_lines) {
        state_.edit_scroll_offset = state_.edit_cursor_line - visible_lines + 1;
      }
    }
    return;
  }
  
  if (event == ftxui::Event::ArrowLeft && state_.edit_cursor_col > 0) {
    state_.edit_cursor_col--;
    return;
  }
  
  if (event == ftxui::Event::ArrowRight) {
    state_.edit_cursor_col = std::min(state_.edit_cursor_col + 1, static_cast<int>(lines[state_.edit_cursor_line].length()));
    return;
  }
  
  // Handle text input
  if (event.is_character() && event.character().size() == 1) {
    char c = event.character()[0];
    if (c >= 32 && c <= 126) { // Printable ASCII
      // Insert character at cursor position
      lines[state_.edit_cursor_line].insert(state_.edit_cursor_col, 1, c);
      state_.edit_cursor_col++;
      state_.edit_has_changes = true;
      
      // Rebuild content string
      rebuildEditContent(lines);
      return;
    }
  }
  
  // Handle Enter (new line)
  if (event == ftxui::Event::Return) {
    // Check if we're on the last line and it's empty
    bool is_last_line = (state_.edit_cursor_line >= static_cast<int>(lines.size()) - 1);
    bool is_line_empty = (state_.edit_cursor_line < static_cast<int>(lines.size()) && 
                         lines[state_.edit_cursor_line].empty());
    
    if (is_last_line && is_line_empty) {
      // Simple approach: just add newline and move cursor
      state_.edit_content += "\n";
      state_.edit_cursor_line++;
      state_.edit_cursor_col = 0;
      state_.edit_has_changes = true;
      
      // Ensure new line is visible - scroll down if needed
      const int visible_lines = 20;
      if (state_.edit_cursor_line >= state_.edit_scroll_offset + visible_lines) {
        state_.edit_scroll_offset = state_.edit_cursor_line - visible_lines + 1;
      }
      
      // Force screen refresh
      screen_.Post([](){});
    } else {
      // Normal behavior: split line at cursor position
      std::string current_line = lines[state_.edit_cursor_line];
      std::string left_part = current_line.substr(0, state_.edit_cursor_col);
      std::string right_part = current_line.substr(state_.edit_cursor_col);
      
      // Update current line and insert new line
      lines[state_.edit_cursor_line] = left_part;
      lines.insert(lines.begin() + state_.edit_cursor_line + 1, right_part);
      
      // Move cursor to beginning of new line
      state_.edit_cursor_line++;
      state_.edit_cursor_col = 0;
      state_.edit_has_changes = true;
      
      // Rebuild content string
      rebuildEditContent(lines);
      
      // Ensure new line is visible - scroll down if needed
      const int visible_lines = 20;
      if (state_.edit_cursor_line >= state_.edit_scroll_offset + visible_lines) {
        state_.edit_scroll_offset = state_.edit_cursor_line - visible_lines + 1;
      }
      
      // Force screen refresh
      screen_.Post([](){});
    }
    
    return;
  }
  
  // Handle Backspace
  if (event == ftxui::Event::Backspace) {
    if (state_.edit_cursor_col > 0) {
      // Delete character before cursor
      lines[state_.edit_cursor_line].erase(state_.edit_cursor_col - 1, 1);
      state_.edit_cursor_col--;
    } else if (state_.edit_cursor_line > 0) {
      // Join with previous line
      state_.edit_cursor_col = static_cast<int>(lines[state_.edit_cursor_line - 1].length());
      lines[state_.edit_cursor_line - 1] += lines[state_.edit_cursor_line];
      lines.erase(lines.begin() + state_.edit_cursor_line);
      state_.edit_cursor_line--;
    }
    state_.edit_has_changes = true;
    rebuildEditContent(lines);
    return;
  }
}

void TUIApp::rebuildEditContent(const std::vector<std::string>& lines) {
  std::ostringstream oss;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) oss << "\n";
    oss << lines[i];
  }
  state_.edit_content = oss.str();
}

void TUIApp::saveEditedNote() {
  try {
    if (state_.notes.empty() || state_.selected_note_index >= static_cast<int>(state_.notes.size())) {
      setStatusMessage("No note selected to save");
      return;
    }
    
    const auto& metadata = state_.notes[static_cast<size_t>(state_.selected_note_index)];
    
    // Load the current note
    auto note_result = note_store_.load(metadata.id());
    if (!note_result) {
      setStatusMessage("Error loading note for save: " + note_result.error().message());
      return;
    }
    
    // Update content
    auto note = *note_result;
    note.setContent(state_.edit_content);
    
    // Save the note
    auto save_result = note_store_.store(note);
    if (!save_result) {
      setStatusMessage("Error saving note: " + save_result.error().message());
      return;
    }
    
    // Update search index
    auto index_result = search_index_.updateNote(note);
    if (!index_result) {
      setStatusMessage("Warning: Failed to update search index");
    }
    
    // Exit edit mode
    state_.edit_mode_active = false;
    state_.edit_content.clear();
    state_.edit_has_changes = false;
    
    // Refresh data to reflect changes
    refreshData();
    
    setStatusMessage("Note saved successfully");
    
  } catch (const std::exception& e) {
    setStatusMessage("Error saving note: " + std::string(e.what()));
  }
}

Element TUIApp::renderNewNoteModal() const {
  if (!state_.new_note_modal_open) {
    return text("");
  }
  
  Elements modal_content;
  
  std::string modal_title = state_.rename_mode_active ? "Rename Note" : "New Note";
  modal_content.push_back(text(modal_title) | bold | center);
  modal_content.push_back(separator());
  modal_content.push_back(text(""));
  
  // Title input
  std::string title_display = state_.new_note_title.empty() ? 
    "[Enter note title]" : state_.new_note_title;
  modal_content.push_back(
    hbox({
      text("Title: "),
      text(title_display) | 
      (state_.new_note_title.empty() ? dim : (bgcolor(Color::White) | color(Color::Black)))
    })
  );
  modal_content.push_back(separator());
  modal_content.push_back(text(""));
  
  // Future enhancements could add:
  // - Notebook selection
  // - Tag input
  // - Template selection
  
  std::string action_text = state_.rename_mode_active ? 
    "Press Enter to rename, Esc to cancel" : 
    "Press Enter to create, Esc to cancel";
  modal_content.push_back(text(action_text) | center | dim);
  
  return vbox(modal_content) |
         border |
         size(WIDTH, GREATER_THAN, 40) |
         size(WIDTH, LESS_THAN, 60) |
         size(HEIGHT, GREATER_THAN, 8) |
         size(HEIGHT, LESS_THAN, 15) |
         bgcolor(Color::DarkBlue) |
         color(Color::White);
}

void TUIApp::suggestTagsForAllNotes() {
  // Check if AI is configured
  if (!config_.ai.has_value()) {
    setStatusMessage("AI not configured - check config file");
    return;
  }
  
  const auto& ai_config = config_.ai.value();
  if (ai_config.provider != "anthropic") {
    setStatusMessage("Only Anthropic provider is currently supported");
    return;
  }
  
  if (ai_config.api_key.empty()) {
    setStatusMessage("AI API key not configured");
    return;
  }
  
  setStatusMessage("Starting AI tag suggestion for all notes...");
  
  int processed = 0;
  int updated = 0;
  int errors = 0;
  
  // Process all notes in all_notes (unfiltered list)
  for (const auto& metadata : state_.all_notes) {
    auto note_result = note_store_.load(metadata.id());
    if (!note_result.has_value()) {
      errors++;
      continue;
    }
    
    auto note = note_result.value();
    
    // Skip notes that already have tags to avoid overwriting manual tags
    if (!note.metadata().tags().empty()) {
      processed++;
      continue;
    }
    
    // Suggest tags using AI
    auto tags_result = suggestTagsForNote(note, ai_config);
    if (tags_result.has_value() && !tags_result.value().empty()) {
      // Apply the suggested tags to the note
      auto updated_metadata = note.metadata();
      for (const auto& tag : tags_result.value()) {
        updated_metadata.addTag(tag);
      }
      
      // Create updated note and save
      nx::core::Note updated_note(std::move(updated_metadata), note.content());
      auto store_result = note_store_.store(updated_note);
      
      if (store_result.has_value()) {
        // Update search index
        auto index_result = search_index_.addNote(updated_note);
        if (index_result.has_value()) {
          updated++;
        } else {
          errors++;
        }
      } else {
        errors++;
      }
    } else {
      errors++;
    }
    
    processed++;
    
    // Update status every 5 notes
    if (processed % 5 == 0) {
      setStatusMessage("AI tagging progress: " + std::to_string(processed) + "/" + 
                       std::to_string(state_.all_notes.size()) + " processed");
    }
  }
  
  // Reload data to reflect changes
  loadNotes();
  loadTags();
  applyFilters();
  
  std::string final_message = "AI tagging complete: " + std::to_string(updated) + 
                             " notes updated, " + std::to_string(errors) + " errors";
  setStatusMessage(final_message);
}

void TUIApp::aiAutoTagSelectedNote() {
  // Check if AI is configured
  if (!config_.ai.has_value()) {
    setStatusMessage("AI not configured - check config file");
    return;
  }
  
  const auto& ai_config = config_.ai.value();
  if (ai_config.provider != "anthropic") {
    setStatusMessage("Only Anthropic provider is currently supported");
    return;
  }
  
  if (ai_config.api_key.empty()) {
    setStatusMessage("AI API key not configured");
    return;
  }
  
  // Check if a note is selected and we're in the notes panel
  if (state_.current_pane != ActivePane::Notes || state_.notes.empty() || 
      state_.selected_note_index < 0 || 
      static_cast<size_t>(state_.selected_note_index) >= state_.notes.size()) {
    setStatusMessage("Select a note in the notes panel to auto-tag");
    return;
  }
  
  const auto& selected_metadata = state_.notes[static_cast<size_t>(state_.selected_note_index)];
  
  // Load the full note
  auto note_result = note_store_.load(selected_metadata.id());
  if (!note_result.has_value()) {
    setStatusMessage("Error loading selected note: " + note_result.error().message());
    return;
  }
  
  auto note = note_result.value();
  
  setStatusMessage("Generating AI tags for selected note...");
  
  // Suggest tags using AI
  auto tags_result = suggestTagsForNote(note, ai_config);
  if (!tags_result.has_value()) {
    setStatusMessage("Error generating AI tags: " + tags_result.error().message());
    return;
  }
  
  auto suggested_tags = tags_result.value();
  
  if (suggested_tags.empty()) {
    setStatusMessage("No AI tag suggestions generated for this note");
    return;
  }
  
  // Add suggested tags to existing tags (don't replace them)
  auto updated_metadata = note.metadata();
  std::set<std::string> existing_tags_set(note.metadata().tags().begin(), note.metadata().tags().end());
  
  int new_tags_added = 0;
  for (const auto& tag : suggested_tags) {
    if (existing_tags_set.find(tag) == existing_tags_set.end()) {
      updated_metadata.addTag(tag);
      new_tags_added++;
    }
  }
  
  if (new_tags_added == 0) {
    setStatusMessage("No new AI tags to add - note already has suggested tags");
    return;
  }
  
  updated_metadata.touch(); // Update modified time
  
  // Create updated note and save
  nx::core::Note updated_note(std::move(updated_metadata), note.content());
  auto store_result = note_store_.store(updated_note);
  if (!store_result.has_value()) {
    setStatusMessage("Error saving AI tags: " + store_result.error().message());
    return;
  }
  
  // Update search index
  auto index_result = search_index_.updateNote(updated_note);
  if (!index_result.has_value()) {
    // Non-fatal - warn but continue
    setStatusMessage("Warning: Failed to update search index: " + index_result.error().message());
  }
  
  // Reload data to reflect changes
  loadNotes();
  loadTags();
  applyFilters();
  
  // Show success message with tags added
  std::string tag_list;
  for (size_t i = 0; i < suggested_tags.size() && i < 3; ++i) {
    if (i > 0) tag_list += ", ";
    tag_list += suggested_tags[i];
  }
  if (suggested_tags.size() > 3) {
    tag_list += "...";
  }
  
  setStatusMessage("Added " + std::to_string(new_tags_added) + " AI tags: " + tag_list);
}

void TUIApp::aiAutoTitleSelectedNote() {
  // Check if AI is configured
  if (!config_.ai.has_value()) {
    setStatusMessage("AI not configured - check config file");
    return;
  }
  
  const auto& ai_config = config_.ai.value();
  if (ai_config.provider != "anthropic") {
    setStatusMessage("Only Anthropic provider is currently supported");
    return;
  }
  
  if (ai_config.api_key.empty()) {
    setStatusMessage("AI API key not configured");
    return;
  }
  
  // Check if a note is selected and we're in the notes panel
  if (state_.current_pane != ActivePane::Notes || state_.notes.empty() || 
      state_.selected_note_index < 0 || 
      static_cast<size_t>(state_.selected_note_index) >= state_.notes.size()) {
    setStatusMessage("Select a note in the notes panel to auto-title");
    return;
  }
  
  const auto& selected_metadata = state_.notes[static_cast<size_t>(state_.selected_note_index)];
  
  // Load the full note
  auto note_result = note_store_.load(selected_metadata.id());
  if (!note_result.has_value()) {
    setStatusMessage("Error loading selected note: " + note_result.error().message());
    return;
  }
  
  auto note = note_result.value();
  
  setStatusMessage("Generating AI title for selected note...");
  
  // Suggest title using AI
  auto title_result = suggestTitleForNote(note, ai_config);
  if (!title_result.has_value()) {
    setStatusMessage("Error generating AI title: " + title_result.error().message());
    return;
  }
  
  auto suggested_title = title_result.value();
  
  if (suggested_title.empty() || suggested_title == note.title()) {
    setStatusMessage("No new AI title suggestion generated");
    return;
  }
  
  // Update note with new title
  auto updated_metadata = note.metadata();
  std::string old_title = updated_metadata.title();
  updated_metadata.setTitle(suggested_title);
  updated_metadata.touch(); // Update modified time
  
  // Update content if it starts with a title heading
  std::string updated_content = note.content();
  if (updated_content.starts_with("# ")) {
    // Replace the first line (title heading)
    size_t first_newline = updated_content.find('\n');
    if (first_newline != std::string::npos) {
      updated_content = "# " + suggested_title + updated_content.substr(first_newline);
    } else {
      updated_content = "# " + suggested_title;
    }
  }
  
  // Create updated note and save
  nx::core::Note updated_note(std::move(updated_metadata), updated_content);
  auto store_result = note_store_.store(updated_note);
  if (!store_result.has_value()) {
    setStatusMessage("Error saving AI title: " + store_result.error().message());
    return;
  }
  
  // Update search index
  auto index_result = search_index_.updateNote(updated_note);
  if (!index_result.has_value()) {
    // Non-fatal - warn but continue
    setStatusMessage("Warning: Failed to update search index: " + index_result.error().message());
  }
  
  // Reload data to reflect changes
  loadNotes();
  loadTags();
  applyFilters();
  
  // Show success message with new title
  std::string display_title = suggested_title;
  if (display_title.length() > 50) {
    display_title = display_title.substr(0, 47) + "...";
  }
  
  setStatusMessage("AI title updated: \"" + display_title + "\"");
}

Result<std::vector<std::string>> TUIApp::suggestTagsForNote(const nx::core::Note& note, 
                                                           const nx::config::Config::AiConfig& ai_config) {
  // Get existing tags from all notes for consistency
  std::set<std::string> existing_tags_set;
  for (const auto& metadata : state_.all_notes) {
    for (const auto& tag : metadata.tags()) {
      existing_tags_set.insert(tag);
    }
  }
  
  std::vector<std::string> existing_tags(existing_tags_set.begin(), existing_tags_set.end());
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 512;
  
  std::string system_prompt = "You are a helpful assistant that suggests relevant tags for notes. "
                             "Analyze the note content and suggest 3-5 concise, relevant tags. "
                             "Tags should be lowercase, single words or short phrases with hyphens. "
                             "Return only a JSON array of tag strings, no other text.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  
  std::string context = "Note title: " + note.title() + "\n\nNote content:\n" + note.content();
  
  if (!existing_tags.empty()) {
    context += "\n\nExisting tags in the collection (for consistency): ";
    for (size_t i = 0; i < existing_tags.size() && i < 20; ++i) {
      if (i > 0) context += ", ";
      context += existing_tags[i];
    }
  }
  
  context += "\n\nSuggest 3-5 relevant tags for this note:";
  user_message["content"] = context;
  messages.push_back(user_message);
  
  request_body["messages"] = messages;
  
  // Make HTTP request to Anthropic API
  nx::util::HttpClient client;
  
  std::vector<std::string> headers = {
    "Content-Type: application/json",
    "x-api-key: " + ai_config.api_key,
    "anthropic-version: 2023-06-01"
  };
  
  auto response = client.post("https://api.anthropic.com/v1/messages", 
                             request_body.dump(), headers);
  
  if (!response.has_value()) {
    return std::unexpected(makeError(ErrorCode::kNetworkError, 
                                   "Failed to call Anthropic API"));
  }
  
  if (response->status_code != 200) {
    return std::unexpected(makeError(ErrorCode::kNetworkError, 
                                   "Anthropic API returned error " + std::to_string(response->status_code)));
  }
  
  // Parse response
  try {
    auto response_json = nlohmann::json::parse(response->body);
    
    if (!response_json.contains("content") || !response_json["content"].is_array() || 
        response_json["content"].empty()) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Invalid response format from Anthropic API"));
    }
    
    auto& content_obj = response_json["content"][0];
    if (!content_obj.contains("text") || !content_obj["text"].is_string()) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Missing text content in Anthropic API response"));
    }
    
    std::string ai_response = content_obj["text"].get<std::string>();
    
    // Try to parse the AI response as JSON array
    try {
      auto tags_json = nlohmann::json::parse(ai_response);
      if (!tags_json.is_array()) {
        return std::unexpected(makeError(ErrorCode::kParseError, 
                                       "AI response is not a JSON array"));
      }
      
      std::vector<std::string> suggestions;
      for (const auto& tag_json : tags_json) {
        if (tag_json.is_string()) {
          suggestions.push_back(tag_json.get<std::string>());
        }
      }
      
      return suggestions;
      
    } catch (const nlohmann::json::parse_error&) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Failed to parse AI response as JSON"));
    }
    
  } catch (const nlohmann::json::parse_error&) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse API response"));
  }
}

Result<std::string> TUIApp::suggestTitleForNote(const nx::core::Note& note, 
                                               const nx::config::Config::AiConfig& ai_config) {
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 128;
  
  std::string system_prompt = "You are a helpful assistant that generates concise, descriptive titles for notes based on their content. "
                             "Analyze the provided content and suggest a single, clear title that captures the main topic or purpose. "
                             "The title should be specific and informative. Return only the title text, no quotes or extra formatting.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  
  // Limit content length to avoid token limits
  std::string limited_content = note.content();
  if (limited_content.length() > 2000) {
    limited_content = limited_content.substr(0, 2000) + "...";
  }
  
  std::string context = "Current title: " + note.title() + "\n\nNote content:\n" + limited_content + 
                       "\n\nGenerate a better, more descriptive title for this note:";
  user_message["content"] = context;
  messages.push_back(user_message);
  
  request_body["messages"] = messages;
  
  // Make HTTP request to Anthropic API
  nx::util::HttpClient client;
  
  std::vector<std::string> headers = {
    "Content-Type: application/json",
    "x-api-key: " + ai_config.api_key,
    "anthropic-version: 2023-06-01"
  };
  
  auto response = client.post("https://api.anthropic.com/v1/messages", 
                             request_body.dump(), headers);
  
  if (!response.has_value()) {
    return std::unexpected(makeError(ErrorCode::kNetworkError, 
                                   "Failed to call Anthropic API: " + response.error().message()));
  }
  
  if (response->status_code != 200) {
    return std::unexpected(makeError(ErrorCode::kNetworkError, 
                                   "Anthropic API returned error " + std::to_string(response->status_code) + 
                                   ": " + response->body));
  }
  
  try {
    nlohmann::json response_json = nlohmann::json::parse(response->body);
    
    if (response_json.contains("error")) {
      std::string error_message = "Anthropic API error";
      if (response_json["error"].contains("message")) {
        error_message = response_json["error"]["message"];
      }
      return std::unexpected(makeError(ErrorCode::kNetworkError, error_message));
    }
    
    if (!response_json.contains("content") || !response_json["content"].is_array() || 
        response_json["content"].empty()) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Invalid response format from Anthropic API"));
    }
    
    auto content_item = response_json["content"][0];
    if (!content_item.contains("text")) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "No text content in Anthropic API response"));
    }
    
    std::string generated_title = content_item["text"];
    
    // Clean up the title - remove quotes and trim whitespace
    if (generated_title.front() == '"' && generated_title.back() == '"') {
      generated_title = generated_title.substr(1, generated_title.length() - 2);
    }
    
    // Trim whitespace
    generated_title.erase(0, generated_title.find_first_not_of(" \t\n\r"));
    generated_title.erase(generated_title.find_last_not_of(" \t\n\r") + 1);
    
    // Limit title length
    if (generated_title.length() > 100) {
      generated_title = generated_title.substr(0, 100);
    }
    
    return generated_title;
    
  } catch (const nlohmann::json::parse_error& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse Anthropic API response: " + std::string(e.what())));
  }
}

// Tag management operations
Result<void> TUIApp::addTagsToNote(const nx::core::NoteId& note_id, const std::vector<std::string>& tags) {
  auto note_result = note_store_.load(note_id);
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }

  auto note = *note_result;
  auto metadata = note.metadata();
  
  for (const auto& tag : tags) {
    if (!metadata.hasTag(tag)) {
      metadata.addTag(tag);
    }
  }

  nx::core::Note updated_note(std::move(metadata), note.content());
  auto store_result = note_store_.store(updated_note);
  if (!store_result.has_value()) {
    return std::unexpected(store_result.error());
  }

  // Update search index
  auto index_result = search_index_.updateNote(updated_note);
  if (!index_result.has_value()) {
    // Non-fatal, just log warning
  }

  return {};
}

Result<void> TUIApp::removeTagsFromNote(const nx::core::NoteId& note_id, const std::vector<std::string>& tags) {
  auto note_result = note_store_.load(note_id);
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }

  auto note = *note_result;
  auto metadata = note.metadata();
  
  for (const auto& tag : tags) {
    if (metadata.hasTag(tag)) {
      metadata.removeTag(tag);
    }
  }

  nx::core::Note updated_note(std::move(metadata), note.content());
  auto store_result = note_store_.store(updated_note);
  if (!store_result.has_value()) {
    return std::unexpected(store_result.error());
  }

  // Update search index
  auto index_result = search_index_.updateNote(updated_note);
  if (!index_result.has_value()) {
    // Non-fatal, just log warning
  }

  return {};
}

Result<void> TUIApp::setTagsForNote(const nx::core::NoteId& note_id, const std::vector<std::string>& tags) {
  auto note_result = note_store_.load(note_id);
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }

  auto note = *note_result;
  auto metadata = note.metadata();
  
  // Replace all tags
  metadata.setTags(tags);

  nx::core::Note updated_note(std::move(metadata), note.content());
  auto store_result = note_store_.store(updated_note);
  if (!store_result.has_value()) {
    return std::unexpected(store_result.error());
  }

  // Update search index
  auto index_result = search_index_.updateNote(updated_note);
  if (!index_result.has_value()) {
    // Non-fatal, just log warning
  }

  return {};
}

void TUIApp::openTagEditModal(const nx::core::NoteId& note_id) {
  // Load current tags for the note
  auto note_result = note_store_.load(note_id);
  if (!note_result.has_value()) {
    setStatusMessage("Error loading note for tag editing");
    return;
  }

  auto note = *note_result;
  const auto& current_tags = note.metadata().tags();
  
  // Build comma-separated string of current tags
  std::string tag_string;
  for (size_t i = 0; i < current_tags.size(); ++i) {
    if (i > 0) tag_string += ", ";
    tag_string += current_tags[i];
  }
  
  state_.tag_edit_modal_open = true;
  state_.tag_edit_note_id = note_id;
  state_.tag_edit_input = tag_string;
  setStatusMessage("Edit tags (comma-separated). Enter to save, Esc to cancel");
}

Element TUIApp::renderTagEditModal() const {
  if (!state_.tag_edit_modal_open) {
    return text("");
  }
  
  Elements modal_content;
  
  modal_content.push_back(text("Edit Tags") | bold | center);
  modal_content.push_back(separator());
  modal_content.push_back(text(""));
  
  // Current note info
  if (state_.tag_edit_note_id.isValid()) {
    auto note_result = note_store_.load(state_.tag_edit_note_id);
    if (note_result.has_value()) {
      modal_content.push_back(
        hbox({
          text("Note: "),
          text(note_result->title()) | bold
        })
      );
      modal_content.push_back(text(""));
    }
  }
  
  // Tag input
  std::string input_display = state_.tag_edit_input.empty() ? 
    "[Enter tags, comma-separated]" : state_.tag_edit_input;
  modal_content.push_back(
    hbox({
      text("Tags: "),
      text(input_display) | 
      (state_.tag_edit_input.empty() ? dim : (bgcolor(Color::White) | color(Color::Black)))
    })
  );
  modal_content.push_back(separator());
  modal_content.push_back(text(""));
  
  modal_content.push_back(text("Press Enter to save, Esc to cancel") | center | dim);
  modal_content.push_back(text("Example: work, urgent, project-alpha") | center | dim);
  
  return vbox(modal_content) |
         border |
         size(WIDTH, GREATER_THAN, 50) |
         size(WIDTH, LESS_THAN, 80) |
         bgcolor(Color::DarkBlue) |
         color(Color::White);
}

void TUIApp::resizeNotesPanel(int delta) {
  if (panel_sizing_.resizeNotes(delta)) {
    // Panel was successfully resized, provide user feedback
    std::string direction = delta > 0 ? "expanded" : "narrowed";
    setStatusMessage("Notes panel " + direction + " (Notes: " + 
                    std::to_string(panel_sizing_.notes_width) + "%, Preview: " + 
                    std::to_string(panel_sizing_.preview_width) + "%)");
  } else {
    // Cannot resize further due to minimum constraints
    std::string reason = delta > 0 ? 
      "Cannot expand further (preview panel at minimum width)" :
      "Cannot narrow further (notes panel at minimum width)";
    setStatusMessage(reason);
  }
}

int TUIApp::calculateVisibleTagsCount() const {
  // Show all tags unless we have a huge number
  // This lets FTXUI's layout engine handle the space naturally
  
  int tag_count = static_cast<int>(state_.tags.size());
  
  // Only limit if we have more than 30 tags (to prevent performance issues)
  if (tag_count <= 30) {
    return tag_count; // Show all tags
  }
  
  // For many tags, calculate based on terminal height
  int terminal_height = screen_.dimy();
  int max_tags = std::max(15, terminal_height - 8);
  
  return std::min(tag_count, max_tags);
}

} // namespace nx::tui