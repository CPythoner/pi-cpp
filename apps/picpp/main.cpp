#include "version.h"

#include <fmt/format.h>

#include <iostream>
#include <string_view>

namespace {

void printHelp(std::ostream& out) {
    out << fmt::format("picpp v{}\n", PICPP_VERSION);
    out << "Usage:\n"
           "  picpp --version\n"
           "  picpp --help\n\n"
           "v0.0.2 exposes Provider/SSE runtime through the pi::ai SDK.\n"
           "Interactive agent CLI is planned for v0.1.0.\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) {
        printHelp(std::cout);
        return 0;
    }

    if (argc == 2) {
        const std::string_view argument{argv[1]};
        if (argument == "--version") {
            std::cout << fmt::format("picpp v{}\n", PICPP_VERSION);
            return 0;
        }
        if (argument == "--help" || argument == "-h") {
            printHelp(std::cout);
            return 0;
        }

        std::cerr << "unknown argument: " << argument << '\n';
        printHelp(std::cerr);
        return 2;
    }

    std::cerr << "unexpected arguments\n";
    printHelp(std::cerr);
    return 2;
}
