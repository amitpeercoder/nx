# nx MVP2 Comprehensive Implementation Plan

**Document Version**: 1.0  
**Target Completion**: MVP2 Phase 1-4  
**Based on**: Existing codebase analysis and MVP2 requirements

## 🎯 Executive Summary

This document provides a complete implementation roadmap for nx MVP2, building on the solid MVP1 foundation. The plan leverages existing architecture patterns and infrastructure to efficiently deliver the remaining specification requirements.

### Current State Analysis
- **MVP1 Status**: ✅ Complete with 22+ commands implemented
- **Infrastructure Ready**: AttachmentStore, export system, command framework
- **Architecture**: Clean modular C++ design with proper error handling
- **Missing**: 15+ core commands + advanced features for power users

### 🎨 Major UX Enhancement: Hierarchical Notebook Navigation

**Key Innovation**: Transform the left TUI panel from simple tags list into a powerful hierarchical navigation system:

```
NOTEBOOKS
▼ Work (15) ✓         <- Expanded, selected notebook
  #meeting (3) ✓      <- Tag within Work, selected
  #urgent (2)         <- Tag within Work
  #project (5)        <- Tag within Work
▶ Personal (8)        <- Collapsed notebook
▶ Projects (12)       <- Collapsed notebook

ALL TAGS              <- Global tags section
#meeting (7)          <- Total across all notebooks
#urgent (4)           
#done (10)
```

**Benefits**:
- **Natural Organization**: Notebooks → Tags → Notes mirrors user mental models
- **Powerful Filtering**: Combine notebook + tag filters with AND/OR logic
- **Context Preservation**: Always know which organizational scope you're in
- **Progressive Disclosure**: Expandable sections keep UI clean while providing power
- **Backward Compatible**: Users can ignore notebooks and use global tags as before

---

## 🏗️ Architecture Foundation

### Existing Patterns to Follow

**Command Structure** (from `include/nx/cli/application.hpp`):
```cpp
class Command {
public:
  virtual Result<int> execute(const GlobalOptions& options) = 0;
  virtual std::string name() const = 0;
  virtual std::string description() const = 0;
  virtual void setupCommand(CLI::App* cmd) {}
};
```

**Service Access Pattern**:
- `Application::noteStore()` - Note CRUD operations
- `Application::searchIndex()` - Search and indexing
- `Application::config()` - Configuration management
- `Application::attachmentStore()` - Attachment operations (needs adding)
- `Application::templateStore()` - Template management (needs adding)
- `Application::notebookManager()` - Notebook operations (needs adding)

**Error Handling Pattern**:
```cpp
Result<T> function() {
  auto result = someOperation();
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }
  return result.value();
}
```

---

## 📋 Phase 1: Core MVP Completion (1-2 weeks)

### 1.1 Attachment Management (`nx attach`)

**Status**: 🟡 Backend exists, CLI missing  
**Infrastructure**: `FilesystemAttachmentStore` fully implemented  
**Location**: `src/cli/commands/attach_command.cpp` (to be created)

#### Implementation Details

**Command Specification**:
```bash
nx attach <note_id> <file_path> [--description "desc"] [--json]
```

**File Structure**:
```cpp
// include/nx/cli/commands/attach_command.hpp
class AttachCommand : public Command {
public:
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "attach"; }
  std::string description() const override { return "Attach file to note"; }
  void setupCommand(CLI::App* cmd) override;

private:
  std::string note_id_;
  std::string file_path_;
  std::string description_;
};
```

**Implementation Steps**:
1. Create `AttachCommand` class following existing pattern
2. Add CLI parsing for note ID, file path, optional description
3. Integrate with existing `AttachmentStore` interface
4. Add JSON output support for scripting
5. Add to `Application::setupCommands()`

**Code Template** (based on existing commands):
```cpp
Result<int> AttachCommand::execute(const GlobalOptions& options) {
  auto& app = Application::instance();
  auto& store = app.noteStore();
  auto& attachment_store = app.attachmentStore(); // New accessor needed
  
  // Resolve note ID (fuzzy matching like existing commands)
  auto note_result = store.resolveId(note_id_);
  if (!note_result.has_value()) {
    return std::unexpected(Error::noteNotFound(note_id_));
  }
  
  // Store attachment using existing infrastructure
  auto attach_result = attachment_store.store(note_result.value(), 
                                              std::filesystem::path(file_path_), 
                                              description_);
  if (!attach_result.has_value()) {
    return std::unexpected(attach_result.error());
  }
  
  // Output result (JSON or human-readable)
  if (options.json) {
    outputJson(attach_result.value());
  } else {
    outputHuman(attach_result.value());
  }
  
  return 0;
}
```

