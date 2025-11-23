#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "options.hpp"

int main(int argc, char* argv[])
{
    auto opts = parse_command_line(argc, argv);
    if (!opts) {
        spdlog::error("Failed to parse command line: {}", opts.error());
        return 1;
    }

    fmt::print("Hasten v{}.{}.{}\n", HASTEN_VERSION_MAJOR, HASTEN_VERSION_MINOR, HASTEN_VERSION_PATCH);
    return 0;
}
