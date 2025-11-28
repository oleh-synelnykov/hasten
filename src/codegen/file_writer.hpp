#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace hasten::codegen
{

std::expected<void, std::string> write_file_if_changed(const std::filesystem::path& path,
                                                       std::expected<std::string, std::string> content);

}  // namespace hasten::codegen