**TUI Integration**:
- Add attachment viewer in preview panel
- Show attachment list in note details
- Support drag-and-drop attachment in TUI (if supported by terminal)
- Attachment preview for images/text files

**Testing Strategy**:
- Unit tests for command parsing
- Integration tests with real files
- Error handling tests (missing files, invalid notes)
- TUI integration tests for attachment display

---

### 1.2 Import System (`nx import dir`)

**Status**: 🔴 Not implemented  
**Infrastructure**: Export system provides pattern to follow  
**Location**: `src/cli/commands/import_command.cpp` (to be created)

#### Implementation Details

**Command Specification**:
```bash
nx import dir <directory> [--notebook name] [--recursive] [--extensions md,txt] [--json]
```

**Architecture** (mirror export system):
```cpp
// include/nx/import_export/import_manager.hpp
class ImportManager {
public:
  struct ImportOptions {
    std::filesystem::path source_dir;
    std::string target_notebook = "imported";
    bool recursive = true;
    std::vector<std::string> extensions = {"md", "txt", "markdown"};
    bool preserve_structure = true;
  };
  
  struct ImportResult {
    size_t notes_imported = 0;
    size_t files_skipped = 0;
    std::vector<std::string> errors;
    std::vector<nx::core::NoteId> created_notes;
  };
  
  Result<ImportResult> importDirectory(const ImportOptions& options);
  
private:
  Result<nx::core::Note> parseMarkdownFile(const std::filesystem::path& file);
  Result<nx::core::Note> parseTextFile(const std::filesystem::path& file);
  bool shouldImportFile(const std::filesystem::path& file, const ImportOptions& options);
};
```

**File Processing Logic**:
```cpp
Result<nx::core::Note> ImportManager::parseMarkdownFile(const std::filesystem::path& file) {
  // Read file content
  auto content_result = readFileContent(file);
  if (!content_result.has_value()) {
    return std::unexpected(content_result.error());
  }
  
  // Parse YAML front-matter if present
  auto [metadata, markdown_content] = parseYamlFrontMatter(content_result.value());
  
  // Create note with new ULID
  auto note_id = nx::core::NoteId::generate();
  auto note = nx::core::Note::create(note_id);
  
  // Set metadata from file or generate defaults
  note.setTitle(metadata.title.empty() ? 
                filenameToTitle(file.filename()) : 
                metadata.title);
  note.setContent(markdown_content);
  note.setTags(metadata.tags);
  note.setNotebook(metadata.notebook);
  
  // Preserve file timestamps if possible
  auto file_time = std::filesystem::last_write_time(file);
  note.setCreated(fileTimeToSystemTime(file_time));
  note.setModified(fileTimeToSystemTime(file_time));
  
  return note;
}
```

**Import Variants**:
```cpp
// Multiple import command implementations
class ImportDirCommand : public Command { /* ... */ };
class ImportFileCommand : public Command { /* ... */ };
class ImportObsidianCommand : public Command { /* ... */ };
class ImportNotionCommand : public Command { /* ... */ };

// Or unified command with subcommands
class ImportCommand : public Command {
  std::string import_type_; // "dir", "file", "obsidian", "notion"
  // ...
};
```

**Special Format Handlers**:
```cpp
class ObsidianImporter {
public:
  Result<ImportResult> importVault(const std::filesystem::path& vault_path);
private:
  Result<void> convertWikiLinks(std::string& content);
  Result<void> handleAttachments(const std::filesystem::path& vault_path);
};

class NotionImporter {
public:
  Result<ImportResult> importExport(const std::filesystem::path& export_path);
private:
  Result<void> parseNotionJSON(const nlohmann::json& data);
  Result<void> convertNotionBlocks(const nlohmann::json& blocks);
};
```

**Implementation Steps**:
1. Create `ImportManager` class in `src/import_export/`
2. Implement file discovery and filtering
3. Add YAML front-matter parsing
4. Handle directory structure → notebook mapping
5. Create specialized importers for Obsidian and Notion
6. Create `ImportCommand` with subcommand support
7. Add progress reporting for large imports
8. Add TUI integration for import progress

---

