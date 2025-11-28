#pragma once

#include "rules.hpp"

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>

namespace hasten::idl::parser
{

namespace x3 = boost::spirit::x3;

using iterator_type = std::string::const_iterator;

using error_context_type = x3::error_handler<iterator_type>;
using position_cache_type = x3::position_cache<std::vector<iterator_type>>;

using skipper_context_type = x3::phrase_parse_context<rule::Skipper>::type;

// clang-format off
using context_type =
    x3::context<
        x3::error_handler_tag,
        std::reference_wrapper<error_context_type>,
        skipper_context_type>;
// clang-format on


}  // namespace hasten::idl::parser
