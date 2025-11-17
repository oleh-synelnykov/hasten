#pragma once

#include "ast.hpp"

namespace hasten::idl::parser
{

bool parse_file(std::string_view input, ast::Module& out, std::string* error = nullptr);

}  // namespace hasten::idl::parser
