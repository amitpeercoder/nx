# nx MVP2 Development Plan

## What's Missing to Make nx a Powerful Note-Taking App

After analyzing the codebase against the original specification and modern note-taking needs, here are the key missing features and improvements needed:

### 🔴 **Critical Missing Core Features (from spec)**

1. **Attachment Management**
   - `nx attach <id> <path>` command not implemented
   - AttachmentStore interface exists but no CLI command
   - No way to attach files/images to notes

2. **Import Functionality** 
   - `nx import dir <path>` command missing
   - Only export is implemented, no import
   - Can't migrate from other note systems

3. **Template System**
   - `nx tpl add/list/remove` commands missing  
   - Basic template support in `new --from` but no management
   - No user-defined templates

4. **Metadata Management**
   - `nx meta <id> --set key=val` command missing
   - Can't update arbitrary metadata fields

5. **System Maintenance Commands**
   - `nx reindex` - not implemented
   - `nx backup create/restore` - missing
   - `nx gc` (garbage collection) - missing
   - `nx doctor` (health check) - missing
   - `nx config get/set` - missing

6. **Notebook Management**
   - `nx nb list` - list all notebooks (not implemented)
   - `nx nb create <name>` - create new notebook (not implemented)
   - `nx nb rename <old> <new>` - rename notebook (not implemented)
   - `nx nb delete <name>` - delete notebook with safety checks (not implemented)
   - `nx nb info <name>` - show notebook statistics (not implemented)
   - Notebooks mentioned in spec but completely missing CLI commands
   - **TUI Enhancement**: Transform left panel into hierarchical navigation with expandable notebooks showing their tags

### 🟡 **Partially Implemented Features**

1. **Encryption**
   - `nx enc init --age <recipient>` - initialize encryption (not implemented)
   - `nx enc on|off` - toggle encryption mode (not implemented)
   - `nx enc status` - show encryption status (not implemented)
   - Basic implementation exists but CLI commands missing
   - Missing per-file encryption workflow
   - No transparent encryption/decryption

2. **Sync System**
   - `nx sync init` - initialize git repository (partially implemented)
   - `nx sync status` - show sync status (not implemented)
   - `nx sync pull` - pull changes (partially implemented)
   - `nx sync push` - push changes (partially implemented)
   - `nx sync resolve` - resolve conflicts (not implemented)
   - Basic sync command exists but subcommands incomplete
   - Missing conflict resolution
   - No automatic sync options

3. **Search/Grep**
   - `nx search <query>` vs `nx grep <query>` - unclear distinction in spec
   - Basic grep exists but missing advanced options
   - No regex support flags
   - Missing content/title/tag specific searches
   - Missing semantic search integration when AI enabled
   - Boolean queries not supported

### 🟢 **Power User Features to Add**

1. **Advanced Search & Filtering**
   - Full-text search with ranking
   - Boolean queries (AND/OR/NOT)
   - Date range queries
   - Saved searches/smart folders

2. **Linking & Graph Features**
   - Wiki-style [[links]] support
   - Graph visualization export
   - Orphaned notes detection
   - Link auto-completion

3. **Workflow Automation**
   - Hooks system (pre/post note creation)
   - Custom scripts integration
   - Watch folders for auto-import
   - Scheduled tasks (daily notes, cleanup)

4. **Better Shell Integration**
   - Bash/Zsh completions
   - Fuzzy finder integration (built-in)
   - Pipe-friendly operations
   - Bulk operations support

5. **Enhanced AI Features**
   - `nx condense` (meeting minutes)
   - `nx digest` (multi-note summary)
   - `nx ai-run --prompt` (custom prompts)
   - Semantic search using embeddings

6. **Performance & Scalability**
   - Incremental indexing
   - Parallel operations for batch commands
   - Caching layer for frequent operations
   - Lazy loading for large notebooks
   - Progress indicators for all long-running operations
   - Memory optimization for large note collections
   - Background indexing and maintenance