### 1.3 Template Management (`nx tpl`)

**Status**: 🟡 Basic support exists in `new --from`  
**Infrastructure**: Template storage needs implementation  
**Location**: `src/cli/commands/template_command.cpp` (to be created)

#### Implementation Details

**Command Specification**:
```bash
nx tpl add <name> <template_file>     # Add template
nx tpl list [--json]                  # List templates  
nx tpl show <name>                    # Display template
nx tpl remove <name>                  # Remove template
```

**Template Storage** (follow XDG pattern):
```cpp
// include/nx/store/template_store.hpp
class TemplateStore {
public:
  struct Template {
    std::string name;
    std::string content;
    std::map<std::string, std::string> variables; // ${var} substitutions
    std::chrono::system_clock::time_point created;
  };
  
  Result<void> addTemplate(const std::string& name, const std::filesystem::path& file);
  Result<void> removeTemplate(const std::string& name);
  Result<Template> getTemplate(const std::string& name);
  Result<std::vector<std::string>> listTemplates();
  Result<std::string> renderTemplate(const std::string& name, 
                                     const std::map<std::string, std::string>& vars);

private:
  std::filesystem::path templates_dir_; // ~/.local/share/nx/.nx/templates/
};
```

**Template Format** (with variable substitution):
```markdown
---
title: "${title}"
tags: [${tags}]
notebook: "${notebook}"
---

# ${title}

## Meeting Details
- **Date**: ${date}
- **Attendees**: ${attendees}

## Agenda
${agenda}

## Notes
${notes}

## Action Items
- [ ] 
```

**Command Implementation**:
```cpp
// Subcommand structure like git
class TemplateCommand : public Command {
public:
  Result<int> execute(const GlobalOptions& options) override;
  void setupCommand(CLI::App* cmd) override;

private:
  std::string subcommand_;
  std::string template_name_;
  std::string template_file_;
  
  Result<int> executeAdd();
  Result<int> executeList();
  Result<int> executeShow();
  Result<int> executeRemove();
};
```

**Integration with `new` command**:
- Extend existing `new --from` to use TemplateStore
- Add variable substitution with prompts
- Support for default templates

**TUI Integration**:
- Template selector in new note dialog
- Template preview in TUI
- Quick template creation from TUI
- Template variable input forms

---

### 1.4 Metadata Management (`nx meta`)

**Status**: 🔴 Not implemented  
**Infrastructure**: Note model supports arbitrary metadata  
**Location**: `src/cli/commands/meta_command.cpp` (to be created)

#### Implementation Details

**Command Specification**:
```bash
nx meta <note_id>                    # Show all metadata
nx meta <note_id> --set key=value    # Set metadata field
nx meta <note_id> --get key          # Get specific field
nx meta <note_id> --delete key       # Remove field
nx meta <note_id> --json             # JSON output
```

**Implementation Pattern** (similar to `view_command.cpp`):
```cpp
class MetaCommand : public Command {
public:
  Result<int> execute(const GlobalOptions& options) override;
  void setupCommand(CLI::App* cmd) override;

private:
  std::string note_id_;
  std::vector<std::string> set_operations_;  // key=value pairs
  std::vector<std::string> get_keys_;
  std::vector<std::string> delete_keys_;
  
  Result<int> executeShow();
  Result<int> executeSet();
  Result<int> executeGet();
  Result<int> executeDelete();
};
```

**Metadata Operations**:
```cpp
Result<int> MetaCommand::executeSet() {
  auto& store = Application::instance().noteStore();
  
  // Load note
  auto note_result = store.load(note_id_);
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }
  auto note = note_result.value();
  
  // Apply set operations
  for (const auto& operation : set_operations_) {
    auto [key, value] = parseKeyValue(operation);
    
    // Handle special fields
    if (key == "title") {
      note.setTitle(value);
    } else if (key == "tags") {
      note.setTags(parseTagList(value));
    } else if (key == "notebook") {
      note.setNotebook(value);
    } else {
      // Custom metadata field
      note.setCustomField(key, value);
    }
  }
  
  // Save note
  auto save_result = store.save(note);
  if (!save_result.has_value()) {
    return std::unexpected(save_result.error());
  }
  
  return 0;
}
```

---

### 1.5 Notebook Management (`nx nb`) + Hierarchical TUI

**Status**: 🔴 Not implemented  
**Infrastructure**: Note model supports notebooks, needs management layer + TUI transformation  
**Location**: `src/cli/commands/notebook_command.cpp` (to be created) + TUI enhancements

