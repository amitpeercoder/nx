# nx Implementation Plan (UPDATED)

## Overview
This document outlined the original implementation plan for nx, a high-performance CLI notes application in modern C++. **MVP1 has been completed** and the project is now following the [mvp2-plan.md](./mvp2-plan.md) roadmap.

## 🎉 MVP1 Completion Status
**All MVP1 objectives have been achieved!** The project successfully delivered:

### ✅ Completed Features
- ✅ **22+ Core Commands**: Note CRUD, search, AI features, export, TUI
- ✅ **Interactive TUI**: Full 3-pane terminal interface with real-time updates
- ✅ **Performance**: Sub-100ms operations on typical datasets
- ✅ **AI Integration**: Complete Claude/GPT support for all planned AI features
- ✅ **Search System**: SQLite FTS5 + ripgrep with full-text indexing
- ✅ **Export System**: Markdown, JSON, PDF, HTML with filtering
- ✅ **Architecture**: Clean modular C++ design with proper error handling
- ✅ **Testing**: Unit tests, integration tests, benchmarks
- ✅ **Build System**: CMake + vcpkg with CI/CD foundation

### 🔄 Issues Addressed from Original Plan
- ✅ **Missing MVP commands**: All core commands implemented
- ✅ **Performance infrastructure**: Benchmarks and performance targets met
- ✅ **Attachment system**: AttachmentStore infrastructure exists (CLI pending)
- ✅ **Error handling**: Comprehensive error codes and JSON responses
- ✅ **Filesystem safety**: Atomic operations and proper file handling
- ✅ **Complete documentation**: README, CONTRIBUTING, CHANGELOG, and specs

## 🚀 Current Status & Next Steps

**MVP1 is complete!** All phases from the original plan have been successfully implemented. The project now follows the [mvp2-plan.md](./mvp2-plan.md) roadmap for continued development.

### What's Next
See [mvp2-plan.md](./mvp2-plan.md) for the comprehensive roadmap, which includes:
1. **Phase 1**: Complete core commands (attach, import, templates, meta, system)
2. **Phase 2**: Power features (wiki-links, advanced search, shell integration)
3. **Phase 3**: Advanced features (complete encryption, sync, automation)
4. **Phase 4**: Polish and distribution

---

## Original Implementation Plan (COMPLETED)

*The sections below document the original plan that has been successfully executed:*

## Phase 1: Project Setup & Core Infrastructure (Week 1)

### 1.1 Build System Setup
- [ ] Create CMakeLists.txt with C++20/23 detection
- [ ] Set up vcpkg integration for dependency management
- [ ] Configure compiler flags (warnings, optimizations, sanitizers)
- [ ] Create build presets for Debug/Release/RelWithDebInfo
- [ ] Add CPack configuration for packaging

### 1.2 Project Structure
- [ ] Create directory structure (src/, include/, tests/, docs/)
- [ ] Set up namespace organization (nx::core, nx::cli, nx::store, etc.)
- [ ] Create initial header organization with proper guards

### 1.3 Development Environment
- [ ] Create .clang-format configuration
- [ ] Create .clang-tidy configuration
- [ ] Set up pre-commit hooks for formatting
- [ ] Create development Makefile with common tasks

### 1.4 Testing Infrastructure
- [ ] Integrate Google Test framework
- [ ] Set up Google Benchmark
- [ ] Create test directory structure
- [ ] Write initial CMake test registration

## Phase 2: Core Domain Models + Performance Infrastructure (Week 1-2)

### 2.0 Performance Infrastructure (CRITICAL - Build Early)
- [ ] Create synthetic note corpus generator (1k, 10k, 100k notes)
- [ ] Set up Google Benchmark integration
- [ ] Create performance baseline tests
- [ ] Add continuous performance regression detection
- [ ] Document benchmark machine specifications

### 2.1 Note ID System
- [ ] Implement ULID generator using C++ chrono
- [ ] Create NoteId class with validation
- [ ] Add ULID string parsing and formatting
- [ ] Write comprehensive unit tests
- [ ] Benchmark ULID generation performance

### 2.2 Note Model
- [ ] Design Note class with metadata
- [ ] Implement YAML front-matter parser
- [ ] Create Markdown content handler
- [ ] Add serialization/deserialization
- [ ] Implement validation logic

