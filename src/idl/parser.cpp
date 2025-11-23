#include "idl/parser.hpp"

#include "idl/ast.hpp"
#include "idl/config.hpp"
#include "idl/rules.hpp"

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>


#include <iostream>

namespace hasten::idl::parser
{

bool parse_file(const std::string& input, ast::Module& out, std::string* error)
{
    auto iter = input.begin();
    auto end = input.end();

    // error handling
    using error_handler_t = x3::error_handler<hasten::idl::parser::iterator_type>;
    error_handler_t err_handler(iter, end, std::cerr);

    auto const with_err = x3::with<x3::error_handler_tag>(std::ref(err_handler))[ module() ];

    ast::Module m;
    bool ok = x3::phrase_parse(iter, end, with_err, skipper(), m);

    if (!ok) {
        if (error) {
            (*error) = "parse error near: ";
            auto rem = std::string(iter, end);
            if (rem.size() > 64)
                rem.resize(64);
            (*error) += rem;
        }
        return false;
    }
    out.name = std::move(m.name);
    out.imports = std::move(m.imports);
    out.decls = std::move(m.decls);
    return true;
}

}  // namespace hasten::idl::parser
