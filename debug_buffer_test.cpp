#include "nx/tui/editor_buffer.hpp"
#include <iostream>

using namespace nx::tui;
using namespace nx;

int main() {
    EditorBuffer::Config config;
    config.gap_config.initial_gap_size = 64;
    config.gap_config.max_buffer_size = 1024 * 1024;
    EditorBuffer buffer(config);
    
    auto init_result = buffer.initialize("Hello\nWorld\nTest");
    if (!init_result) {
        std::cout << "Initialize failed: " << init_result.error().message() << std::endl;
        return 1;
    }
    
    std::cout << "=== Initial State ===" << std::endl;
    std::cout << "Line count: " << buffer.getLineCount() << std::endl;
    for (size_t i = 0; i < buffer.getLineCount(); ++i) {
        auto line = buffer.getLine(i);
        if (line) {
            std::cout << "Line " << i << ": '" << line.value() << "' (length: " << line.value().length() << ")" << std::endl;
        }
    }
    
    std::cout << "\n=== Testing insertChar(0, 5, ' ') ===" << std::endl;
    auto insert_result = buffer.insertChar(0, 5, ' ');
    if (!insert_result) {
        std::cout << "insertChar failed: " << insert_result.error().message() << std::endl;
    } else {
        std::cout << "insertChar succeeded" << std::endl;
    }
    
    std::cout << "\n=== After Insert ===" << std::endl;
    for (size_t i = 0; i < buffer.getLineCount(); ++i) {
        auto line = buffer.getLine(i);
        if (line) {
            std::cout << "Line " << i << ": '" << line.value() << "' (length: " << line.value().length() << ")" << std::endl;
        }
    }
    
    return 0;
}