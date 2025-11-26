#include "frontend/frontend.hpp"

#include "idl/parser.hpp"

#include <expected>
#include <filesystem>
#include <fstream>

namespace hasten::frontend
{

/**
 * @brief Parse a program from a root IDL file.
 *
 * @param root_path Path to the root IDL file.
 * @return std::expected<Program, std::string> Parsed program or error message.
 */
std::expected<Program, std::string> parse_program(const std::string& root_path)
{
    Program program;
    auto maybe = detail::parse_imports(root_path, program.files);
    if (!maybe) {
        return std::unexpected(maybe.error());
    }
    return program;
}

namespace detail
{

std::expected<std::string, std::string> read_file(const std::string& path)
{
    namespace fs = std::filesystem;

    const fs::path fs_path(path);

    std::error_code ec;
    if (!fs::is_regular_file(fs_path, ec) || ec) {
        return std::unexpected("Failed to open file: " + fs_path.string() + ": " + ec.message());
    }

    std::ifstream file(fs_path, std::ios::in);
    if (!file.is_open()) {
        return std::unexpected("Failed to open file: " + fs_path.string() + ": " + ec.message());
    }

    const auto file_size = fs::file_size(fs_path, ec);
    if (ec) {
        return std::unexpected("Failed to read file: " + fs_path.string() + ": " + ec.message());
    }

    std::string content(file_size, '\0');
    if (!file.read(content.data(), static_cast<std::streamsize>(content.size()))) {
        return std::unexpected("Failed to read file: " + fs_path.string() + ": " + ec.message());
    }

    return content;
}

std::expected<SourceFile, std::string> parse_file_content(
    std::expected<std::string, std::string> maybe_file_content)
{
    if (!maybe_file_content) {
        return std::unexpected(maybe_file_content.error());
    }

    if (auto result = idl::parser::parse_file(maybe_file_content.value())) {
        return SourceFile{.content = std::move(maybe_file_content.value()),
                          .module = std::move(result->module),
                          .position_cache = std::move(result->position_cache)};
    } else {
        return std::unexpected(result.error());
    }
}

std::expected<SourceFile, std::string> parse_single_file(const std::string& path)
{
    // parsing content doesn't set the path, so we need to do it after parsing is successful
    auto set_path = [&path](std::expected<SourceFile, std::string> maybe_source_file) {
        if (maybe_source_file) {
            maybe_source_file->path = path;
        }
        return maybe_source_file;
    };

    return read_file(path)
        .and_then(parse_file_content)
        .and_then(set_path);
}

std::expected<void, std::string> parse_imports(const std::string& path, Program::Files& all_imports)
{
    auto maybe_source_file = parse_single_file(path);
    if (!maybe_source_file) {
        return std::unexpected(maybe_source_file.error());
    }

    auto [iterator, inserted] = all_imports.try_emplace(path, std::move(maybe_source_file.value()));
    if (!inserted) {
        return std::unexpected("Duplicate import: " + path);
    }

    // Import paths are relative to the root file, so we need to extract the path to the root file first,
    // and then treat import paths relative to that.
    namespace fs = std::filesystem;
    fs::path root_path(path);
    root_path.remove_filename();

    const auto& imports = iterator->second.module.imports;
    for (const auto& import : imports) {
        if (!all_imports.contains((root_path / import.path).string())) {
            auto maybe = parse_imports((root_path / import.path).string(), all_imports);
            if (!maybe) {
                return std::unexpected(maybe.error());
            }
        }
    }

    return {};
}

}  // namespace detail

}  // namespace hasten::frontend
