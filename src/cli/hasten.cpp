#include "cli/hasten.hpp"
#include "cli/options.hpp"

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

namespace hasten
{

int run(int argc, char* argv[])
{
    auto opts = parse_command_line(argc, argv);
    if (!opts) {
        spdlog::error("Failed to parse command line: {}", opts.error());
        return 1;
    }

    if (opts->help_message) {
        fmt::print("{}", opts->help_message.value());
        return 0;
    }

    fmt::print("Hasten v{}.{}.{}\n", HASTEN_VERSION_MAJOR, HASTEN_VERSION_MINOR, HASTEN_VERSION_PATCH);
    return 0;
}

}  // namespace hasten