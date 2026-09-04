// SPDX-License-Identifier: MIT

#include <iostream>
#include <string_view>

#ifndef JR800_PROJECT_VERSION
#error "JR800_PROJECT_VERSION must be defined"
#endif

#ifndef JR800_TOOL_NAME
#error "JR800_TOOL_NAME must be defined"
#endif

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << JR800_TOOL_NAME << ' ' << JR800_PROJECT_VERSION << '\n';
        return 0;
    }

    std::cerr << "Usage: " << JR800_TOOL_NAME << " --version\n";
    return 2;
}