#### Implementation Details

**Command Specification**:
```bash
nx nb list [--json]                   # List all notebooks with note counts
nx nb create <name>                   # Create new notebook
nx nb rename <old> <new>              # Rename notebook
nx nb delete <name> [--force]         # Delete notebook (with safety checks)
nx nb info <name>                     # Show notebook statistics
```

**Notebook Manager Architecture**:
```cpp
// include/nx/store/notebook_manager.hpp
class NotebookManager {
public:
  struct NotebookInfo {
    std::string name;
    size_t note_count;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_modified;
    std::vector<std::string> tags; // most common tags in notebook
  };
  
  Result<void> createNotebook(const std::string& name);
  Result<void> deleteNotebook(const std::string& name, bool force = false);
  Result<void> renameNotebook(const std::string& old_name, const std::string& new_name);
  Result<std::vector<NotebookInfo>> listNotebooks();
  Result<NotebookInfo> getNotebookInfo(const std::string& name);
  Result<bool> notebookExists(const std::string& name);
  
private:
  nx::store::NoteStore& note_store_;
};
```

**Command Implementation**:
```cpp
class NotebookCommand : public Command {
public:
  Result<int> execute(const GlobalOptions& options) override;
  void setupCommand(CLI::App* cmd) override;

private:
  std::string subcommand_;
  std::string notebook_name_;
  std::string new_name_;
  bool force_ = false;
  
  Result<int> executeList();
  Result<int> executeCreate();
  Result<int> executeRename();
  Result<int> executeDelete();
  Result<int> executeInfo();
};
```

**Safety Checks for Delete**:
```cpp
Result<int> NotebookCommand::executeDelete() {
  auto& manager = Application::instance().notebookManager();
  
  // Check if notebook exists
  auto info_result = manager.getNotebookInfo(notebook_name_);
  if (!info_result.has_value()) {
    return std::unexpected(Error::notebookNotFound(notebook_name_));
  }
  
  // Safety check: prevent deletion of non-empty notebooks without --force
  if (info_result.value().note_count > 0 && !force_) {
    std::cerr << "Error: Notebook '" << notebook_name_ << "' contains " 
              << info_result.value().note_count << " notes.\n";
    std::cerr << "Use --force to delete anyway, or move notes to another notebook first.\n";
    return 1;
  }
  
  // Confirm deletion in interactive mode
  if (!force_ && isatty(STDIN_FILENO)) {
    std::cout << "Delete notebook '" << notebook_name_ << "'? [y/N]: ";
    std::string response;
    std::getline(std::cin, response);
    if (response != "y" && response != "Y") {
      std::cout << "Aborted.\n";
      return 0;
    }
  }
  
  auto delete_result = manager.deleteNotebook(notebook_name_, force_);
  if (!delete_result.has_value()) {
    return std::unexpected(delete_result.error());
  }
  
  std::cout << "Notebook '" << notebook_name_ << "' deleted.\n";
  return 0;
}
```

**TUI Integration - Hierarchical Navigation Panel**:
- **Primary Enhancement**: Transform left panel into hierarchical navigation:
  ```
  NOTEBOOKS
  ▼ Work (15) ✓         <- Expanded, selected notebook
    #meeting (3) ✓      <- Tag within Work, selected
    #urgent (2)         <- Tag within Work
    #project (5)        <- Tag within Work
  ▶ Personal (8)        <- Collapsed notebook
  ▶ Projects (12)       <- Collapsed notebook

  ALL TAGS              <- Global tags section
  #meeting (7)          <- Total across all notebooks
  #urgent (4)           
  #done (10)
  ```
- **Smart Filtering Logic**:
  - Notebook selection: Filter to notes in that notebook
  - Notebook tag selection: Filter to tagged notes within notebook
  - Global tag selection: Filter across all notebooks
  - Multiple selections: OR within same level, AND across levels
- **Keyboard Navigation**:
  - ↑/↓: Navigate flattened tree items
  - Enter: Toggle notebook expansion or apply filter
  - Space: Toggle selection (multi-select)
  - Tab: Switch to notes panel
  - 'N': Toggle notebooks section
  - 'T': Toggle ALL TAGS section
  - Ctrl+N: Quick notebook switcher (fuzzy finder)
  - Ctrl+Shift+N: Create new notebook dialog
