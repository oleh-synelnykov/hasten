#pragma once

#include "rules.hpp"

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>

namespace hasten::idl::parser
{

namespace x3 = boost::spirit::x3;

using iterator_type = std::string::const_iterator;

using error_handler_type = x3::error_handler<iterator_type>;

using skipper_context_type = x3::phrase_parse_context<rule::Skipper>::type;

using context_type =
    x3::context< x3::error_handler_tag, std::reference_wrapper<error_handler_type>, skipper_context_type>;

}  // namespace hasten::idl::parser
