#pragma once

#include "idl/ast.hpp"

#include <nlohmann/json_fwd.hpp>

namespace hasten::idl::ast
{

/**
 * @brief Produce a JSON representation of an AST module.
 *
 * This is primarily intended for diagnostics / debugging output where the
 * complete parse tree is required (e.g., --print-ast CLI flag).
 */
nlohmann::json to_json(const Module& module);

}  // namespace hasten::idl::ast
