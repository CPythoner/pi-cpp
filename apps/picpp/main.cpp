#include "version.h"

#include <fmt/format.h>

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--version") {
            std::cout << fmt::format("picpp v{}\n", PICPP_VERSION);
            return 0;
        }
    }
    std::cerr << "usage: picpp [--version]\n";
    return 1;
}
