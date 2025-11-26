#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "cli/hasten.hpp"

int main(int argc, char* argv[])
{
    try {
        return hasten::run(argc, argv);
    } catch (const std::exception& e) {
        spdlog::error("{}", e.what());
        return 1;
    }
}
