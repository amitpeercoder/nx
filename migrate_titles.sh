#!/bin/bash

# Migration script for nx notes to ensure titles match first line of content
set -e

echo "=== Note Title Migration Tool ==="
echo "This will update notes to ensure their title matches the first line of content."

NOTES_DIR="$HOME/.local/share/nx/notes"
BACKUP_DIR="${NOTES_DIR}_backup_$(date +%Y%m%d_%H%M%S)"

if [ ! -d "$NOTES_DIR" ]; then
    echo "Error: Notes directory not found at $NOTES_DIR"
    exit 1
fi

# Create backup
echo "Creating backup at: $BACKUP_DIR"
cp -r "$NOTES_DIR" "$BACKUP_DIR"

# Count notes
TOTAL_NOTES=$(ls "$NOTES_DIR"/*.md 2>/dev/null | wc -l || echo 0)
echo "Found $TOTAL_NOTES notes to analyze..."

if [ "$TOTAL_NOTES" -eq 0 ]; then
    echo "No notes found to migrate."
    exit 0
fi

MIGRATED=0

# Process each note file
for note_file in "$NOTES_DIR"/*.md; do
    if [ ! -f "$note_file" ]; then
        continue
    fi
    
    basename=$(basename "$note_file")
    echo "Processing: $basename"
    
    # Read the file
    if ! content=$(cat "$note_file"); then
        echo "  ERROR: Could not read $basename"
        continue
    fi
    
    # Split into YAML frontmatter and content
    if ! echo "$content" | grep -q "^---$"; then
        echo "  SKIP: No YAML frontmatter in $basename"
        continue
    fi
    
    # Extract YAML frontmatter
    yaml_content=$(echo "$content" | sed -n '/^---$/,/^---$/p' | sed '1d;$d')
    note_content=$(echo "$content" | sed -n '/^---$/,//p' | sed '1,2d')
    
    # Extract current title from YAML
    current_title=$(echo "$yaml_content" | grep "^title:" | sed 's/^title: //' | sed 's/^"//' | sed 's/"$//')
    
    # Skip notebook files
    if [[ "$current_title" == .notebook* ]]; then
        echo "  SKIP: Notebook marker file"
        continue
    fi
    
    # Skip if title contains template variables
    if [[ "$current_title" == *"{{"* ]]; then
        echo "  SKIP: Contains template variables (needs manual attention)"
        continue
    fi
    
    # Get first line of content
    first_line=$(echo "$note_content" | head -n1 | sed 's/^[[:space:]]*//' | sed 's/[[:space:]]*$//')
    
    # Check if first line already matches title (with or without markdown heading)
    title_without_md=$(echo "$first_line" | sed 's/^#\+ *//')
    
    if [[ "$first_line" == "$current_title" ]] || [[ "$title_without_md" == "$current_title" ]] || [[ -z "$current_title" ]]; then
        echo "  OK: Title already matches first line"
        continue
    fi
    
    # Need to migrate - prepend title as markdown heading
    if [[ -n "$current_title" ]]; then
        echo "  MIGRATING: Adding title '$current_title' as first line"
        
        # Create new content with title as heading
        new_note_content="# $current_title"$'\n'$'\n'"$note_content"
        
        # Reconstruct the file
        new_content="---"$'\n'"$yaml_content"$'\n'"---"$'\n'$'\n'"$new_note_content"
        
        # Write back to file
        if echo "$new_content" > "$note_file"; then
            echo "  SUCCESS: Migrated $basename"
            ((MIGRATED++))
        else
            echo "  ERROR: Failed to write $basename"
        fi
    else
        echo "  SKIP: Empty title"
    fi
done

echo ""
echo "Migration complete!"
echo "  Total notes: $TOTAL_NOTES"
echo "  Migrated: $MIGRATED"
echo "  Backup created at: $BACKUP_DIR"
echo ""

if [ "$MIGRATED" -gt 0 ]; then
    echo "You can now use your notes with the new title system."
    echo "If something goes wrong, restore from backup: cp -r \"$BACKUP_DIR\"/* \"$NOTES_DIR\"/\"
fi