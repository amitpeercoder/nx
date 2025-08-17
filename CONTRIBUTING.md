# Contributing to nx

Thank you for your interest in contributing to nx! This guide will help you get started with development and understand our contribution process.

## 🎯 Project Goals

nx aims to be the fastest, most reliable CLI note-taking application that respects user data and privacy:

- **Performance**: Sub-100ms operations on 10k+ notes
- **Local-first**: Full offline functionality
- **Security**: Safe handling of user data
- **Composability**: Works well with Unix tools
- **Maintainability**: Clear, well-tested code

## 🚀 Getting Started

### Prerequisites

- **C++20/23 compatible compiler** (GCC 10+, Clang 12+)
- **CMake 3.20+**
- **Ninja build system**
- **vcpkg** for dependency management
- **Git** for version control

### Development Setup

1. **Clone the repository**
   ```bash
   git clone https://github.com/your-org/nx
   cd nx
   ```

2. **Install dependencies**
   ```bash
   # Install vcpkg if not already installed
   git clone https://github.com/Microsoft/vcpkg.git
   ./vcpkg/bootstrap-vcpkg.sh
   
   # Dependencies will be automatically installed by CMake
   ```

3. **Build the project**
   ```bash
   # Debug build with sanitizers
   cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=address
   cmake --build build-debug
   
   # Release build
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

4. **Run tests**
   ```bash
   ctest --test-dir build-debug --output-on-failure
   ```

5. **Set up development tools**
   ```bash
   # Install pre-commit hooks
   pip install pre-commit
   pre-commit install
   ```

## 📋 Development Workflow

### Before You Start

1. Check existing [issues](https://github.com/your-org/nx/issues) and [MVP2 plan](./mvp2-plan.md)
2. Create an issue for new features or bugs
3. Fork the repository
4. Create a feature branch: `git checkout -b feature/your-feature-name`

### Code Standards

#### C++ Guidelines
- **Modern C++**: Use C++20/23 features where appropriate
- **RAII**: No raw owning pointers, prefer `unique_ptr`
- **Error handling**: Use `std::expected<T, Error>` pattern
- **No exceptions**: In hot paths or core functionality
- **Const correctness**: Mark methods and variables const when possible
- **Move semantics**: Use move constructors and assignment operators

#### Code Style
- **Format**: Use `.clang-format` configuration (run `make format`)
- **Naming**: 
  - Classes: `PascalCase`
  - Functions/variables: `snake_case`
  - Constants: `kCamelCase`
  - Members: `snake_case_` (trailing underscore)
- **Headers**: Use `#pragma once`
- **Includes**: System headers first, then third-party, then project headers

#### Testing
- **Unit tests**: For all new functionality
- **Integration tests**: For command-line interfaces
- **Performance tests**: For performance-critical code
- **Coverage**: Aim for >80% test coverage

### Project Structure

```
nx/
├── src/                    # Source code
│   ├── app/               # Application entry point
│   ├── cli/               # Command-line interface
│   ├── core/              # Core domain models
│   ├── store/             # Data storage
│   ├── index/             # Search indexing
│   ├── tui/               # Terminal user interface
│   ├── sync/              # Git synchronization
│   ├── crypto/            # Encryption
│   └── util/              # Utilities
├── include/nx/            # Public headers
├── tests/                 # Test suite
│   ├── unit/              # Unit tests
│   ├── integration/       # Integration tests
│   └── benchmark/         # Performance tests
├── docs/                  # Documentation
└── dev/scripts/           # Development scripts
```

## 🐛 Bug Reports

When reporting bugs, include:

1. **Environment**: OS, compiler version, nx version
2. **Steps to reproduce**: Minimal example
3. **Expected behavior**: What should happen
4. **Actual behavior**: What actually happens
5. **Logs**: Any error messages or debug output
6. **Data**: Sample notes that trigger the issue (if safe to share)

## ✨ Feature Requests

For new features:

1. **Check the roadmap**: See [mvp2-plan.md](./mvp2-plan.md)
2. **Describe the use case**: Why is this needed?
3. **Proposed solution**: How should it work?
4. **Alternatives**: Other approaches considered
5. **Breaking changes**: Impact on existing functionality

