#pragma once

#include "codegen/options.hpp"
#include "frontend/program.hpp"

#include <expected>
#include <string>

namespace hasten::codegen
{

class Generator
{
public:
    Generator(const frontend::Program& program, GenerationOptions options);

    std::expected<void, std::string> run();

private:
    const frontend::Program& _program;
    GenerationOptions _options;
};

}  // namespace hasten::codegen
