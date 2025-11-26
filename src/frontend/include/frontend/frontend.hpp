#pragma once

#include "frontend/program.hpp"

#include <expected>
#include <string>

namespace hasten::frontend
{

/**
 * @brief Parse a program from a root IDL file.
 *
 * @param root_path Path to the root IDL file.
 * @return std::expected<Program, std::string> Parsed program or error message.
 */
std::expected<Program, std::string> parse_program(const std::string& root_path);

namespace detail
{

/**
 * @brief Read a file into a string.
 *
 * @param path Path to the file.
 * @return File content or error message.
 */
std::expected<std::string, std::string> read_file(const std::string& path);

/**
 * @brief Parse a file content into an AST.
 *
 * @param maybe_file_content File content or error message.
 * @return Parsed file or error message.
 */
std::expected<SourceFile, std::string> parse_file_content(
    std::expected<std::string, std::string> maybe_file_content);

/**
 * @brief Parse a file into an AST.
 *
 * @param path Path to the file.
 * @return Parsed file or error message.
 */
std::expected<SourceFile, std::string> parse_single_file(const std::string& path);

/**
 * @brief Recursively parse all imports of a file.
 *
 * Passing all_imports by reference allows us to avoid copying the map and
 * check if a file has already been parsed.
 *
 * @param path Path to the file.
 * @param all_imports Map of all parsed imports.
 * @return All parsed imports or error message.
 */
std::expected<void, std::string> parse_imports(const std::string& path, Program::Files& all_imports);

}  // namespace detail

}  // namespace hasten::frontend