## 🔧 Development Areas

### High-Priority Areas

1. **Core Commands** (Phase 1)
   - File attachment system
   - Directory import functionality
   - Template management
   - Metadata commands
   - System maintenance tools

2. **Performance** (Ongoing)
   - Search optimization
   - Large dataset handling
   - Memory usage optimization

3. **Security** (Critical)
   - Command injection fixes
   - Secure temporary files
   - Input validation

### Good First Issues

- Documentation improvements
- Unit test additions
- Small bug fixes
- Code quality improvements
- Performance micro-optimizations

## 🧪 Testing Guidelines

### Running Tests

```bash
# All tests
ctest --test-dir build-debug --output-on-failure

# Unit tests only
ctest --test-dir build-debug -L unit

# Integration tests
ctest --test-dir build-debug -L integration

# Performance tests
./build/tests/benchmarks/nx_bench

# With sanitizers
cmake -B build-debug -DSANITIZE=address,undefined
```

### Writing Tests

#### Unit Tests
```cpp
#include <gtest/gtest.h>
#include "nx/core/note.hpp"

class NoteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
    }
};

TEST_F(NoteTest, CreateValidNote) {
    auto note = Note::create("Test Title", "Content");
    ASSERT_TRUE(note.has_value());
    EXPECT_EQ(note->title(), "Test Title");
}
```

#### Integration Tests
```cpp
TEST(CLITest, CreateAndListNotes) {
    TemporaryDirectory temp_dir;
    
    // Create note
    auto result = runCommand({"new", "Test Note"}, temp_dir.path());
    EXPECT_EQ(result.exit_code, 0);
    
    // List notes
    auto list_result = runCommand({"ls"}, temp_dir.path());
    EXPECT_THAT(list_result.stdout, HasSubstr("Test Note"));
}
```

### Performance Requirements

- **Note creation**: < 50ms P95
- **Search (FTS)**: < 200ms P95 on 10k notes
- **List operations**: < 100ms P95 on 10k notes
- **Full reindex**: < 45s on mid-range laptop
- **Memory usage**: < 100MB for typical operations

## 📝 Documentation

### Code Documentation
- **Public APIs**: Comprehensive Doxygen comments
- **Complex algorithms**: Inline comments explaining approach
- **TODOs**: Use `// TODO(username): description` format

### User Documentation
- **Command help**: Keep `--help` output concise and useful
- **Man pages**: Update for new commands
- **Examples**: Include practical examples in documentation

## 🚀 Release Process

### Version Numbering
We use semantic versioning (MAJOR.MINOR.PATCH):
- **MAJOR**: Breaking changes to CLI interface
- **MINOR**: New features, non-breaking changes
- **PATCH**: Bug fixes, performance improvements

### Release Checklist
- [ ] All tests pass
- [ ] Performance benchmarks meet targets
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
- [ ] Version bumped in CMakeLists.txt
- [ ] Git tag created
- [ ] Release packages built

## 🤝 Community Guidelines

### Code of Conduct
- Be respectful and inclusive
- Focus on constructive feedback
- Help newcomers learn and contribute
- Assume good intentions

### Communication
- **Issues**: For bug reports and feature requests
- **Discussions**: For questions and design discussions
- **Pull Requests**: For code contributions

### Review Process
1. **Automated checks**: CI must pass
2. **Code review**: At least one maintainer approval
3. **Testing**: Manual testing for UI changes
4. **Documentation**: Updates for user-facing changes

## 🎯 Current Priorities

See [mvp2-plan.md](./mvp2-plan.md) for the current development roadmap. High-priority areas include:

1. **Completing MVP2 Phase 1**: Core missing commands
2. **Security fixes**: Resolving command injection vulnerabilities
3. **Performance optimization**: Meeting sub-100ms targets
4. **Test coverage**: Increasing test coverage to >80%

## 📞 Getting Help

- **Documentation**: Check existing docs first
- **Issues**: Search existing issues before creating new ones
- **Discussions**: Use GitHub Discussions for questions
- **Code review**: Ask for help in pull request comments

Thank you for contributing to nx! Your help makes this project better for everyone. 🚀