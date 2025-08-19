#!/bin/bash

# Test migration on a single file
set -e

note_file="/tmp/test_note.md"

echo "Testing migration on: $note_file"
echo "Original content:"
echo "----------------"
cat "$note_file"
echo "----------------"

# Read the file
content=$(cat "$note_file")

# Split into YAML frontmatter and content
yaml_content=$(echo "$content" | sed -n '/^---$/,/^---$/p' | sed '1d;$d')
note_content=$(echo "$content" | sed -n '/^---$/,//p' | sed '1,2d')

echo "Extracted YAML:"
echo "$yaml_content"
echo ""

echo "Extracted content:"
echo "$note_content"
echo ""

# Extract current title from YAML
current_title=$(echo "$yaml_content" | grep "^title:" | sed 's/^title: //' | sed 's/^"//' | sed 's/"$//')

echo "Current title: '$current_title'"

# Get first line of content
first_line=$(echo "$note_content" | head -n1 | sed 's/^[[:space:]]*//' | sed 's/[[:space:]]*$//')
echo "First line: '$first_line'"

# Check if migration needed
title_without_md=$(echo "$first_line" | sed 's/^#\+ *//')

if [[ "$first_line" == "$current_title" ]] || [[ "$title_without_md" == "$current_title" ]]; then
    echo "No migration needed"
else
    echo "Migration needed - will prepend title as heading"
    
    # Create new content with title as heading
    new_note_content="# $current_title"$'\n'$'\n'"$note_content"
    
    # Reconstruct the file
    new_content="---"$'\n'"$yaml_content"$'\n'"---"$'\n'$'\n'"$new_note_content"
    
    echo ""
    echo "New content would be:"
    echo "----------------"
    echo "$new_content"
    echo "----------------"
fi