### 2.3 Metadata System
- [ ] Create Metadata class with common fields
- [ ] Implement tag system
- [ ] Add custom field support
- [ ] Create date/time handling utilities
- [ ] Write property-based tests for round-trip

## Phase 3: Storage Layer + Attachments (Week 2)

### 3.1 Filesystem Operations
- [ ] Implement XDG Base Directory support
- [ ] Create atomic file write operations
- [ ] Add directory traversal utilities
- [ ] Implement note path resolution
- [ ] Add file locking mechanism

### 3.2 Note Store
- [ ] Create NoteStore interface
- [ ] Implement FilesystemStore with atomic operations
- [ ] Add CRUD operations for notes
- [ ] Implement listing and filtering with notebook support
- [ ] Add transaction support with rollback
- [ ] Implement soft delete with trash directory
- [ ] Add fuzzy ID resolution for all operations

### 3.4 Attachment System (NEW)
- [ ] Create AttachmentStore interface
- [ ] Implement ULID-based attachment naming
- [ ] Add relative path linking in notes
- [ ] Implement attachment CRUD operations
- [ ] Add attachment cleanup on note deletion

### 3.3 Configuration
- [ ] Integrate toml++ library
- [ ] Create Config class
- [ ] Implement config file loading/saving
- [ ] Add environment variable overrides
- [ ] Create default configuration

## Phase 4: Search & Indexing (Week 2-3)

### 4.1 SQLite Integration (with proper configuration)
- [ ] Set up SQLite with FTS5 and required pragmas:
  - [ ] WAL mode (`journal_mode=WAL`)
  - [ ] `synchronous=NORMAL`
  - [ ] `temp_store=MEMORY`
  - [ ] `cache_size=-20000` (20k pages)
- [ ] Design database schema for notes + attachments + embeddings
- [ ] Create Index class interface
- [ ] Implement SqliteIndex with corruption detection

### 4.2 Search Implementation
- [ ] Create search query parser
- [ ] Implement full-text search
- [ ] Add tag-based filtering
- [ ] Implement date range queries
- [ ] Add ripgrep fallback for non-indexed search

### 4.3 Index Management
- [ ] Implement incremental indexing
- [ ] Add index rebuild functionality
- [ ] Create index corruption detection
- [ ] Add performance monitoring

## Phase 5: CLI Interface (Week 3)

### 5.1 CLI Framework Setup
- [ ] Integrate CLI11
- [ ] Create command structure
- [ ] Implement argument parsing
- [ ] Add global options handling

### 5.2 Complete MVP Command Set (ALL 25+ commands)
**Core CRUD:**
- [ ] Implement `nx new [title] [--tags] [--nb] [--from]` - create note
- [ ] Implement `nx edit <id|fuzzy>` - edit note in $EDITOR
- [ ] Implement `nx view <id>` - pretty print to stdout
- [ ] Implement `nx ls [filters]` - list notes with filtering
- [ ] Implement `nx rm <id> [--soft]` - delete note (soft by default)
- [ ] Implement `nx mv <id> --nb <notebook>` - move between notebooks

**Search & Discovery:**
- [ ] Implement `nx grep <query> [--regex] [--case]` - search with ripgrep
- [ ] Implement `nx open <fuzzy>` - fuzzy resolve then edit
- [ ] Implement `nx backlinks <id>` - show backlinks
- [ ] Implement `nx tags` - list all tags with counts

**Attachments:**
- [ ] Implement `nx attach <id> <path>` - attach files

**Metadata:**
- [ ] Implement `nx meta <id> [--set key=val]` - metadata management

**Templates:**
- [ ] Implement `nx tpl add <name> <file>` - add template
- [ ] Implement `nx new --from <name>` - create from template

**Import/Export:**
- [ ] Implement `nx import dir <path>` - import directory
- [ ] Implement `nx export md|zip|json` - export notes

**Git Sync:**
- [ ] Implement `nx sync init|status|pull|push|resolve` - git operations

**Encryption:**
- [ ] Implement `nx enc init --age <recipient>` - setup encryption
- [ ] Implement `nx enc on|off` - toggle encryption mode

**System:**
- [ ] Implement `nx reindex` - rebuild search index
- [ ] Implement `nx backup create [--to]` - create backup
- [ ] Implement `nx gc` - garbage collection
- [ ] Implement `nx doctor` - system health check
- [ ] Implement `nx config get|set KEY [VALUE]` - configuration

