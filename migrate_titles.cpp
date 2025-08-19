#include "nx/cli/application.hpp"
#include "nx/core/note.hpp"
#include <iostream>
#include <vector>
#include <set>

using namespace nx;
using namespace nx::core;

// Function to check if content already starts with the current title
bool contentStartsWithTitle(const Note& note, const std::string& stored_title) {
    if (stored_title.empty() || note.content().empty()) {
        return false;
    }
    
    std::string first_line;
    size_t end_of_line = note.content().find('\n');
    if (end_of_line != std::string::npos) {
        first_line = note.content().substr(0, end_of_line);
    } else {
        first_line = note.content();
    }
    
    // Remove leading/trailing whitespace
    auto start = first_line.find_first_not_of(" \t\r");
    if (start == std::string::npos) return false;
    auto end = first_line.find_last_not_of(" \t\r");
    first_line = first_line.substr(start, end - start + 1);
    
    // Check if it matches the title (with or without markdown heading)
    return first_line == stored_title || 
           first_line == "# " + stored_title ||
           first_line == "## " + stored_title;
}

int main(int argc, char* argv[]) {
    try {
        // Initialize the application
        nx::cli::Application app;
        auto init_result = app.initialize();
        if (!init_result.has_value()) {
            std::cerr << "Failed to initialize application: " << init_result.error().message() << std::endl;
            return 1;
        }
        
        std::cout << "=== Note Title Migration Tool ===" << std::endl;
        std::cout << "This will update notes to ensure their title matches the first line of content." << std::endl;
        
        // Get all notes
        auto note_ids_result = app.noteStore().getAllNoteIds();
        if (!note_ids_result.has_value()) {
            std::cerr << "Failed to get note IDs: " << note_ids_result.error().message() << std::endl;
            return 1;
        }
        
        auto note_ids = *note_ids_result;
        std::cout << "Found " << note_ids.size() << " notes to analyze..." << std::endl;
        
        std::vector<NoteId> notes_to_migrate;
        std::set<std::string> problematic_titles;
        
        // Analyze each note
        for (const auto& note_id : note_ids) {
            auto note_result = app.noteStore().load(note_id);
            if (!note_result.has_value()) {
                std::cerr << "Warning: Failed to load note " << note_id.toString() << std::endl;
                continue;
            }
            
            auto note = *note_result;
            std::string stored_title = note.metadata().title();
            std::string derived_title = note.title();  // This uses our new logic
            
            // Skip notebook marker notes (they start with .)
            if (stored_title.starts_with(".notebook")) {
                continue;
            }
            
            // Check if migration is needed
            bool needs_migration = false;
            std::string reason;
            
            if (stored_title.empty()) {
                needs_migration = true;
                reason = "empty stored title";
            } else if (stored_title.find("{{") != std::string::npos) {
                needs_migration = true;
                reason = "template variables in title";
            } else if (!contentStartsWithTitle(note, stored_title)) {
                needs_migration = true;
                reason = "first line doesn't match stored title";
            }
            
            if (needs_migration) {
                notes_to_migrate.push_back(note_id);
                std::cout << "  NEEDS MIGRATION: " << note_id.toString() << " - " << reason << std::endl;
                std::cout << "    Stored title: '" << stored_title << "'" << std::endl;
                std::cout << "    Derived title: '" << derived_title << "'" << std::endl;
                std::cout << "    Content preview: " << note.content().substr(0, 100) << "..." << std::endl;
                std::cout << std::endl;
            }
        }
        
        std::cout << "Analysis complete. " << notes_to_migrate.size() << " notes need migration." << std::endl;
        
        if (notes_to_migrate.empty()) {
            std::cout << "No migration needed!" << std::endl;
            return 0;
        }
        
        // Ask for confirmation
        std::cout << "Proceed with migration? (y/N): ";
        std::string response;
        std::getline(std::cin, response);
        
        if (response != "y" && response != "Y") {
            std::cout << "Migration cancelled." << std::endl;
            return 0;
        }
        
        // Perform migration
        std::cout << "Starting migration..." << std::endl;
        int migrated_count = 0;
        
        for (const auto& note_id : notes_to_migrate) {
            auto note_result = app.noteStore().load(note_id);
            if (!note_result.has_value()) {
                std::cerr << "Error loading note " << note_id.toString() << " during migration" << std::endl;
                continue;
            }
            
            auto note = *note_result;
            std::string stored_title = note.metadata().title();
            
            // Skip template notes - they need manual fixing
            if (stored_title.find("{{") != std::string::npos) {
                std::cout << "  SKIPPING template note: " << note_id.toString() << " (needs manual attention)" << std::endl;
                continue;
            }
            
            // Determine the new content
            std::string new_content;
            
            if (stored_title.empty()) {
                // For empty titles, keep content as-is (it will derive title from first line)
                new_content = note.content();
            } else {
                // Prepend the stored title as a markdown heading
                new_content = "# " + stored_title;
                if (!note.content().empty()) {
                    // If content doesn't start with newline, add one
                    if (note.content()[0] != '\n') {
                        new_content += "\n\n";
                    } else {
                        new_content += "\n";
                    }
                    new_content += note.content();
                }
            }
            
            // Update the note
            note.setContent(new_content);
            
            // Save the updated note
            auto save_result = app.noteStore().store(note);
            if (!save_result.has_value()) {
                std::cerr << "Error saving note " << note_id.toString() << ": " << save_result.error().message() << std::endl;
                continue;
            }
            
            // Update search index
            auto index_result = app.searchIndex().updateNote(note);
            if (!index_result.has_value()) {
                std::cerr << "Warning: Failed to update search index for " << note_id.toString() << std::endl;
            }
            
            migrated_count++;
            std::cout << "  MIGRATED: " << note_id.toString() << " - '" << stored_title << "'" << std::endl;
        }
        
        std::cout << "Migration complete! Successfully migrated " << migrated_count << " notes." << std::endl;
        
        // Show notes that still need manual attention
        if (migrated_count < notes_to_migrate.size()) {
            std::cout << "\nNotes requiring manual attention:" << std::endl;
            for (const auto& note_id : notes_to_migrate) {
                auto note_result = app.noteStore().load(note_id);
                if (note_result.has_value()) {
                    auto note = *note_result;
                    if (note.metadata().title().find("{{") != std::string::npos) {
                        std::cout << "  " << note_id.toString() << " - contains template variables" << std::endl;
                    }
                }
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Migration failed with error: " << e.what() << std::endl;
        return 1;
    }
}