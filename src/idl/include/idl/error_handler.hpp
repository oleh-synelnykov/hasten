#pragma once

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>

namespace hasten::idl::parser
{

namespace x3 = boost::spirit::x3;

struct GenericParsingErrorHandler {
    template <typename Iterator, typename Exception, typename Context>
    x3::error_handler_result on_error(Iterator& first, Iterator const& last, Exception const& e,
                                      Context const& context)
    {
        auto& error_handler = x3::get<x3::error_handler_tag>(context).get();
        std::string message = "Expected " + e.which() + " here:";
        error_handler(e.where(), message);
        return x3::error_handler_result::fail;
    }
};

}  // namespace hasten::idl::parser
