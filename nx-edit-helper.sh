#!/bin/bash

# External editor helper script - completely independent of nx TUI process
# This should give vim a completely clean environment

# Get parameters
EDITOR_CMD="$1"
FILE_PATH="$2"

# Complete terminal reset
printf '\033c'
sleep 0.1

# Clear any environment variables that might interfere
unset LINES COLUMNS

# Reset all terminal modes to defaults
stty sane
reset

# Small delay to ensure terminal is ready
sleep 0.1

# Launch editor with clean environment
exec "$EDITOR_CMD" "$FILE_PATH"