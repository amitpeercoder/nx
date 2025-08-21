#include "nx/tui/word_wrapper.hpp"
#include <iostream>

int main() {
    std::string test_line = "This is a very long line that should definitely wrap when word wrapping is enabled. It contains enough text to exceed the normal preview panel width and demonstrate the word wrapping functionality in action.";
    
    std::cout << "Original line length: " << test_line.length() << "\n";
    std::cout << "Original line: " << test_line << "\n\n";
    
    // Test with different widths
    for (size_t width : {40, 60, 80}) {
        std::cout << "Wrapping at width " << width << ":\n";
        auto wrapped = nx::tui::WordWrapper::wrapLine(test_line, width);
        
        for (size_t i = 0; i < wrapped.size(); ++i) {
            std::cout << "[" << i << "] " << wrapped[i] << " (len=" << wrapped[i].length() << ")\n";
        }
        std::cout << "\n";
    }
    
    return 0;
}