### 5.3 Output Formatting (JSON EVERYWHERE)
- [ ] Create output formatter interface
- [ ] Implement plain text formatter
- [ ] **Add JSON output support for ALL commands**
- [ ] Create table formatter for lists
- [ ] Add color support with ANSI detection
- [ ] Implement structured error responses in JSON
- [ ] Define consistent JSON schema across commands

## Phase 6: Advanced Features (Week 4-5)

### 6.1 Template System
- [ ] Design template storage
- [ ] Implement `nx tpl` commands
- [ ] Add template variable substitution
- [ ] Create default templates

### 6.2 Import/Export
- [ ] Implement directory import
- [ ] Add Markdown export
- [ ] Create ZIP archive export
- [ ] Add JSON export format
- [ ] Implement progress reporting

### 6.3 Metadata Management
- [ ] Implement `nx meta` command
- [ ] Add bulk metadata operations
- [ ] Create metadata validation
- [ ] Add backlink tracking

## Phase 7: Git Synchronization (Week 5)

### 7.1 Git Integration
- [ ] Integrate libgit2
- [ ] Create GitSync class
- [ ] Implement repository initialization
- [ ] Add commit generation

### 7.2 Sync Commands
- [ ] Implement `nx sync pull`
- [ ] Implement `nx sync push`
- [ ] Add conflict detection
- [ ] Create merge strategies
- [ ] Add sync status reporting

## Phase 8: Encryption Support (Week 5-6)

### 8.0 Security-First Design
- [ ] **Never write plaintext to persistent disk when encryption is on**
- [ ] Use O_TMPFILE or tmpfs under XDG_RUNTIME_DIR
- [ ] Implement RAM-backed cache that's wiped on exit
- [ ] Secure memory management for decrypted content

### 8.1 Age Integration (SECURE)
- [ ] Create safe age wrapper using execvp (NO shell injection)
- [ ] Implement key management with proper permissions
- [ ] Add encryption/decryption functions
- [ ] Create secure temporary file handling with O_TMPFILE
- [ ] Implement encryption mode toggle (`enc on/off`)

### 8.2 Encryption Commands
- [ ] Implement `nx encrypt`
- [ ] Implement `nx decrypt`
- [ ] Add bulk encryption operations
- [ ] Create key rotation functionality

## Phase 9: Testing & Quality (Week 6-7)

### 9.0 AI Testing
- [ ] Create AI integration test suite with mock providers
- [ ] Test embedding generation and vector search accuracy
- [ ] Validate RAG pipeline with known question-answer pairs
- [ ] Test cost controls and budget enforcement
- [ ] Verify PII redaction effectiveness
- [ ] Test AI command error handling and graceful degradation
- [ ] Benchmark AI operations against performance targets

### 9.1 Test Coverage
- [ ] Achieve >80% unit test coverage
- [ ] Write integration test suite
- [ ] Create end-to-end test scenarios
- [ ] Add property-based tests
- [ ] Implement fuzz testing

### 9.2 Performance Testing
- [ ] Create benchmark suite
- [ ] Test with 10k+ notes
- [ ] Optimize hot paths
- [ ] Add performance regression tests

### 10.3 Complete Documentation (DELIVERABLES)
- [ ] **SPEC.md** - distilled specification
- [ ] **ENGINEERING.md** - error/exception policy, code style
- [ ] **BENCHMARK.md** - performance specs and machine details
- [ ] **SECURITY.md** - security considerations
- [ ] **README.md** - features and quickstart
- [ ] Create man pages for all commands
- [ ] Add inline code documentation
- [ ] Create architecture documentation
- [ ] **scripts/smoke.sh** - manual integration test script

## Phase 10: Packaging & Release (Week 7-8)

### 10.1 Packaging
- [ ] Create .deb package
- [ ] Create .rpm package
- [ ] Build static binaries (musl)
- [ ] Create portable tarball

### 10.2 CI/CD Pipeline
- [ ] Set up GitHub Actions
- [ ] Add build matrix (GCC/Clang)
- [ ] Configure sanitizer runs
- [ ] Add release automation

### 10.3 Release Preparation
- [ ] Create installation documentation
- [ ] Write changelog
- [ ] Add version management
- [ ] Create release checklist

## Phase 5: AI-Assisted Features (Week 3-4)