- **Visual Enhancements**:
  - Notebook icons: 📁 (closed) 📂 (open)
  - Selection indicators: ✓ for active filters
  - Color coding: Active filters in green/blue
  - Breadcrumb: "Work > #meeting (3 notes)" in status bar

**Advanced TUI State Management**:
```cpp
// Enhanced TUIState structure for hierarchical navigation
struct NotebookUIInfo {
  std::string name;
  size_t note_count;
  std::vector<std::string> tags;  // Tags within this notebook
  std::map<std::string, size_t> tag_counts;  // Per-tag counts
  bool expanded = false;
  bool selected = false;
};

struct TUIState {
  // Existing fields...
  
  // Notebook navigation
  std::vector<NotebookUIInfo> notebooks;
  int selected_nav_index = 0;  // Index in flattened nav tree
  
  // Navigation items (flattened for keyboard nav)
  enum NavItemType { Notebook, NotebookTag, GlobalTag };
  struct NavItem {
    NavItemType type;
    std::string name;
    std::string parent_notebook;  // Empty for notebooks/global tags
    bool selected = false;
  };
  std::vector<NavItem> nav_items;  // Flattened navigation tree
  
  // Filter state
  std::set<std::string> active_notebooks;
  std::map<std::string, std::set<std::string>> active_notebook_tags;  // notebook -> tags
  std::set<std::string> active_global_tags;
  
  bool show_all_tags_section = true;
};
```

---

### 1.6 Encryption Commands (`nx enc`)

**Status**: 🟡 Backend exists, CLI missing  
**Infrastructure**: Basic encryption exists, needs command interface  
**Location**: `src/cli/commands/encrypt_command.cpp` (to be created)

#### Implementation Details

**Command Specification**:
```bash
nx enc init --age <recipient>         # Initialize encryption with age key
nx enc on                             # Enable encryption mode
nx enc off                            # Disable encryption mode
nx enc status                         # Show encryption status
```

**Enhanced Encryption Manager**:
```cpp
// include/nx/crypto/encryption_manager.hpp
class EncryptionManager {
public:
  struct EncryptionStatus {
    bool enabled = false;
    std::string recipient;
    size_t encrypted_notes = 0;
    size_t total_notes = 0;
    std::string key_file_path;
  };
  
  Result<void> initialize(const std::string& age_recipient);
  Result<void> enableEncryption();
  Result<void> disableEncryption();
  Result<EncryptionStatus> getStatus();
  Result<void> encryptAllNotes();
  Result<void> decryptAllNotes();
  
private:
  bool is_initialized_ = false;
  std::string age_recipient_;
};
```

---

### 1.7 Sync Subcommands (`nx sync`)

**Status**: 🟡 Basic sync exists, subcommands missing  
**Infrastructure**: Git sync foundation exists  
**Location**: Enhance existing `src/cli/commands/sync_command.cpp`

#### Implementation Details

**Command Specification**:
```bash
nx sync init [--remote <url>]         # Initialize git repository
nx sync status                        # Show sync status
nx sync pull                          # Pull changes from remote
nx sync push                          # Push changes to remote  
nx sync resolve                       # Resolve merge conflicts
```

**Enhanced Sync Manager**:
```cpp
// include/nx/sync/git_sync_manager.hpp
class GitSyncManager {
public:
  struct SyncStatus {
    bool is_repo = false;
    bool has_remote = false;
    std::string remote_url;
    size_t ahead = 0;
    size_t behind = 0;
    std::vector<std::string> conflicts;
    std::vector<std::string> staged_files;
    std::vector<std::string> unstaged_files;
  };
  
  Result<void> initRepository(const std::string& remote_url = "");
  Result<SyncStatus> getStatus();
  Result<void> pull();
  Result<void> push();
  Result<std::vector<std::string>> getConflicts();
  Result<void> resolveConflict(const std::string& file_path, const std::string& resolution);
  
private:
  std::filesystem::path notes_dir_;
};
```

---

### 1.8 System Maintenance Commands

#### 1.8.1 Reindex Command (`nx reindex`)

**Status**: 🔴 Not implemented  
**Infrastructure**: Index interface exists  
**Location**: `src/cli/commands/reindex_command.cpp` (to be created)

