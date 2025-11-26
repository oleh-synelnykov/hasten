#pragma once

#include "idl/ast.hpp"
#include "idl/config.hpp"

#include <string>

namespace hasten::frontend
{

struct SourceFile {
    std::string path;                                 ///< Path to the file
    std::string content;                              ///< Whole content of the file
    idl::ast::Module module;                          ///< Parsed AST of the file
    idl::parser::position_cache_type position_cache;  ///< Position cache for the file
};

}  // namespace hasten::frontend