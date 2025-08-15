# Prompt: Build a Linux CLI Markdown notes app in modern C++

You are a senior C++ engineer. Implement a **local-first, plaintext Markdown** note-taking tool named **`nx`** for Linux terminals. Prioritize speed, correctness, and low maintenance. Deliver a production-grade MVP with tests and CI.

## Product goals
- **Instant UX:** common ops under **100 ms** on **10k notes**; FTS queries P95 **< 200 ms**.
- **Local-first:** works fully offline; Git sync optional.
- **Plaintext + YAML:** notes are Markdown files with YAML front-matter.
- **Composable:** excellent shell ergonomics; plays nice with `$EDITOR`, `fzf`, `ripgrep`.
- **Secure option:** per-file encryption (age) without leaking plaintext to disk.
- **Boring to maintain:** minimal deps, clear modules, deterministic behavior.

## Non-goals (MVP)
- No mobile, web UI, or multiuser real-time collab.
- No server backend. Sync is Git only.
- No AI features.

## Operating environment
- Linux (x86_64/ARM64). Build with **GCC** and **Clang**.
- Static and dynamic builds (glibc + musl tarballs).
- Respect XDG: data in `~/.local/share/nx`, config in `~/.config/nx`, cache/temp in `~/.cache/nx` or `$XDG_RUNTIME_DIR`.

## Data model
- **Filename:** `ULID-slug.md` (ULID = monotonic, sortable, URL-safe).
- **Front-matter (YAML):**
  ```yaml
  ---
  id: 01J8Y4N9W8K6W3K4T4S0S3QF4N
  title: "Design meeting notes"
  created: 2025-08-14T09:32:15Z
  updated: 2025-08-14T10:01:22Z
  tags: [platform, planning]
  notebook: "work"
  links: ["01J8ABC...", "01J8DEF..."]
  ---
  ```
- **Attachments:** stored under `attachments/` with `ULID-*` filenames; notes link with relative paths.
- **Index:** disposable SQLite database `index.sqlite` using **FTS5**.

## Directory layout
```
~/.local/share/nx/
  notes/
    01J8…-title.md
  attachments/
    01J8…-diagram.png
  .nx/
    index.sqlite
    trash/                 # for soft deletes
~/.config/nx/
  config.toml
```

## CLI surface (MVP)
- `nx new [title] [--tags a,b] [--nb <notebook>] [--from <template>]`
- `nx edit <id|fuzzy>` → opens in `$EDITOR`
- `nx view <id>` → pretty print to stdout (no ANSI by default; `--ansi` optional)
- `nx rm <id> [--soft]`
- `nx mv <id> --nb <notebook>`
- `nx attach <id> <path>`
- `nx ls [--nb …] [--tag …] [--since …] [--until …] [--sort updated|created] [--limit N] [--json]`
- `nx grep <query> [--regex] [--case] [--content|--title|--tag] [--json]`
- `nx open <fuzzy>` (resolve then open in editor)
- `nx backlinks <id>`
- `nx tags` (list tags with counts)
- `nx meta <id> [--set key=val …]`
- `nx tpl add <name> <file>` / `nx new --from <name>`
- `nx import dir <path>` / `nx export md|zip|json [--filter …]`
- `nx sync init|status|pull|push|resolve`
- `nx enc init --age <recipient>` / `nx enc on|off`
- `nx reindex` / `nx backup create [--to <path>]` / `nx gc` / `nx doctor`
- `nx config get|set KEY [VALUE]`

All commands support `-q/--quiet`, `-v/--verbose`, `--json` for scriptable output, and non-zero exit codes on failure.

## Technical requirements

### Language & standards
- **C++23** (or C++20 if necessary) with: `std::filesystem`, ranges, `string_view`, `span`.
- RAII only. **No raw owning pointers.** Prefer `unique_ptr`. Avoid `shared_ptr` unless ownership is truly shared.
- Error handling: choose one policy and enforce:
  - **Preferred:** error-code style with `expected<T, Error>` (use `tl::expected` if std unavailable). No exceptions in hot paths.
- UTF-8 everywhere.

### Dependencies (prefer via vcpkg; pin versions)
- **CLI:** CLI11
- **Config:** toml++
- **YAML:** yaml-cpp (strict schema, canonicalize on write)
- **SQLite:** system `sqlite3` with **FTS5**; wrapper either `sqlite_modern_cpp` or RAII over C API
- **Git:** libgit2
- **Logging:** spdlog (structured; emits to stderr)
- **JSON output:** nlohmann/json
- **ULID:** lightweight header-only or local impl
- **Encryption:** Phase 1 shell-out to `age`/`rage` binary; abstract behind `Encryptor` interface

