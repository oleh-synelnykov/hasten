#pragma once

#include "frontend/program.hpp"
#include "ir.hpp"

namespace hasten::codegen::ir
{

CompilationUnit build_internal_representation(const frontend::Program& program);

}  // namespace hasten::codegen::ir
