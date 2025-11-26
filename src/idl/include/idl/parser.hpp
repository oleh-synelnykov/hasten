#pragma once

#include "ast.hpp"
#include "idl/config.hpp"

#include <expected>

namespace hasten::idl::parser
{

struct ParseResult {
    ast::Module module;
    position_cache_type position_cache;
};

std::expected<ParseResult, std::string> parse_file(const std::string& input);

}  // namespace hasten::idl::parser
