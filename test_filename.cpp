#include "nx/core/note.hpp"
#include <iostream>
int main() {
    auto note = nx::core::Note::create("Test Note Title", "Content");
    std::cout << "Title: " << note.title() << std::endl;
    std::cout << "Filename: " << note.filename() << std::endl;
    
    auto note2 = nx::core::Note::create("Special!@# Characters & Spaces", "Special!@# Characters & Spaces Content");
    std::cout << "Title2: " << note2.title() << std::endl;
    std::cout << "Filename2: " << note2.filename() << std::endl;
    return 0;
}
