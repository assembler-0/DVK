#pragma once
#include <iostream>

namespace input {
    // Helper to get user input safely
    inline std::string getInput() {
        std::string input;
        std::getline(std::cin, input);
        if (std::cin.eof()) {
            throw std::runtime_error("Input stream closed.");
        }
        // Basic trim
        const size_t first = input.find_first_not_of(" \t\n\r");
        if (std::string::npos == first) return "";
        const size_t last = input.find_last_not_of(" \t\n\r");
        return input.substr(first, last - first + 1);
    }
}