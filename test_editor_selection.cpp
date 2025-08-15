#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

int main() {
    const std::vector<std::string> fallback_editors = {"nano", "micro", "nvim", "vim", "vi"};
    std::string editor;
    
    for (const auto& candidate : fallback_editors) {
        std::cout << "Testing: " << candidate << std::endl;
        if (system(("which " + candidate + " > /dev/null 2>&1").c_str()) == 0) {
            editor = candidate;
            std::cout << "Found: " << candidate << std::endl;
            break;
        } else {
            std::cout << "Not found: " << candidate << std::endl;
        }
    }
    
    std::cout << "Selected editor: " << editor << std::endl;
    return 0;
}