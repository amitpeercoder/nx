# Semantic Versioning Implementation

This document describes the semantic versioning system implemented for nx.

## Overview

nx uses a git-based semantic versioning system that automatically generates version strings based on the git repository state, build type, and environment context.

## Version Format

### Release Builds (Clean Repository on Tag)
```
1.0.0
1.0.0-alpha.1
1.0.0-beta.2
1.0.0-rc.1
```

### Development Builds
```
0.1.0-dev.7.abc1234+dirty     # Development with uncommitted changes
0.1.0-dev.7.abc1234           # Development, clean
0.1.0-dev.abc1234             # Development, no commits since tag
```

### Debug Builds
```
0.1.0-debug.abc1234+dirty     # Debug with uncommitted changes
0.1.0-debug.abc1234           # Debug, clean
```

### CI/CD Builds
```
0.1.0-ci.7.abc1234            # CI build with commit count
0.1.0-ci.abc1234              # CI build on tag
```

## Implementation Details

### CMake Integration

The versioning system is implemented through:
- `cmake/GitVersion.cmake` - Git version detection logic
- `cmake/version.hpp.in` - C++ header template
- Generated `build/include/nx/version.hpp` - Runtime version info

### Version Detection Logic

1. **Git Information Extraction**:
   - Current commit hash (short)
   - Working tree dirty status
   - Commits since last tag
   - Latest semantic version tag

2. **Build Context Detection**:
   - CI environment variables (`CI`, `GITHUB_ACTIONS`, `GITLAB_CI`)
   - CMake build type (`Debug`, `Release`, etc.)
   - Working tree status

3. **Version String Construction**:
   - Base version from git tags or default (0.1.0)
   - Prerelease identifier based on context
   - Build metadata for development builds
   - Dirty flag for uncommitted changes

### C++ API

```cpp
#include "nx/version.hpp"

// Access version information
std::string version = nx::GitVersionInfo::getVersionString();
bool is_dev = nx::GitVersionInfo::isDevelopment();
bool is_dirty = nx::GitVersionInfo::isDirty();

// Individual components
int major = nx::GitVersionInfo::major;
int minor = nx::GitVersionInfo::minor;
int patch = nx::GitVersionInfo::patch;
```

## Usage Examples

### CMake Configuration
```bash
# Debug build
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
# Output: 0.1.0-debug.abc1234+dirty

# Release build
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
# Output: 0.1.0-dev.7.abc1234+dirty (if on development branch)
# Output: 1.0.0 (if on clean release tag)

# CI build
CI=true cmake -B build-ci -G Ninja -DCMAKE_BUILD_TYPE=Release
# Output: 0.1.0-ci.7.abc1234
```

### Runtime Version Check
```bash
./nx --version
# Examples:
# 0.1.0-dev.7.abc1234+dirty
# 1.0.0
# 0.1.0-debug.abc1234
```

## Tagging Strategy

### Development Tags
- Use pre-release identifiers: `v0.1.0-alpha.1`, `v0.1.0-beta.1`
- These generate versions like: `0.1.0-alpha.1`

### Release Tags
- Clean semantic version tags: `v1.0.0`, `v1.2.3`
- These generate clean versions: `1.0.0`, `1.2.3`

### Tag Creation
```bash
# Development pre-release
git tag v0.1.0-alpha.1
git push origin v0.1.0-alpha.1

# Release
git tag v1.0.0
git push origin v1.0.0
```

## Benefits

1. **Automatic Versioning**: No manual version updates required
2. **Context Awareness**: Different version formats for different contexts
3. **Git Integration**: Leverages git history for version information
4. **Build Traceability**: Commit hash and dirty status provide full traceability
5. **CI/CD Ready**: Automatic detection of CI environments
6. **Semantic Compliance**: Follows semantic versioning specification

## Environment Variables

The system respects these environment variables for CI detection:
- `CI` - Generic CI indicator
- `GITHUB_ACTIONS` - GitHub Actions
- `GITLAB_CI` - GitLab CI

## File Structure

```
cmake/
├── GitVersion.cmake          # Version detection logic
└── version.hpp.in           # C++ header template

build/include/nx/
└── version.hpp              # Generated version header (build-time)

src/common.cpp               # Version API implementation
include/nx/common.hpp        # Version API declaration
```

This implementation provides a robust, automated semantic versioning system that adapts to different development and deployment contexts.