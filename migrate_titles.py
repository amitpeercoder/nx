#!/usr/bin/env python3

import os
import re
import shutil
from datetime import datetime
from pathlib import Path

def parse_note_file(filepath):
    """Parse a note file into YAML frontmatter and content."""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Split by YAML frontmatter delimiters
    parts = content.split('---\n', 2)
    if len(parts) != 3:
        return None, None, None
    
    yaml_content = parts[1].strip()
    note_content = parts[2].strip()
    
    return yaml_content, note_content, content

def extract_title_from_yaml(yaml_content):
    """Extract title from YAML frontmatter."""
    for line in yaml_content.split('\n'):
        if line.startswith('title:'):
            title = line[6:].strip()
            # Remove quotes if present
            title = re.sub(r'^["\']|["\']$', '', title)
            return title
    return ""

def get_first_line_title(content):
    """Get the first non-empty line, cleaned up."""
    if not content:
        return ""
    
    first_line = content.split('\n')[0].strip()
    # Remove markdown heading markers
    first_line = re.sub(r'^#+\s*', '', first_line)
    return first_line

def needs_migration(stored_title, first_line):
    """Check if a note needs migration."""
    if not stored_title:
        return False  # Empty titles are ok
    
    if '{{' in stored_title:
        return True  # Template variables need manual attention
    
    # Check if first line matches title (with or without markdown)
    return first_line != stored_title

def migrate_note(filepath, stored_title, yaml_content, note_content):
    """Migrate a single note file."""
    # Add title as markdown heading at the beginning
    new_content = f"# {stored_title}\n\n{note_content}"
    
    # Reconstruct full file
    full_content = f"---\n{yaml_content}\n---\n\n{new_content}\n"
    
    # Write back to file
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(full_content)

def main():
    print("=== Note Title Migration Tool ===")
    print("This will update notes to ensure their title matches the first line of content.")
    
    notes_dir = Path.home() / ".local/share/nx/notes"
    if not notes_dir.exists():
        print(f"Error: Notes directory not found at {notes_dir}")
        return 1
    
    # Create backup
    backup_dir = notes_dir.parent / f"notes_backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    print(f"Creating backup at: {backup_dir}")
    shutil.copytree(notes_dir, backup_dir)
    
    # Find all note files
    note_files = list(notes_dir.glob("*.md"))
    print(f"Found {len(note_files)} notes to analyze...")
    
    if not note_files:
        print("No notes found to migrate.")
        return 0
    
    notes_to_migrate = []
    
    # Analyze each note
    for note_file in note_files:
        print(f"\nAnalyzing: {note_file.name}")
        
        yaml_content, note_content, full_content = parse_note_file(note_file)
        if yaml_content is None:
            print("  SKIP: Invalid format")
            continue
        
        stored_title = extract_title_from_yaml(yaml_content)
        first_line = get_first_line_title(note_content)
        
        print(f"  Stored title: '{stored_title}'")
        print(f"  First line: '{first_line}'")
        
        # Skip notebook markers
        if stored_title.startswith('.notebook'):
            print("  SKIP: Notebook marker file")
            continue
        
        # Check for template variables
        if '{{' in stored_title:
            print("  SKIP: Contains template variables (needs manual attention)")
            continue
        
        if needs_migration(stored_title, first_line):
            notes_to_migrate.append((note_file, stored_title, yaml_content, note_content))
            print("  NEEDS MIGRATION")
        else:
            print("  OK: Title already matches")
    
    print(f"\nAnalysis complete. {len(notes_to_migrate)} notes need migration.")
    
    if not notes_to_migrate:
        print("No migration needed!")
        return 0
    
    # Ask for confirmation
    response = input("Proceed with migration? (y/N): ")
    if response.lower() != 'y':
        print("Migration cancelled.")
        return 0
    
    # Perform migration
    print("Starting migration...")
    migrated_count = 0
    
    for note_file, stored_title, yaml_content, note_content in notes_to_migrate:
        try:
            migrate_note(note_file, stored_title, yaml_content, note_content)
            print(f"  MIGRATED: {note_file.name} - '{stored_title}'")
            migrated_count += 1
        except Exception as e:
            print(f"  ERROR: Failed to migrate {note_file.name}: {e}")
    
    print(f"\nMigration complete! Successfully migrated {migrated_count} notes.")
    print(f"Backup created at: {backup_dir}")
    
    if migrated_count > 0:
        print("\nYou can now use your notes with the new title system.")
        print(f"If something goes wrong, restore from backup: cp -r '{backup_dir}'/* '{notes_dir}'/")
    
    return 0

if __name__ == "__main__":
    exit(main())