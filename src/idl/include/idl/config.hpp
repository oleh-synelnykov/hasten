#pragma once

#include "rules_definition.hpp"

#include <boost/spirit/home/x3.hpp>

namespace hasten::idl::parser
{

namespace x3 = boost::spirit::x3;

using iterator_type = std::string::const_iterator;
using context_type = x3::phrase_parse_context<decltype(rule::skipper)>::type;

}  // namespace hasten::idl::parser