### Build, packaging, and tooling
- **Build:** CMake + Ninja; reproducible builds.
- **Package:** CPack `.deb` + `.rpm` + tarballs (glibc + musl).
- **Tooling:** clang-format, clang-tidy, cppcheck; Address/UB/Thread sanitizers in CI.
- **CI:** GitHub Actions (clang + gcc matrix; ASan/UBSan on PRs; release artifacts on tags).

### Modules & boundaries
```
src/
  app/            # main(), CLI wiring
  cli/            # verbs -> service calls (CLI11)
  core/           # Note, NoteId(ULID), Metadata, parsing/validation
  store/          # FilesystemStore, AttachmentStore (XDG paths)
  index/          # SqliteIndex (FTS5), RipgrepIndex (fallback via shell-out)
  sync/           # GitSync (libgit2) with simple conflict resolution
  crypto/         # Encryptor iface; AgeEncryptor (shell)
  import_export/  # DirImporter, Tar/MD/JSON Exporter
  util/           # Result/Error, Time (RFC3339), Slugify, XDG, IO helpers
tests/
  unit/, integ/, fuzz/, bench/
```

### Indexing & search
- SQLite FTS5 virtual table across `title`, `content`, `tags`, `links`.
- WAL mode, pragmas:
  - `journal_mode=WAL`
  - `synchronous=NORMAL`
  - `temp_store=MEMORY`
  - `cache_size=-20000` (20k pages in memory)
- Incremental updates on note create/edit/delete.
- Deterministic `nx reindex` from the filesystem if index is missing or corrupt.
- `nx grep` falls back to `ripgrep` when FTS is unavailable, with JSON normalization of results.

### Filesystem and editing
- All writes are **atomic**:
  - Write to temp, `fsync`, then `rename`.
  - Preserve permissions and timestamps.
- Respect `$EDITOR`; use `$VISUAL` then `$EDITOR`, fallback to `vi`.
- When **encryption is on**:
  - Never write plaintext to persistent disk. Use `O_TMPFILE` or tmpfs under `$XDG_RUNTIME_DIR`.
  - Decrypted cache is RAM-backed and wiped on exit.

### Git sync
- `sync init`: `git init`, `.gitignore` for index, cache, tmp, and trash. Optional remote.
- `sync push/pull/status`: rebase-style flow; simple 3-way merges for text.
- On conflict: keep both; mark `(...conflict-<short-sha>).md` and surface via `nx sync resolve`.

### Encryption
- `enc init --age <recipient>` stores recipient in config.
- `enc on/off` toggles a mode where read/write operations transparently encrypt/decrypt files.
- Use `age`/`rage` binary via safe shell-out (no shell injection; execvp with argv).
- Key management is user-managed; provide `nx doctor` checks and helpful errors.

### Config
- `~/.config/nx/config.toml` keys:
  - `root`, `editor`, `indexer = "fts"|"ripgrep"`, `encryption = "age"|"none"`, `sync = "git"|"none"`, `defaults.notebook`, `defaults.tags = []`.
- `nx config get|set` supports dot paths; validates types.

### Performance targets (enforced with benches)
- Create/list/edit on 10k notes P95 < 100 ms.
- FTS queries P95 < 200 ms.
- Full reindex 10k notes < 45 s on mid-range laptop (document machine spec in BENCHMARK.md).

### Logging & telemetry
- Logging via spdlog with levels; default `info`.
- No telemetry in MVP. (Hook is allowed but **disabled** by default.)

### Error handling & UX
- Clear, **actionable** error messages.
- Non-zero exit codes per POSIX conventions.
- `--json` returns structured errors: `{ "error": { "code": "X", "message": "…" } }`.
- Don’t print ANSI unless `--ansi` is set or a TTY is detected.

### Security
- No network I/O unless `sync` is invoked.
- Redact paths and secrets in logs unless `NX_UNSAFE_LOGS=1`.
- Validate YAML schema strictly; canonicalize on write to eliminate diffs noise.

## Test strategy (must implement)
- **Unit tests:** core parsing/serialization, ULID, slugify, YAML schema, config parsing.
- **Integration:** end-to-end temp repo: create → edit → reindex → grep → sync → enc on/off.
- **Property tests:** YAML round-trip; ULID ordering; path normalization.
- **Fuzz tests:** front-matter parser, Markdown splitter, path sanitization.
- **Benchmarks:** Google Benchmark for index ops: insert, update, query, reindex.
- **Golden tests:** stable CLI outputs (text and JSON).
- **CI gates:** style, static analysis, ASan/UBSan clean, test/bench pass.

