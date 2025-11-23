#include "idl/parser.hpp"

#include "idl/ast.hpp"
#include "idl/config.hpp"
#include "idl/rules.hpp"

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>

#include <iostream>
#include <sstream>

namespace hasten::idl::parser
{

bool parse_file(const std::string& input, ast::Module& out, std::string* error)
{
    auto iter = input.begin();
    auto end = input.end();

    std::ostringstream diag_stream;

    // Create a position cache for this buffer
    position_cache_type positions(iter, end);

    // error handling
    using error_handler_t = x3::error_handler<hasten::idl::parser::iterator_type>;
    error_handler_t err_handler(iter, end, diag_stream);

    // clang-format off
    const auto parser =
        x3::with<x3::error_handler_tag>(std::ref(err_handler))[
            x3::with<position_cache_tag>(std::ref(positions))[
                module()
            ]
        ];
    // clang-format on

    ast::Module m;
    bool ok = x3::phrase_parse(iter, end, parser, skipper(), m);

    if (!ok || iter != end) {
        auto diagnostic = diag_stream.str();
        if (error) {
            if (!diagnostic.empty()) {
                (*error) = std::move(diagnostic);
            } else {
                (*error) = "Parse error near: ";
                auto rem = std::string(iter, end);
                if (rem.size() > 64)
                    rem.resize(64);
                (*error) += rem;
            }
        } else if (!diagnostic.empty()) {
            std::cerr << diagnostic;
        }
        return false;
    }
    out = std::move(m);
    return true;
}

}  // namespace hasten::idl::parser
