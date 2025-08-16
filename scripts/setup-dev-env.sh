#!/bin/bash
# Development environment setup script for nx project
# This script sets up pre-commit hooks, dependencies, and development tools

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if we're in the project root
if [[ ! -f "CMakeLists.txt" ]] || [[ ! -f ".pre-commit-config.yaml" ]]; then
    log_error "Please run this script from the nx project root directory"
    exit 1
fi

log_info "Setting up nx development environment..."

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    log_error "Python 3 is required but not installed"
    exit 1
fi

# Install pre-commit if not available
if ! command -v pre-commit &> /dev/null; then
    log_info "Installing pre-commit..."
    if command -v pip3 &> /dev/null; then
        pip3 install --user pre-commit
    elif command -v brew &> /dev/null; then
        brew install pre-commit
    elif command -v apt-get &> /dev/null; then
        sudo apt-get update && sudo apt-get install -y python3-pip
        pip3 install --user pre-commit
    else
        log_error "Cannot install pre-commit. Please install manually: pip install pre-commit"
        exit 1
    fi
fi

# Install pre-commit hooks
log_info "Installing pre-commit hooks..."
pre-commit install
pre-commit install --hook-type commit-msg

# Install additional development tools based on platform
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    log_info "Setting up Linux development tools..."
    
    # Check for package manager and install tools
    if command -v apt-get &> /dev/null; then
        log_info "Installing development dependencies via apt..."
        sudo apt-get update
        sudo apt-get install -y \
            clang-format \
            clang-tidy \
            cppcheck \
            ninja-build \
            cmake \
            pkg-config \
            libsqlite3-dev \
            libgit2-dev \
            libcurl4-openssl-dev \
            valgrind \
            lcov
    elif command -v dnf &> /dev/null; then
        log_info "Installing development dependencies via dnf..."
        sudo dnf install -y \
            clang-tools-extra \
            cppcheck \
            ninja-build \
            cmake \
            pkgconfig \
            sqlite-devel \
            libgit2-devel \
            libcurl-devel \
            valgrind \
            lcov
    elif command -v pacman &> /dev/null; then
        log_info "Installing development dependencies via pacman..."
        sudo pacman -S --needed \
            clang \
            cppcheck \
            ninja \
            cmake \
            pkgconf \
            sqlite \
            libgit2 \
            curl \
            valgrind \
            lcov
    else
        log_warning "Unknown package manager. Please install development tools manually."
    fi
    
elif [[ "$OSTYPE" == "darwin"* ]]; then
    log_info "Setting up macOS development tools..."
    
    # Check if Homebrew is available
    if command -v brew &> /dev/null; then
        log_info "Installing development dependencies via Homebrew..."
        brew install \
            llvm \
            cppcheck \
            ninja \
            cmake \
            pkg-config \
            sqlite \
            libgit2 \
            curl \
            lcov
    else
        log_warning "Homebrew not found. Please install development tools manually."
        log_info "Consider installing Homebrew: https://brew.sh"
    fi
    
else
    log_warning "Unsupported OS type: $OSTYPE"
    log_info "Please install development tools manually"
fi

# Setup vcpkg if not present
if [[ ! -d "vcpkg" ]]; then
    log_info "Setting up vcpkg package manager..."
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    
    if [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
        ./bootstrap-vcpkg.bat
    else
        ./bootstrap-vcpkg.sh
    fi
    
    cd ..
    log_success "vcpkg installed successfully"
else
    log_info "vcpkg already present, updating..."
    cd vcpkg
    git pull
    cd ..
fi

# Create build directories
log_info "Creating build directories..."
mkdir -p build-debug build-release build-sanitizer

# Run initial checks
log_info "Running initial code quality checks..."

# Check formatting
if command -v clang-format &> /dev/null; then
    log_info "Checking code formatting..."
    if find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format --dry-run --Werror &> /dev/null; then
        log_success "Code formatting check passed"
    else
        log_warning "Code formatting issues found. Run 'clang-format -i src/**/*.cpp src/**/*.hpp' to fix"
    fi
else
    log_warning "clang-format not found, skipping format check"
fi

# Test pre-commit hooks
log_info "Testing pre-commit hooks..."
if pre-commit run --all-files &> /dev/null; then
    log_success "Pre-commit hooks working correctly"
else
    log_warning "Pre-commit hooks found issues. This is normal for first setup."
fi

# Generate initial build files
log_info "Generating initial build configuration..."
if cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug &> /dev/null; then
    log_success "Debug build configuration generated"
else
    log_warning "Could not generate debug build configuration. Check CMake setup."
fi

# Create useful aliases and scripts
log_info "Creating development helper scripts..."

cat > scripts/format-code.sh << 'EOF'
#!/bin/bash
# Format all source code
find src include tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
echo "Code formatting complete"
EOF

cat > scripts/run-tests.sh << 'EOF'
#!/bin/bash
# Run all tests with coverage
set -e

echo "Building tests..."
cmake -B build-test -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage"
cmake --build build-test

echo "Running tests..."
cd build-test
ctest --output-on-failure

echo "Generating coverage report..."
if command -v lcov &> /dev/null; then
    lcov --capture --directory . --output-file coverage.info
    lcov --remove coverage.info '/usr/*' '*/vcpkg/*' '*/build-test/*' --output-file coverage.info
    lcov --list coverage.info
else
    echo "lcov not found, skipping coverage report"
fi
EOF

cat > scripts/static-analysis.sh << 'EOF'
#!/bin/bash
# Run static analysis tools
set -e

echo "Running cppcheck..."
if command -v cppcheck &> /dev/null; then
    cppcheck --enable=all --error-exitcode=0 \
        --suppress=missingIncludeSystem \
        --suppress=unusedFunction \
        --inline-suppr \
        src/ include/
else
    echo "cppcheck not found"
fi

echo "Running clang-tidy..."
if command -v clang-tidy &> /dev/null; then
    cmake -B build-analysis -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    find src -name "*.cpp" | head -5 | xargs clang-tidy -p build-analysis
else
    echo "clang-tidy not found"
fi
EOF

chmod +x scripts/*.sh

log_success "Development environment setup complete!"
echo
log_info "Next steps:"
echo "  1. Build the project: cmake --build build-debug"
echo "  2. Run tests: ./scripts/run-tests.sh"
echo "  3. Format code: ./scripts/format-code.sh"
echo "  4. Run static analysis: ./scripts/static-analysis.sh"
echo
log_info "Pre-commit hooks are now active and will run on every commit."
log_info "To run hooks manually: pre-commit run --all-files"
echo
log_success "Happy coding! 🚀"