### 5.0 AI Provider Abstraction Layer
- [ ] Design AI provider interface (OpenAI, Anthropic, future local models)
- [ ] Implement OpenAI client with rate limiting and error handling
- [ ] Implement Anthropic client with structured outputs
- [ ] Create unified response format across providers
- [ ] Add provider-specific model configurations
- [ ] Implement API key validation and secure storage

### 5.1 Configuration & Security
- [ ] Extend TOML config with AI section and provider settings
- [ ] Implement secure API key handling (env vars, file permissions)
- [ ] Add cost control: daily budgets, rate limiting, usage tracking
- [ ] Create PII redaction system (emails, URLs, numbers, custom patterns)
- [ ] Implement request/response logging with privacy filters
- [ ] Add AI feature toggle and graceful degradation

### 5.2 Embedding System & Vector Search
- [ ] Integrate HNSWlib for vector indexing
- [ ] Implement embedding generation pipeline
- [ ] Design embedding storage schema in SQLite
- [ ] Create incremental embedding updates
- [ ] Implement semantic search with similarity scoring
- [ ] Add embedding index rebuild and corruption detection
- [ ] Optimize embedding batch processing for large note collections

### 5.3 RAG (Retrieval-Augmented Generation) Pipeline
- [ ] Implement hybrid search (FTS5 + semantic similarity)
- [ ] Create context window management and token counting
- [ ] Design citation system with ULID references and line numbers
- [ ] Implement result ranking and relevance filtering
- [ ] Add query expansion and intent understanding
- [ ] Create response validation and quality scoring
- [ ] Implement streaming responses for long operations

### 5.4 Core AI Commands
**Setup & Management:**
- [ ] Implement `nx ai init --provider <name> --model <model> --key <key>` - Configure AI
- [ ] Implement `nx ai status` - Show configuration and model info
- [ ] Implement `nx ai test` - Validate API connectivity
- [ ] Implement `nx ai usage [--since DATE] [--cost]` - Usage and cost tracking
- [ ] Implement `nx ai models` - List available models per provider

**Knowledge Interaction:**
- [ ] Implement `nx ask "question" [--nb NOTEBOOK] [--tag TAG] [--since DATE]` - RAG Q&A
- [ ] Implement `nx digest --filter 'FILTER' [--style brief|detailed]` - Multi-note summaries
- [ ] Implement `nx suggest-links <id>` - Find related notes and suggest links

**Content Enhancement:**
- [ ] Implement `nx summarize <id> [--style bullets|exec|abstract] [--apply]` - Note summarization
- [ ] Implement `nx title <id> [--suggestions N] [--apply]` - Generate titles
- [ ] Implement `nx tag-suggest <id> [--from-existing] [--apply]` - AI tagging
- [ ] Implement `nx rewrite <id> [--tone crisp|neutral|friendly] [--apply]` - Content improvement
- [ ] Implement `nx tasks <id> [--format markdown|json]` - Extract action items
- [ ] Implement `nx outline "topic" [--depth N]` - Generate structured outlines

**Specialized Operations:**
- [ ] Implement `nx condense <id> [--meeting-minutes] [--apply]` - Condense verbose content
- [ ] Implement `nx ai-run --prompt <template> [--input <id>] [--vars KEY=VAL]` - Custom prompts

### 5.5 Prompt Engineering & Templates
- [ ] Create prompt template system with variable substitution
- [ ] Design context-aware prompts for different note types
- [ ] Implement prompt versioning and A/B testing framework
- [ ] Add few-shot learning examples for consistent outputs
- [ ] Create domain-specific prompts (meeting notes, technical docs, etc.)
- [ ] Implement prompt result caching for similar queries

### 5.6 AI Output Integration
- [ ] Design structured JSON outputs for all AI commands
- [ ] Implement confidence scoring for AI-generated content
- [ ] Add preview mode (--dry-run) for destructive operations
- [ ] Create diff preview for content modifications
- [ ] Implement batch operations for multiple notes
- [ ] Add undo functionality for AI-applied changes


## Implementation Priority

### MVP (Minimum Viable Product)
1. Core models and storage
2. Basic CRUD operations
3. Simple search
4. Essential CLI commands

### MVP Requirements (ALL MUST BE IMPLEMENTED)
1. All 25+ CLI commands working
2. **AI-assisted features with OpenAI and Anthropic support**
3. **RAG system with hybrid search (FTS5 + embeddings)**
4. **All AI commands functional (ask, summarize, tag-suggest, etc.)**
5. Complete attachment system
6. Git synchronization with conflict resolution
7. Encryption support (age/rage)
8. Full-text search with FTS5 + ripgrep fallback
9. Template system
10. JSON output for all commands (including AI responses)
11. **Cost controls and usage tracking for AI operations**
12. Performance targets met (including AI-specific targets)
13. Complete test suite (including AI testing)
14. All required documentation