## Acceptance criteria (MVP)
1. `nx new`, `ls`, `grep`, `edit`, `rm --soft`, `attach`, `tags`, `meta`, `reindex` work as specified with JSON output options.
2. Indexing is incremental; `reindex` rebuilds deterministically from files.
3. Git sync round-trip works locally and with a remote; conflict handling produces both versions and a clear status.
4. Encryption mode prevents plaintext from touching persistent disk; editing path audited.
5. Performance targets met on 10k-note synthetic corpus (provide generator + scripts).
6. Packaging produces installable `.deb`, `.rpm`, and tarballs; `nx doctor` passes on a clean Linux machine.

## Deliverables
- Source tree per module layout.
- `README.md` (features + quickstart), `SPEC.md` (this prompt distilled), `ENGINEERING.md` (error/exception policy, code style), `BENCHMARK.md`, `SECURITY.md`.
- CI config and release artifacts.
- Example corpus + benchmark scripts.
- Manual test script (`scripts/smoke.sh`).

## Stretch (time permitting, but **not** required)
- Minimal curses TUI: `nx ui` with list + preview + live filter.
- Backlinks graph export (`nx graph --dot`).
- Attachment text extraction (optional) via `pdftotext`, gated behind config.

## Implementation constraints
- Keep external deps minimal and pinned.
- No global singletons except for a narrow logger facade.
- All filesystem operations must be race-aware and atomic where possible.
- Avoid UB landmines (no `string_view` to temporaries; lifetime audits).
- Public CLI and JSON output are **stable contracts**; add features without breaking them.

---

**Now, generate the full C++ project skeleton (CMake), implement the MVP commands end-to-end, include tests and CI, and adhere strictly to the acceptance criteria and performance targets above.**


## AI Integration (Optional, Opt-In)

If the user sets an OpenAI or Anthropic API key in `~/.config/nx/config.toml`, `nx` unlocks AI-assisted features. These work **only** when explicitly invoked and never modify data without `--apply`.

### Config keys (example)
```toml
[ai]
provider = "openai"         # or "anthropic"
model = "gpt-4o-mini"       # or "claude-3.5-sonnet"
api_key = "env:OPENAI_API_KEY"  # Prefer env ref over plaintext
max_tokens = 1200
temperature = 0.2
rate_limit_qpm = 20
daily_usd_budget = 1.50
enable_embeddings = true
embedding_model = "text-embedding-3-small"  # or "claude-embedding-1"
top_k = 6

[ai.redaction]
strip_emails = true
strip_urls = false
mask_numbers = true
```

### AI capabilities
1. **Ask over your notes (RAG Q&A)** – `nx ask "question" [--nb …] [--tag …]`
2. **Summarize** – `nx summarize <id> [--style bullets|exec] [--apply]`
3. **Auto-title / Tag Suggestion** – `nx title <id>`, `nx tag-suggest <id>`
4. **Rewrite / Clarify** – `nx rewrite <id> [--tone crisp|neutral]`
5. **Action Item Extraction** – `nx tasks <id>`
6. **Link Suggestions** – `nx suggest-links <id>`
7. **Outline Generator** – `nx outline "topic"`
8. **Meeting Minutes Condenser** – `nx condense <id>`
9. **Multi-note Digest** – `nx digest --filter '…'`
10. **Prompt Templates** – `nx ai-run --prompt <name> [--input <id>]`

### Retrieval & embeddings
- Default: SQLite FTS5 + heuristics.
- If `enable_embeddings = true`, build local HNSWlib index for semantic search.
- Embeddings stored alongside notes in `index.sqlite` and `.nx/vecindex.hnsw`.

### Cost control & privacy
- `--dry-run` estimates tokens & cost before sending.
- Daily budget enforcement via `daily_usd_budget`.
- Basic regex redaction for PII before sending.
- No telemetry; API key never logged.

### Example AI commands
```bash
nx ai init --provider openai --model gpt-4o-mini --key env:OPENAI_API_KEY
nx ai status
nx ai test
nx ask "What did we decide about IAM tenant cache?" --nb work --since 2025-07-01
nx summarize <id> --style exec --apply
nx tag-suggest <id> --apply
nx title <id> --apply
nx rewrite <id> --tone crisp --dry-run
nx suggest-links <id> --apply
nx digest --filter 'tag:iam since:2025-08-01'
nx ai usage --since 2025-08-01
```

### Acceptance criteria for AI
1. Works with OpenAI **or** Anthropic keys.
2. FTS-only retrieval works without embeddings; embeddings improve quality when enabled.
3. Commands support `--json` structured output with `answer_md`, `citations`, and `usage`.
4. Cost estimation and daily budget limit enforced.
5. API keys never leak to logs; config permissions checked.
6. Citations include ULIDs and line ranges for answers.

---
