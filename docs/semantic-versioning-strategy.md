# Semantic Versioning Strategy for nx

## Overview

This document outlines the semantic versioning strategy for the nx CLI notes application, following [SemVer 2.0.0](https://semver.org/) specification.

## Version Format

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
```

### Examples
- `1.0.0` - First stable release
- `1.1.0` - Minor feature addition
- `1.0.1` - Bug fix release
- `2.0.0` - Breaking change
- `1.1.0-beta.1` - Pre-release
- `1.0.0+build.123` - Build metadata

## Versioning Rules

### MAJOR version (X.y.z)
Increment when making **incompatible API changes**:
- CLI command interface changes that break scripts
- Configuration file format changes requiring migration
- Database/index format changes requiring migration
- Removal of commands or major features

### MINOR version (x.Y.z)
Increment when adding **backward-compatible functionality**:
- New CLI commands
- New features that don't affect existing workflows
- New configuration options with sensible defaults
- Performance improvements without interface changes

### PATCH version (x.y.Z)
Increment for **backward-compatible bug fixes**:
- Security fixes
- Bug fixes that don't change behavior
- Documentation corrections
- Build system improvements

## Pre-release Identifiers

### Development Builds
- `alpha` - Early development, unstable
- `beta` - Feature complete, testing phase
- `rc` - Release candidate, final testing

### Examples
- `1.1.0-alpha.1` - First alpha of 1.1.0
- `1.1.0-beta.2` - Second beta of 1.1.0
- `1.1.0-rc.1` - First release candidate

## Build Metadata

Build metadata is added automatically by the Git-based version system:
- `+dev.N.HASH` - Development build
- `+ci.BUILD_ID` - CI build
- `+dirty` - Working directory has uncommitted changes

## Current Version Status

### Completed Milestones
- ✅ **MVP1**: Core note management functionality
- ✅ **MVP2 Phase 1**: Advanced features (templates, attachments, maintenance)
- ✅ **MVP3**: Enhanced TUI editor with security improvements
- ✅ **Security & Architecture Review**: Critical fixes and DI refactoring

### Version 1.0.0 Readiness
The codebase is ready for `v1.0.0` release based on:
- ✅ Feature completeness: All planned MVP features implemented
- ✅ Security: Critical vulnerabilities fixed
- ✅ Architecture: Clean DI pattern implemented
- ✅ Testing: 96% test pass rate
- ✅ Documentation: Comprehensive user and developer docs
- ✅ Cross-platform: macOS/Linux support with proper path handling

## Release Planning

### v1.0.0 (Next Release)
**Target**: Immediate
**Content**: 
- Core stable API
- MVP1-3 features
- Security hardening
- DI architecture
- Cross-platform support

### v1.1.0 (Future)
**Target**: 3-6 months
**Content**:
- Wiki-links (`[[note-title]]`)
- Advanced search (boolean queries)
- Shell completions
- Additional export formats

### v1.2.0 (Future)
**Target**: 6-12 months
**Content**:
- Plugin system
- Advanced encryption features
- Performance optimizations
- Extended AI integrations

### v2.0.0 (Future)
**Target**: 12+ months
**Content**:
- Breaking API changes if needed
- Major architectural improvements
- New paradigms or interfaces

## Git Tag Strategy

### Release Tags
- `v1.0.0` - Stable releases
- `v1.1.0-beta.1` - Pre-releases
- Use annotated tags with release notes

### Branch Strategy
- `main` - Stable, always releasable
- `develop` - Integration branch for features
- `feature/*` - Feature development
- `hotfix/*` - Critical fixes for releases

### Tag Creation Process
```bash
# Create annotated tag with release notes
git tag -a v1.0.0 -m "
nx v1.0.0 - First Stable Release

## Features
- Complete note management CLI
- Advanced TUI editor
- Template system
- Attachment management
- Search and indexing
- Git synchronization
- Security hardening

## Technical Improvements
- Dependency injection architecture
- Cross-platform compatibility
- Comprehensive test suite
- Professional error handling
"

# Push tag to origin
git push origin v1.0.0
```

## Automated Version Management

The existing Git-based version system automatically:
1. **Detects version type**: `release`, `development`, `ci`, `debug`
2. **Calculates semantic version**: Based on latest tag + commits
3. **Generates build metadata**: Commit hash, dirty state, build info
4. **Provides version API**: Available throughout application

### Version Detection Logic
```cpp
// Automatic version determination
if (GitVersionInfo::isRelease()) {
    // Clean tagged release: "1.0.0"
} else if (GitVersionInfo::isDevelopment()) {
    // Development build: "1.0.0+dev.5.a1b2c3d"
} else if (GitVersionInfo::isCIBuild()) {
    // CI build: "1.0.0+ci.123"
}
```

## Breaking Change Policy

### Communication
- **6 weeks notice** for MAJOR version breaking changes
- **Clear migration guides** with before/after examples
- **Deprecation warnings** in previous MINOR versions

### Documentation
- **CHANGELOG.md** with detailed breaking changes
- **Migration guides** for major version upgrades
- **API compatibility matrix** for supported versions

## Release Automation

Future GitHub Actions workflows will:
1. **Validate semver compliance** on PR/commit
2. **Auto-tag releases** based on conventional commits
3. **Generate changelogs** from commit messages
4. **Build release artifacts** (binaries, packages)
5. **Publish GitHub releases** with assets

This establishes nx as a professional, enterprise-ready CLI application with clear versioning standards.