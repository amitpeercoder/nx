#include "nx/core/note.hpp"
#include <iostream>

int main() {
    // Test 1: Simple content
    auto note1 = nx::core::Note::create("", "Simple title test\n\nThis is content.");
    std::cout << "Test 1 - Title: '" << note1.title() << "'" << std::endl;
    
    // Test 2: Markdown heading
    auto note2 = nx::core::Note::create("", "# Markdown Heading\n\nContent below the heading.");
    std::cout << "Test 2 - Title: '" << note2.title() << "'" << std::endl;
    
    // Test 3: Change content and see title update
    auto note3 = nx::core::Note::create("", "Original First Line\n\nSome content.");
    std::cout << "Test 3a - Original Title: '" << note3.title() << "'" << std::endl;
    
    note3.setContent("## Changed First Line\n\nSome different content.");
    std::cout << "Test 3b - Changed Title: '" << note3.title() << "'" << std::endl;
    
    // Test 4: Empty content
    auto note4 = nx::core::Note::create("", "");
    std::cout << "Test 4 - Empty Title: '" << note4.title() << "'" << std::endl;
    
    return 0;
}