```cpp
class ReindexCommand : public Command {
  Result<int> execute(const GlobalOptions& options) override {
    auto& index = Application::instance().searchIndex();
    
    // Clear existing index
    auto clear_result = index.clear();
    if (!clear_result.has_value()) {
      return std::unexpected(clear_result.error());
    }
    
    // Rebuild from all notes
    auto& store = Application::instance().noteStore();
    auto notes_result = store.listAll();
    if (!notes_result.has_value()) {
      return std::unexpected(notes_result.error());
    }
    
    // Index all notes with progress reporting
    size_t total = notes_result.value().size();
    size_t processed = 0;
    
    for (const auto& note_id : notes_result.value()) {
      auto note_result = store.load(note_id);
      if (note_result.has_value()) {
        index.addNote(note_result.value());
        ++processed;
        
        if (!options.quiet && processed % 100 == 0) {
          std::cerr << "Indexed " << processed << "/" << total << " notes\n";
        }
      }
    }
    
    // Optimize index
    auto optimize_result = index.optimize();
    if (!optimize_result.has_value()) {
      return std::unexpected(optimize_result.error());
    }
    
    if (!options.quiet) {
      std::cout << "Reindexed " << processed << " notes successfully\n";
    }
    
    return 0;
  }
};
```

#### 1.8.2 Backup Command (`nx backup`)

**Implementation**:
```bash
nx backup create [--to path] [--compress] [--exclude-attachments]
nx backup restore <backup_path>
nx backup list
```

**Backup Strategy**:
- Create timestamped archives of notes directory
- Include/exclude options for attachments
- Metadata preservation
- Incremental backup support

#### 1.8.3 Garbage Collection (`nx gc`)

**Cleanup Operations**:
- Remove orphaned attachments
- Clean up temporary files
- Optimize database
- Remove empty directories
- Fix broken symlinks

#### 1.8.4 Health Check (`nx doctor`)

**Diagnostic Checks**:
- Verify note file integrity
- Check index consistency
- Validate attachment references
- Test configuration
- Performance benchmarks

#### 1.8.5 Configuration Management (`nx config`)

**Pattern** (follow git config):
```bash
nx config get core.notes_dir
nx config set core.editor nvim
nx config list [--json]
```

---

---

### 1.9 Search vs Grep Clarification (`nx search`)

**Status**: 🔴 Missing - unclear distinction in spec  
**Infrastructure**: grep exists, search command missing  
**Location**: `src/cli/commands/search_command.cpp` (to be created)

#### Implementation Details

**Command Distinction**:
```bash
# nx grep - Raw text search with ripgrep-like interface
nx grep <pattern> [--regex] [--case] [--content|--title|--tag]

# nx search - Structured search with ranking and intelligence
nx search <query> [--semantic] [--bool] [--rank] [--limit N]
```

**Search Command Features**:
```cpp
class SearchCommand : public Command {
public:
  Result<int> execute(const GlobalOptions& options) override;
  void setupCommand(CLI::App* cmd) override;

private:
  std::string query_;
  bool semantic_ = false;      // Use AI embeddings for semantic search
  bool boolean_ = false;       // Parse as boolean query (AND/OR/NOT)
  bool rank_results_ = true;   // Rank by relevance
  size_t limit_ = 50;
  std::string sort_by_ = "relevance"; // relevance, created, modified
  
  Result<std::vector<SearchResult>> performSearch();
  Result<std::vector<SearchResult>> performSemanticSearch();
  Result<std::vector<SearchResult>> performBooleanSearch();
};
```

**Search Result with Ranking**:
```cpp
struct SearchResult {
  nx::core::NoteId note_id;
  std::string title;
  std::string excerpt;          // Highlighted excerpt
  double relevance_score;       // 0.0 - 1.0
  std::vector<size_t> match_positions;
  std::string match_type;       // "title", "content", "tag", "semantic"
};
```

**Boolean Query Parser**:
```cpp
class BooleanQueryParser {
public:
  struct QueryNode {
    enum Type { TERM, AND, OR, NOT };
    Type type;
    std::string term;
    std::vector<std::unique_ptr<QueryNode>> children;
  };
  
  Result<std::unique_ptr<QueryNode>> parse(const std::string& query);
  Result<std::vector<SearchResult>> execute(const QueryNode& query);
};
```

---

## 📋 Phase 2: Power Features (2-3 weeks)

### 2.1 Enhanced Search & Boolean Queries

**Boolean Query Support**:
```sql
-- Extend existing SQLite FTS5 queries
SELECT * FROM notes_fts WHERE notes_fts MATCH 'python AND (machine OR learning) NOT deprecated';
```

