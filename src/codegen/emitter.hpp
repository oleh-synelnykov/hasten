#pragma once

#include "codegen/options.hpp"
#include "ir.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace hasten::codegen
{

class Emitter
{
public:
    struct InterfaceArtifacts {
        std::string interface_name;
        std::filesystem::path client_source;
        std::filesystem::path server_source;
    };

    struct OutputFiles {
        std::filesystem::path header;
        std::filesystem::path include_dir;
        std::string module_base;
        std::vector<InterfaceArtifacts> interfaces;
    };

    Emitter(const GenerationOptions& options, const std::filesystem::path& root);

    std::expected<OutputFiles, std::string> emit_module(const ir::Module& module) const;

private:
    const GenerationOptions& _options;
    std::filesystem::path _root;
};

}  // namespace hasten::codegen
