#include "file_writer.hpp"

#include <fstream>
#include <iterator>

namespace hasten::codegen
{

std::expected<void, std::string> write_file_if_changed(const std::filesystem::path& path,
                                                       std::expected<std::string, std::string> maybe_content)
{
    if (!maybe_content) {
        return std::unexpected(maybe_content.error());
    }

    std::string existing;
    if (std::filesystem::exists(path)) {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (!in) {
            return std::unexpected("Failed to read existing file '" + path.string() + "'");
        }
        existing.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    if (existing == maybe_content.value()) {
        return {};
    }

    std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected("Failed to write file '" + path.string() + "'");
    }
    out.write(maybe_content->data(), static_cast<std::streamsize>(maybe_content->size()));
    return {};
}

}  // namespace hasten::codegen
