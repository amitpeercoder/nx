# nx Shell Completions

This directory contains shell completion scripts for the nx CLI tool, providing intelligent auto-completion for commands, options, and nx-specific data like note IDs, notebooks, and tags.

## Features

### Supported Shells
- **Bash** (4.0+)
- **Zsh** (5.0+)

### Completion Capabilities
- ✅ **Commands**: Complete all nx commands and subcommands
- ✅ **Options**: Complete command-line flags and options
- ✅ **Note IDs**: Auto-complete note identifiers from your collection
- ✅ **Notebooks**: Complete notebook names from `nx notebook list`
- ✅ **Tags**: Complete tag names from `nx tags`
- ✅ **Templates**: Complete template names from `nx tpl list`
- ✅ **Config Keys**: Complete configuration keys for `nx config`
- ✅ **File Paths**: Complete file and directory paths where appropriate
- ✅ **Contextual**: Smart completion based on command context

### Examples

```bash
# Command completion
nx <TAB>                    # Shows all available commands
nx notebook <TAB>           # Shows: list create rename delete info

# Note ID completion
nx edit <TAB>               # Shows available note IDs
nx view <TAB>               # Shows available note IDs

# Notebook completion
nx new --nb <TAB>           # Shows available notebooks
nx ls --notebook <TAB>      # Shows available notebooks

# Tag completion
nx ls --tag <TAB>           # Shows available tags
nx new --tags <TAB>         # Shows available tags

# Config completion
nx config get <TAB>         # Shows available config keys
nx config set <TAB>         # Shows available config keys

# Option completion
nx export <TAB>             # Shows: md json pdf html
nx doctor --category <TAB>  # Shows: config storage git tools performance
```

## Installation

### Automatic Installation (Recommended)

Use the provided installation script:

```bash
# Install for current user (all shells)
./install.sh

# Install system-wide (requires sudo)
./install.sh --system

# Install for specific shell only
./install.sh --shell bash
./install.sh --shell zsh
```

### Manual Installation

#### Bash

1. **User installation:**
   ```bash
   mkdir -p ~/.local/share/bash-completion/completions
   cp nx.bash ~/.local/share/bash-completion/completions/nx
   echo "source ~/.local/share/bash-completion/completions/nx" >> ~/.bashrc
   ```

2. **System installation:**
   ```bash
   sudo cp nx.bash /usr/share/bash-completion/completions/nx
   # or
   sudo cp nx.bash /etc/bash_completion.d/nx
   ```

#### Zsh

1. **User installation:**
   ```bash
   mkdir -p ~/.local/share/zsh/site-functions
   cp _nx ~/.local/share/zsh/site-functions/
   echo 'fpath=(~/.local/share/zsh/site-functions $fpath)' >> ~/.zshrc
   echo 'autoload -U compinit && compinit' >> ~/.zshrc
   ```

2. **System installation:**
   ```bash
   sudo cp _nx /usr/share/zsh/site-functions/
   # or
   sudo cp _nx /usr/local/share/zsh/site-functions/
   ```

## Activation

After installation, restart your shell or source your configuration:

```bash
# Bash
source ~/.bashrc

# Zsh
source ~/.zshrc
```

## Testing

Test the completions by typing `nx` followed by `<TAB><TAB>`:

```bash
nx <TAB><TAB>
# Should show all available commands

nx new --<TAB><TAB>
# Should show available options for the new command

nx edit <TAB><TAB>
# Should show available note IDs (if you have notes)
```

## Dynamic Completions

The completion scripts include dynamic completions that query your nx installation for up-to-date data:

- **Note IDs**: Fetched from `nx ls`
- **Notebooks**: Fetched from `nx notebook list`
- **Tags**: Fetched from `nx tags`
- **Templates**: Fetched from `nx tpl list`

This ensures completions are always current with your actual data.

## Requirements

- **nx CLI tool** must be installed and in your PATH
- **jq** (optional, for better JSON parsing of completions)
- **Bash 4.0+** or **Zsh 5.0+**

## Troubleshooting

### Completions Not Working

1. **Check nx is in PATH:**
   ```bash
   which nx
   nx --version
   ```

2. **Verify completion files are installed:**
   ```bash
   # Bash
   ls -la ~/.local/share/bash-completion/completions/nx
   
   # Zsh
   ls -la ~/.local/share/zsh/site-functions/_nx
   ```

3. **Test completion loading:**
   ```bash
   # Bash
   source ~/.local/share/bash-completion/completions/nx
   
   # Zsh
   autoload -U compinit && compinit
   ```

4. **Check shell configuration:**
   ```bash
   # Bash
   grep -n "nx" ~/.bashrc
   
   # Zsh
   grep -n "nx\|fpath\|compinit" ~/.zshrc
   ```

### Slow Completions

If completions feel slow, it may be due to large note collections. The completion scripts limit results to reasonable numbers, but you can:

1. Ensure nx commands complete quickly:
   ```bash
   time nx ls >/dev/null
   time nx notebook list >/dev/null
   ```

2. Consider using `--json` output for faster parsing (requires jq)

### Permission Issues

If you encounter permission issues:

```bash
# For user installations
chmod 644 ~/.local/share/bash-completion/completions/nx
chmod 644 ~/.local/share/zsh/site-functions/_nx

# For system installations (with sudo)
sudo chmod 644 /usr/share/bash-completion/completions/nx
sudo chmod 644 /usr/share/zsh/site-functions/_nx
```

## Contributing

To improve or extend the completions:

1. Edit the appropriate completion file (`nx.bash` or `_nx`)
2. Test your changes by sourcing the file
3. Submit a pull request with your improvements

The completion scripts are designed to be:
- **Fast**: Minimal overhead for common completions
- **Accurate**: Context-aware completions
- **Robust**: Graceful fallbacks when nx commands fail
- **Maintainable**: Clear structure and good comments