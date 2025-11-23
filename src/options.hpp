#pragma once

#include <expected>
#include <optional>
#include <string>

struct Options {
    std::string input_file;
    std::optional<std::string> output_dir;  // if not specified, use the same directory as input_file
    bool check_only = false;                // if true, only check the input file for syntax errors
    bool print_ast = false;                 // if true, print the AST to stdout as JSON
    bool assign_uids = false;               // if true, assign unique IDs to all nodes
};

std::expected<Options, std::string> parse_command_line(int argc, char* argv[]);
