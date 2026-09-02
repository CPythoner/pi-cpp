#include "version.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--version") {
            std::cout << "picpp v" << PICPP_VERSION << "\n";
            return 0;
        }
    }
    std::cerr << "usage: picpp [--version]\n";
    return 1;
}
