#pragma once

#include "options.hpp"

namespace hasten {

/**
 * @brief The entry point of Hasten.
 *
 * This function is the entry point of Hasten and is separated
 * from main() for testability.
 *
 * @param argc The number of command line arguments.
 * @param argv The command line arguments.
 * @return int The exit code.
 */
int run(int argc, char* argv[]);

}  // namespace hasten