**Field-Specific Search**:
```bash
nx search "title:meeting AND tag:work AND created:>2024-01-01"
```

### 2.2 Wiki-Links (`[[note title]]`)

**Implementation Strategy**:
1. Extend markdown parser to detect `[[...]]` syntax
2. Add link resolution during note save/load
3. Update backlinks automatically
4. Add link validation and repair tools

**Link Resolution**:
```cpp
class WikiLinkProcessor {
public:
  struct Link {
    std::string text;           // [[text]]
    std::string target_title;   // resolved title
    nx::core::NoteId target_id; // resolved note ID
    bool valid = false;
  };
  
  Result<std::vector<Link>> extractLinks(const std::string& content);
  Result<std::string> resolveLinks(const std::string& content);
  Result<void> updateBacklinks(const nx::core::Note& note);
};
```

### 2.3 Shell Integration

**Bash/Zsh Completions**:
- Generate completion scripts
- Command argument completion
- Note ID completion with fuzzy matching
- Tag and notebook completion

**Built-in Fuzzy Finder**:
```cpp
class FuzzyFinder {
public:
  struct Match {
    std::string text;
    double score;
    std::vector<size_t> match_positions;
  };
  
  std::vector<Match> search(const std::string& query, 
                           const std::vector<std::string>& candidates);
};
```

---

## 📋 Phase 3: Advanced Integration (2-3 weeks)

### 3.1 Complete Encryption Workflow

**Transparent Encryption**:
```cpp
class EncryptionManager {
public:
  Result<void> enableEncryption(const std::string& recipient);
  Result<void> disableEncryption();
  Result<std::string> decryptNote(const nx::core::NoteId& id);
  Result<void> encryptNote(const nx::core::NoteId& id);
  
private:
  bool encryption_enabled_ = false;
  std::string age_recipient_;
};
```

### 3.2 Advanced Git Sync

**Conflict Resolution**:
- Three-way merge for note conflicts
- Automatic conflict markers
- Interactive resolution prompts
- Branch management

### 3.3 Automation & Hooks

**Hook System**:
```cpp
class HookManager {
public:
  enum class HookType {
    PreNoteCreate,
    PostNoteCreate,
    PreNoteUpdate,
    PostNoteUpdate,
    PreNoteDelete,
    PostNoteDelete
  };
  
  Result<void> registerHook(HookType type, const std::string& script_path);
  Result<void> executeHooks(HookType type, const nx::core::Note& note);
};
```

---

## 📋 Phase 4: Polish & AI Enhancement (1-2 weeks)

### 4.1 Advanced AI Features

**Custom Prompts**:
```bash
nx ai-run --prompt meeting-summary --input note_id --vars "attendees=John,Jane"
```

**Semantic Search**:
- Integrate embedding generation
- Vector similarity search
- Hybrid ranking (FTS + semantic)

### 4.2 Documentation & Distribution

**Package Distribution**:
- `.deb` packages for Ubuntu/Debian
- `.rpm` packages for Fedora/RHEL
- Homebrew formula
- Docker image
- Static binaries

---

## 🧪 Testing Strategy

### Unit Tests
```cpp
// Example test structure
TEST(AttachCommandTest, ValidAttachment) {
  TemporaryDirectory temp_dir;
  TestApplication app(temp_dir.path());
  
  // Create test note
  auto note_id = createTestNote(app, "Test Note");
  
  // Create test file
  auto test_file = temp_dir.path() / "test.txt";
  writeFile(test_file, "test content");
  
  // Execute attach command
  AttachCommand cmd;
  cmd.setNoteId(note_id.toString());
  cmd.setFilePath(test_file.string());
  
  auto result = cmd.execute(GlobalOptions{});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 0);
  
  // Verify attachment was created
  auto attachments = app.attachmentStore().listForNote(note_id);
  ASSERT_TRUE(attachments.has_value());
  EXPECT_EQ(attachments.value().size(), 1);
}
```

### Integration Tests
```bash
#!/bin/bash
# Test full workflow
nx new "Test Note" --tags test
NOTE_ID=$(nx ls --json | jq -r '.[0].id')
echo "test content" > test.txt
nx attach "$NOTE_ID" test.txt
nx ls --json | jq '.[] | select(.id == "'$NOTE_ID'") | .attachments | length' | grep -q "1"
```

