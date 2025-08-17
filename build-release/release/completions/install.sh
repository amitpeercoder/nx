#!/bin/bash
# Installation script for nx shell completions
# Usage: ./install.sh [--user|--system] [--shell bash|zsh|all]

set -e

# Default values
INSTALL_TYPE="user"
SHELL_TYPE="all"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

usage() {
    echo "Install nx shell completions"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --user      Install for current user only (default)"
    echo "  --system    Install system-wide (requires sudo)"
    echo "  --shell     Shell type: bash, zsh, or all (default: all)"
    echo "  --help      Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Install for current user, all shells"
    echo "  $0 --system          # Install system-wide, all shells"
    echo "  $0 --shell bash      # Install bash completion only"
    echo "  $0 --shell zsh       # Install zsh completion only"
}

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

detect_shell() {
    if [[ -n "$ZSH_VERSION" ]]; then
        echo "zsh"
    elif [[ -n "$BASH_VERSION" ]]; then
        echo "bash"
    else
        # Fallback to examining $SHELL
        case "$SHELL" in
            */zsh) echo "zsh" ;;
            */bash) echo "bash" ;;
            *) echo "unknown" ;;
        esac
    fi
}

install_bash_completion() {
    local install_type="$1"
    local source_file="$SCRIPT_DIR/nx.bash"
    
    if [[ ! -f "$source_file" ]]; then
        log_error "Bash completion file not found: $source_file"
        return 1
    fi
    
    if [[ "$install_type" == "system" ]]; then
        # System-wide installation
        local target_dirs=(
            "/usr/share/bash-completion/completions"
            "/etc/bash_completion.d"
            "/usr/local/share/bash-completion/completions"
        )
        
        local installed=false
        for dir in "${target_dirs[@]}"; do
            if [[ -d "$dir" ]]; then
                log_info "Installing bash completion to $dir/nx"
                sudo cp "$source_file" "$dir/nx"
                sudo chmod 644 "$dir/nx"
                installed=true
                break
            fi
        done
        
        if [[ "$installed" == false ]]; then
            log_error "No suitable system bash completion directory found"
            log_info "Tried: ${target_dirs[*]}"
            return 1
        fi
    else
        # User installation
        local completion_dir="$HOME/.local/share/bash-completion/completions"
        
        if [[ ! -d "$completion_dir" ]]; then
            log_info "Creating user bash completion directory: $completion_dir"
            mkdir -p "$completion_dir"
        fi
        
        log_info "Installing bash completion to $completion_dir/nx"
        cp "$source_file" "$completion_dir/nx"
        chmod 644 "$completion_dir/nx"
        
        # Add sourcing to ~/.bashrc if not already present
        local bashrc="$HOME/.bashrc"
        local source_line="source $completion_dir/nx"
        
        if [[ -f "$bashrc" ]]; then
            if ! grep -Fxq "$source_line" "$bashrc"; then
                echo "" >> "$bashrc"
                echo "# nx shell completion" >> "$bashrc"
                echo "$source_line" >> "$bashrc"
                log_info "Added sourcing line to $bashrc"
            else
                log_info "Sourcing line already exists in $bashrc"
            fi
        else
            log_warning "$bashrc not found. You may need to source the completion manually."
        fi
    fi
    
    log_success "Bash completion installed successfully"
}

install_zsh_completion() {
    local install_type="$1"
    local source_file="$SCRIPT_DIR/_nx"
    
    if [[ ! -f "$source_file" ]]; then
        log_error "Zsh completion file not found: $source_file"
        return 1
    fi
    
    if [[ "$install_type" == "system" ]]; then
        # System-wide installation
        local target_dirs=(
            "/usr/share/zsh/site-functions"
            "/usr/local/share/zsh/site-functions"
            "/usr/share/zsh/functions/Completion"
        )
        
        local installed=false
        for dir in "${target_dirs[@]}"; do
            if [[ -d "$dir" ]]; then
                log_info "Installing zsh completion to $dir/_nx"
                sudo cp "$source_file" "$dir/_nx"
                sudo chmod 644 "$dir/_nx"
                installed=true
                break
            fi
        done
        
        if [[ "$installed" == false ]]; then
            log_error "No suitable system zsh completion directory found"
            log_info "Tried: ${target_dirs[*]}"
            return 1
        fi
    else
        # User installation
        local completion_dir="$HOME/.local/share/zsh/site-functions"
        
        if [[ ! -d "$completion_dir" ]]; then
            log_info "Creating user zsh completion directory: $completion_dir"
            mkdir -p "$completion_dir"
        fi
        
        log_info "Installing zsh completion to $completion_dir/_nx"
        cp "$source_file" "$completion_dir/_nx"
        chmod 644 "$completion_dir/_nx"
        
        # Add to fpath in ~/.zshrc if not already present
        local zshrc="$HOME/.zshrc"
        local fpath_line="fpath=(\"$completion_dir\" \$fpath)"
        
        if [[ -f "$zshrc" ]]; then
            if ! grep -Fq "$completion_dir" "$zshrc"; then
                echo "" >> "$zshrc"
                echo "# nx shell completion" >> "$zshrc"
                echo "$fpath_line" >> "$zshrc"
                echo "autoload -U compinit && compinit" >> "$zshrc"
                log_info "Added fpath and compinit to $zshrc"
            else
                log_info "Completion directory already in fpath in $zshrc"
            fi
        else
            log_warning "$zshrc not found. You may need to add the completion to your fpath manually."
        fi
    fi
    
    log_success "Zsh completion installed successfully"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --user)
            INSTALL_TYPE="user"
            shift
            ;;
        --system)
            INSTALL_TYPE="system"
            shift
            ;;
        --shell)
            SHELL_TYPE="$2"
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Validate shell type
case "$SHELL_TYPE" in
    bash|zsh|all)
        ;;
    *)
        log_error "Invalid shell type: $SHELL_TYPE"
        log_info "Valid options: bash, zsh, all"
        exit 1
        ;;
esac

# Check if running as root when system install requested
if [[ "$INSTALL_TYPE" == "system" && $EUID -ne 0 ]]; then
    log_info "System installation requested. Checking sudo access..."
    sudo -v || {
        log_error "sudo access required for system installation"
        exit 1
    }
fi

# Detect current shell if not specified
current_shell=$(detect_shell)
log_info "Detected shell: $current_shell"
log_info "Install type: $INSTALL_TYPE"
log_info "Target shells: $SHELL_TYPE"

echo ""

# Install completions based on shell type
case "$SHELL_TYPE" in
    bash)
        install_bash_completion "$INSTALL_TYPE"
        ;;
    zsh)
        install_zsh_completion "$INSTALL_TYPE"
        ;;
    all)
        install_bash_completion "$INSTALL_TYPE" || true
        echo ""
        install_zsh_completion "$INSTALL_TYPE" || true
        ;;
esac

echo ""
log_success "Installation complete!"
echo ""
log_info "To activate completions:"

if [[ "$INSTALL_TYPE" == "user" ]]; then
    case "$SHELL_TYPE" in
        bash)
            echo "  - Restart your bash session or run: source ~/.bashrc"
            ;;
        zsh)
            echo "  - Restart your zsh session or run: source ~/.zshrc"
            ;;
        all)
            echo "  - Restart your shell session or:"
            echo "    - For bash: source ~/.bashrc"
            echo "    - For zsh: source ~/.zshrc"
            ;;
    esac
else
    echo "  - Restart your shell session"
    echo "  - Completions should be automatically available"
fi

echo ""
log_info "Test completions with: nx <TAB><TAB>"