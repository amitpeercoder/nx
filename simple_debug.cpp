#include <iostream>
#include <string>
#include <vector>

// Simple version without ICU for testing
size_t calculateDisplayWidth(const std::string& text) {
    return text.length(); // Simple counting for test
}

std::vector<std::string> simpleWrapLine(const std::string& line, size_t max_width) {
    std::vector<std::string> wrapped;
    
    if (line.empty() || max_width < 2) {
        wrapped.push_back(line);
        return wrapped;
    }
    
    std::string remaining = line;
    while (!remaining.empty()) {
        if (remaining.length() <= max_width) {
            wrapped.push_back(remaining);
            break;
        }
        
        // Find last space within max_width
        size_t break_point = max_width;
        for (size_t i = max_width; i > 0; --i) {
            if (remaining[i] == ' ') {
                break_point = i;
                break;
            }
        }
        
        wrapped.push_back(remaining.substr(0, break_point));
        remaining = remaining.substr(break_point);
        
        // Skip leading spaces
        while (!remaining.empty() && remaining[0] == ' ') {
            remaining.erase(0, 1);
        }
    }
    
    return wrapped;
}

int main() {
    std::string test_line = "This is a very long line that should definitely wrap when word wrapping is enabled. It contains enough text to exceed the normal preview panel width and demonstrate the word wrapping functionality in action.";
    
    std::cout << "Original line length: " << test_line.length() << "\n";
    std::cout << "Original line: " << test_line << "\n\n";
    
    // Test with different widths
    for (size_t width : {40, 60, 80}) {
        std::cout << "Wrapping at width " << width << ":\n";
        auto wrapped = simpleWrapLine(test_line, width);
        
        for (size_t i = 0; i < wrapped.size(); ++i) {
            std::cout << "[" << i << "] " << wrapped[i] << " (len=" << wrapped[i].length() << ")\n";
        }
        std::cout << "\n";
    }
    
    return 0;
}