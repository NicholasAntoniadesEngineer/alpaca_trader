#include "logging/logging_macros.hpp"
#include <iostream>
#include <string>

void log_message(const std::string& msg, const std::string& file) {
    std::cout << "LENGTH: " << msg.length() << " | MSG: [" << msg << "]" << std::endl;
}

int main() {
    LOG_SECTION_HEADER("TEST");
    return 0;
}
