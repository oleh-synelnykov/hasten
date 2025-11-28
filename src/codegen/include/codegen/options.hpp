#pragma once

#include <filesystem>

namespace hasten::codegen
{

struct GenerationOptions {
    std::filesystem::path output_dir;
};

}  // namespace hasten::codegen
