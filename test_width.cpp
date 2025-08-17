#include "include/nx/tui/unicode_handler.hpp"
#include <iostream>

int main() {
    auto init_result = nx::tui::UnicodeHandler::initialize();
    if (!init_result) {
        std::cout << "Failed to initialize Unicode handler" << std::endl;
        return 1;
    }
    
    size_t width = nx::tui::UnicodeHandler::calculateDisplayWidth("Hi世");
    std::cout << "Width of 'Hi世': " << width << std::endl;
    
    width = nx::tui::UnicodeHandler::calculateDisplayWidth("Hi");
    std::cout << "Width of 'Hi': " << width << std::endl;
    
    auto result = nx::tui::UnicodeHandler::truncateToWidth("Hi世界", 3, false);
    if (result) {
        std::cout << "Truncated result: '" << result.value() << "'" << std::endl;
    }
    
    nx::tui::UnicodeHandler::cleanup();
    return 0;
}