## 🚀 **Implementation Plan**

### **Phase 1: Complete Core MVP (1-2 weeks)**
**Priority: HIGH - Missing spec-required features**

1. **Attachment Management**
   - Implement `nx attach <id> <path>` command
   - CLI integration with existing AttachmentStore
   - Support for images, PDFs, documents
   - Automatic MIME type detection
   - Link generation for notes

2. **Import System**
   - Implement `nx import dir <path>` command  
   - Implement `nx import file <path>` command
   - Implement `nx import obsidian <vault_path>` command
   - Implement `nx import notion <export_path>` command
   - Support for:
     - Plain markdown files
     - Obsidian vaults with [[links]]
     - Notion exports (JSON/HTML)
     - Text files
     - Directory structures → notebooks
   - Metadata preservation and creation
   - Progress reporting for large imports

3. **Template Management**
   - Implement `nx tpl add <name> <file>` command
   - `nx tpl list` - show available templates
   - `nx tpl remove <name>` - delete template
   - `nx tpl show <name>` - preview template
   - Enhanced `nx new --from <template>` integration

4. **Metadata Commands**
   - Implement `nx meta <id>` - show all metadata
   - `nx meta <id> --set key=val` - update metadata
   - `nx meta <id> --get key` - get specific field
   - `nx meta <id> --delete key` - remove field

5. **System Maintenance**
   - `nx reindex` - rebuild search index
   - `nx backup create [--to path]` - create backup
   - `nx backup restore <path>` - restore from backup
   - `nx backup list` - list available backups
   - `nx gc` - cleanup orphaned files, optimize
   - `nx doctor` - health check and diagnostics
   - `nx config get <key>` - get config value
   - `nx config set <key> <value>` - set config value
   - `nx config list` - list all configuration

6. **Notebook Management**
   - `nx nb list` - list all notebooks with note counts
   - `nx nb create <name>` - create new notebook
   - `nx nb rename <old> <new>` - rename notebook
   - `nx nb delete <name>` - delete notebook (with safety checks)
   - `nx nb info <name>` - show notebook statistics
   
   **TUI Integration**:
   - **Primary Enhancement**: Replace tags panel with hierarchical navigation panel:
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
   - Smart filtering: notebook + tag combinations with AND/OR logic
   - Keyboard navigation: Enter (expand/filter), Space (multi-select), Tab (switch panels)
   - Show current notebook in status bar breadcrumb
   - Quick notebook creation from TUI (Ctrl+Shift+N)
   - Notebook switcher with fuzzy finder (Ctrl+N)

### **Phase 2: Power Features (2-3 weeks)**
**Priority: MEDIUM - Enhance usability and power**

1. **Enhanced Search**
   - Boolean query support (AND/OR/NOT)
   - Field-specific search (title:, content:, tag:)
   - Date range filtering
   - Regex support with proper flags
   - Search result ranking

2. **Wiki-Links & Graph**
   - `[[note title]]` link syntax support
   - Automatic link resolution
   - `nx graph --export dot` for visualization
   - Orphaned notes detection
   - Link validation and repair

3. **Shell Integration**
   - Bash/Zsh completion scripts
   - Built-in fuzzy finder for note selection
   - Pipe-friendly JSON output
   - Bulk operations support

4. **Workflow Features**
   - Complete export system: `nx export zip` (missing from current export)
   - Daily note templates with date variables
   - Auto-tagging based on content/location
   - Batch operations for multiple notes
   - Watch mode for external file changes
   - Note aliases and shortcuts
   - Note pinning/favorites system
   - Note archiving (beyond soft delete)

### **Phase 3: Advanced Integration (2-3 weeks)**
**Priority: MEDIUM - Professional features**

1. **Complete Encryption**
   - Implement `nx enc init --age <recipient>` command
   - Implement `nx enc on|off` toggle commands
   - Implement `nx enc status` command
   - Transparent encrypt/decrypt workflow
   - Key management commands
   - Per-notebook encryption policies
   - Secure temporary file handling