### Performance Tests
```cpp
BENCHMARK(BenchmarkAttachCommand) {
  // Benchmark attaching files of various sizes
  for (auto size : {1024, 1024*1024, 10*1024*1024}) {
    auto test_file = createTestFile(size);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    AttachCommand cmd;
    cmd.execute(/* ... */);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 100); // < 100ms target
  }
}
```

---

## 📊 Success Metrics

### Phase 1 Success Criteria
- [ ] All 15+ missing commands implemented and tested
- [ ] Notebook management fully functional (nx nb list/create/rename/delete/info)
- [ ] **Hierarchical navigation panel** replaces tags panel with expandable notebooks
- [ ] **Smart filtering logic** works for notebook+tag combinations
- [ ] **Keyboard navigation** handles flattened tree structure smoothly
- [ ] **Visual feedback** clearly shows active filters and selection state
- [ ] **Breadcrumb navigation** shows current context in status bar
- [ ] Encryption commands working (nx enc init/on/off/status)
- [ ] Sync subcommands complete (nx sync init/status/pull/push/resolve)
- [ ] Search vs grep distinction clear and implemented
- [ ] Attachment system fully functional with TUI integration
- [ ] Import can handle multiple formats (dir, file, obsidian, notion)
- [ ] Template system with variable substitution and TUI integration
- [ ] System maintenance tools prevent data corruption
- [ ] All new features integrate seamlessly with hierarchical TUI

### Phase 2 Success Criteria  
- [ ] Boolean search queries work correctly
- [ ] Wiki-links resolve and update backlinks automatically
- [ ] Shell completions work in bash/zsh
- [ ] Performance targets met with enhanced features

### Phase 3 Success Criteria
- [ ] Encryption workflow is transparent and secure
- [ ] Git sync handles merge conflicts gracefully
- [ ] Hook system enables automation
- [ ] Performance excellent with 10k+ notes

### Phase 4 Success Criteria
- [ ] AI features provide measurable value
- [ ] Distribution packages install cleanly
- [ ] Documentation is comprehensive
- [ ] Ready for public release

---

## 🚀 Implementation Timeline

### Week 1-2: Phase 1 Core Features
- **Days 1-2**: `nx attach` command with TUI integration
- **Days 3-5**: `nx nb` notebook management commands + **Hierarchical TUI Navigation Panel**
  - Implement NotebookManager class
  - Transform left panel into hierarchical navigation
  - Add smart filtering logic for notebook+tag combinations
  - Implement keyboard navigation for flattened tree
- **Days 6-7**: `nx import` system with multiple format support
- **Days 8-9**: `nx tpl` template management with TUI integration
- **Days 10-11**: `nx meta` metadata management
- **Days 12-13**: `nx enc` encryption commands
- **Days 14-15**: `nx sync` subcommands and `nx search` vs grep clarification
- **Days 16**: System maintenance commands (reindex, backup, gc, doctor, config)

### Week 3-4: Phase 2 Power Features
- **Days 17-18**: Enhanced search with boolean queries and ranking
- **Days 19-20**: Wiki-links implementation with backlink management
- **Days 21-22**: Shell integration and completions
- **Days 23-24**: Workflow automation and batch operations

### Week 5-6: Phase 3 Advanced Integration
- **Days 25-26**: Complete encryption workflow with transparent operations
- **Days 27-28**: Advanced Git sync with conflict resolution UI
- **Days 29-30**: Hook system and automation framework
- **Days 31-32**: Performance optimizations and batch operations

### Week 7-8: Phase 4 Polish
- **Days 33-34**: Advanced AI features with semantic search
- **Days 35-36**: Distribution packages and comprehensive documentation
- **Days 37-38**: Final testing and release preparation

---

## 🛠️ Development Guidelines

### Code Quality Standards
- Follow existing patterns in codebase
- Maintain C++23 standard compliance
- Use RAII and modern C++ practices
- Comprehensive error handling with `Result<T>`
- Unit tests for all new functionality

### Integration Requirements
- Commands must integrate with existing TUI
- All operations must support JSON output
- Maintain backwards compatibility
- Follow XDG directory specifications

### Security Considerations
- Validate all file operations
- Prevent path traversal attacks
- Secure temporary file handling
- Proper encryption key management

---

This implementation plan provides a comprehensive roadmap for completing nx MVP2 while building on the excellent foundation established in MVP1. Each phase is designed to deliver incremental value while maintaining code quality and user experience.