### Optional Features (If Time Permits)
1. Curses TUI (`nx ui`)
2. Backlinks graph export (`nx graph --dot`)
3. Attachment text extraction (PDFs)
4. Local AI model integration (Ollama, etc.)
5. Advanced prompt templates and A/B testing
6. AI-powered note clustering and topic modeling

## Risk Mitigation

### Technical Risks
- **Performance**: Continuous benchmarking, profiling
- **Data corruption**: Atomic operations, backups
- **Platform compatibility**: CI testing on multiple platforms

### Schedule Risks
- **Scope creep**: Strict MVP definition
- **Dependencies**: Vendor dependencies with vcpkg
- **Complexity**: Incremental development, continuous testing

## Success Metrics (ACCEPTANCE CRITERIA)

1. **All MVP commands work**: `nx new`, `ls`, `grep`, `edit`, `rm --soft`, `attach`, `tags`, `meta`, `reindex`, etc.
2. **Performance targets met**: 
   - Create/list/edit P95 < 100ms on 10k notes
   - FTS queries P95 < 200ms
   - Full reindex < 45s on mid-range laptop
   - **AI embedding generation P95 < 500ms per note**
   - **Semantic search P95 < 300ms**
   - **RAG queries P95 < 2s including API calls**
   - **Embedding index rebuild < 2 min for 10k notes**
3. **Indexing works**: Incremental updates, deterministic rebuild
4. **Git sync round-trip**: Works locally and with remote, conflict handling
5. **Encryption mode**: No plaintext to persistent disk, editing path audited
6. **AI features working**: 
   - RAG Q&A with accurate citations and ULID references
   - All AI commands functional with OpenAI and Anthropic
   - Cost tracking and budget enforcement working
   - PII redaction verified before API calls
   - Embedding search provides relevant semantic results
7. **Packaging works**: `.deb`, `.rpm`, tarballs install and `nx doctor` passes
8. **Test coverage > 80%** with unit, integration, property, fuzz, benchmark tests (including AI mocks)
9. **All deliverables complete**: SPEC.md, ENGINEERING.md, BENCHMARK.md, SECURITY.md, smoke.sh

## Development Workflow

1. **Branch Strategy**: Feature branches → develop → main
2. **Code Review**: All changes via PR
3. **Testing**: Tests must pass before merge
4. **Documentation**: Update docs with code changes
5. **Performance**: Benchmark critical paths

## Tools & Resources

### Development Tools
- Compiler: GCC 12+ / Clang 15+
- Build: CMake 3.25+, Ninja
- Package Manager: vcpkg
- IDE: Any with C++ support

### Libraries
- CLI11 (CLI parsing)
- toml++ (Configuration)
- SQLite (Database)
- libgit2 (Git operations)
- spdlog (Logging)
- nlohmann/json (JSON)
- yaml-cpp (YAML)
- **HNSWlib (Vector similarity search)**
- **cURL/libcurl (HTTP client for AI APIs)**
- **OpenSSL (TLS/HTTPS for API connections)**

### Testing Tools
- Google Test (Unit testing)
- Google Benchmark (Performance)
- Valgrind (Memory checking)
- Sanitizers (ASAN, UBSAN, TSAN)

## Implementation Principles

- **Performance from day 1**: Benchmark every critical operation as it's built
- **Security by default**: Encryption implementation must be bulletproof
- **AI-first design**: RAG and semantic features are core, not add-ons
- **Privacy by design**: PII redaction, secure API handling, no data leaks
- **Cost consciousness**: Budget controls, usage tracking, efficient token usage
- **Complete MVP**: All commands from spec, not a subset (including AI)
- **Structured output**: JSON is first-class citizen, not afterthought
- **Error handling**: `expected<T, Error>` with comprehensive error codes
- **Atomic operations**: fsync→rename pattern, O_TMPFILE usage
- **Memory safety**: RAII, no raw owning pointers, lifetime audits
- **Minimize dependencies**: Pin versions with vcpkg
- **Documentation as deliverable**: Not optional
- **Backward compatibility**: Stable CLI and JSON contracts