2. **Advanced Git Sync**
   - Complete `nx sync init|status|pull|push|resolve` subcommands
   - Conflict resolution strategies
   - Automatic sync on changes
   - Branch management for collaboration
   - Sync status and diagnostics
   - Merge conflict UI in TUI

3. **Automation & Hooks**
   - Pre/post command hooks
   - Watch folder auto-import
   - Scheduled maintenance tasks
   - Custom script integration

4. **Performance Optimizations**
   - Incremental indexing
   - Parallel operations where safe
   - Memory usage optimization
   - Large notebook handling

### **Phase 4: Polish & AI Enhancement (1-2 weeks)**
**Priority: LOW - Nice to have**

1. **Advanced AI Features**
   - `nx search <query> --semantic` - semantic search using embeddings
   - `nx condense <id>` - meeting minutes summarization
   - `nx digest --filter` - multi-note summaries
   - `nx ai-run --prompt <name>` - custom AI prompts
   - AI-powered link suggestions
   - Custom prompt templates and management

2. **Documentation & Distribution**
   - Comprehensive man pages
   - User guide and examples
   - Package for major distributions
   - Docker image for easy deployment

## **Success Criteria**

### **Phase 1 Complete When:**
- [ ] All original spec commands are implemented (attach, import, tpl, meta, reindex, backup, gc, doctor, config)
- [ ] Notebook management commands work (nb list/create/rename/delete/info)
- [ ] **Hierarchical notebook TUI** implemented with expandable notebooks showing contained tags
- [ ] **Multi-level filtering** works (notebook selection + tag within notebook + global tag filters)
- [ ] **Navigation panel** replaces simple tags panel with smart keyboard navigation
- [ ] Encryption commands functional (enc init/on/off/status)
- [ ] Sync subcommands complete (sync init/status/pull/push/resolve)
- [ ] Can attach files to notes and reference them in TUI
- [ ] Can import existing note collections from multiple formats
- [ ] Template system is fully functional with variable substitution
- [ ] System maintenance tools work reliably and safely

### **Phase 2 Complete When:**
- [ ] Search supports boolean queries and field-specific searches
- [ ] Clear distinction between `nx search` and `nx grep` commands
- [ ] Wiki-links work seamlessly with automatic backlink updates
- [ ] Shell integration is polished (completions, fuzzy finder)
- [ ] Export system is complete including ZIP format
- [ ] Common workflows are streamlined with batch operations
- [ ] **Hierarchical navigation panel** replaces simple tags panel with expandable notebooks
- [ ] **Smart filtering** works with notebook+tag combinations (e.g., "Work notebook + #urgent tag")
- [ ] **TUI keyboard navigation** handles flattened tree structure smoothly
- [ ] TUI integration for all new features

### **Phase 3 Complete When:**
- [ ] Encryption workflow is production-ready
- [ ] Git sync handles conflicts gracefully
- [ ] Automation reduces manual maintenance
- [ ] Performance is excellent with 10k+ notes

### **Phase 4 Complete When:**
- [ ] AI features provide real value
- [ ] Documentation is comprehensive
- [ ] Easy to install and use
- [ ] Ready for public release

## **Development Notes**

### **Architecture Considerations**
- Maintain backward compatibility for existing notes
- Keep CLI interface stable and intuitive
- Ensure all operations are atomic and safe
- Preserve the local-first philosophy

### **Testing Strategy**
- Unit tests for all new commands
- Integration tests for workflows
- Performance tests with large datasets
- Manual testing of TUI integration

### **Risk Mitigation**
- Implement backup/restore early
- Extensive testing with real note collections
- Gradual rollout of breaking changes
- Clear migration paths for users

---

This plan transforms nx from a basic note app into a powerful, professional-grade knowledge management system comparable to Obsidian/Logseq but with superior CLI ergonomics and local-first principles.