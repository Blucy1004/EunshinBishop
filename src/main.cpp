#include "core/version.h"
#include "uci/uci.h"

#include <iostream>
#include <string_view>

namespace {

void printHelp() {
    std::cout << Eunshin::Version::idString() << "\n\n"
              << "Usage: EunshinBishop [--version] [--help]\n\n"
              << "With no arguments, EunshinBishop speaks the UCI protocol on\n"
              << "stdin/stdout; run it from a UCI-compatible GUI, or type UCI\n"
              << "commands directly (uci, isready, position, go, stop, quit).\n\n"
              << "  --version   print the engine identity string and exit\n"
              << "  --help      print this message and exit\n";
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--version") {
            std::cout << Eunshin::Version::idString() << "\n";
            return 0;
        }
        if (arg == "--help") {
            printHelp();
            return 0;
        }
    }

    return Eunshin::UCI::run(std::cin, std::cout);
}
