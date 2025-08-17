#!/bin/bash
set -euo pipefail

# nx CLI Notes Application Installation Script
# Version: 1.0.0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
COMPLETIONS_DIR="${COMPLETIONS_DIR:-/usr/local/share/bash-completion/completions}"
ZSH_COMPLETIONS_DIR="${ZSH_COMPLETIONS_DIR:-/usr/local/share/zsh/site-functions}"
DOCS_DIR="${DOCS_DIR:-/usr/local/share/doc/nx}"

# Detect platform
OS="$(uname -s)"
ARCH="$(uname -m)"

case "$OS" in
    Darwin*)
        PLATFORM="darwin"
        ;;
    Linux*)
        PLATFORM="linux"
        ;;
    *)
        echo -e "${RED}Error: Unsupported operating system: $OS${NC}" >&2
        exit 1
        ;;
esac

case "$ARCH" in
    x86_64|amd64)
        ARCH="x86_64"
        ;;
    arm64|aarch64)
        ARCH="arm64"
        ;;
    *)
        echo -e "${RED}Error: Unsupported architecture: $ARCH${NC}" >&2
        exit 1
        ;;
esac

echo -e "${BLUE}nx CLI Notes Application Installer${NC}"
echo -e "${BLUE}====================================${NC}"
echo ""
echo "Platform: $PLATFORM-$ARCH"
echo "Install directory: $INSTALL_DIR"
echo ""

# Check if we're in the extracted release directory
if [[ ! -f "./nx" ]]; then
    echo -e "${RED}Error: nx binary not found in current directory${NC}" >&2
    echo "Please extract the release package and run this script from the extracted directory." >&2
    exit 1
fi

# Check if we have write permissions
if [[ ! -w "$INSTALL_DIR" ]]; then
    echo -e "${YELLOW}Warning: No write permission to $INSTALL_DIR${NC}"
    echo "You may need to run this script with sudo or choose a different installation directory."
    echo ""
    read -p "Continue with installation? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Install binary
echo -e "${BLUE}Installing nx binary...${NC}"
if cp "./nx" "$INSTALL_DIR/nx"; then
    chmod +x "$INSTALL_DIR/nx"
    echo -e "${GREEN}✓ Binary installed to $INSTALL_DIR/nx${NC}"
else
    echo -e "${RED}✗ Failed to install binary${NC}" >&2
    exit 1
fi

# Install shell completions
if [[ -d "./completions" ]]; then
    echo -e "${BLUE}Installing shell completions...${NC}"
    
    # Bash completions
    if [[ -d "$COMPLETIONS_DIR" ]] || mkdir -p "$COMPLETIONS_DIR" 2>/dev/null; then
        if cp "./completions/nx.bash" "$COMPLETIONS_DIR/nx" 2>/dev/null; then
            echo -e "${GREEN}✓ Bash completions installed${NC}"
        else
            echo -e "${YELLOW}⚠ Could not install bash completions${NC}"
        fi
    fi
    
    # Zsh completions
    if [[ -d "$ZSH_COMPLETIONS_DIR" ]] || mkdir -p "$ZSH_COMPLETIONS_DIR" 2>/dev/null; then
        if cp "./completions/_nx" "$ZSH_COMPLETIONS_DIR/_nx" 2>/dev/null; then
            echo -e "${GREEN}✓ Zsh completions installed${NC}"
        else
            echo -e "${YELLOW}⚠ Could not install zsh completions${NC}"
        fi
    fi
fi

# Install documentation
if [[ -d "./docs" ]]; then
    echo -e "${BLUE}Installing documentation...${NC}"
    if mkdir -p "$DOCS_DIR" 2>/dev/null && cp -r "./docs/"* "$DOCS_DIR/" 2>/dev/null; then
        echo -e "${GREEN}✓ Documentation installed to $DOCS_DIR${NC}"
    else
        echo -e "${YELLOW}⚠ Could not install documentation${NC}"
    fi
fi

# Verify installation
echo ""
echo -e "${BLUE}Verifying installation...${NC}"
if command -v nx >/dev/null 2>&1; then
    VERSION=$(nx --version 2>/dev/null || echo "unknown")
    echo -e "${GREEN}✓ nx is installed and available in PATH${NC}"
    echo "Version: $VERSION"
else
    echo -e "${YELLOW}⚠ nx is installed but not in PATH${NC}"
    echo "You may need to add $INSTALL_DIR to your PATH environment variable."
fi

echo ""
echo -e "${GREEN}Installation completed!${NC}"
echo ""
echo "Quick start:"
echo "  nx new \"My First Note\"     # Create a new note"
echo "  nx ls                       # List all notes"
echo "  nx ui                       # Launch interactive TUI"
echo "  nx --help                   # Show all available commands"
echo ""
echo "For more information, visit the documentation at:"
echo "  $DOCS_DIR/"
echo ""
echo -e "${BLUE}Happy note-taking! 📝${NC}"