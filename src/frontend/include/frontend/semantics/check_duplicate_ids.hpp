#pragma once

#include "frontend/diagnostic.hpp"
#include "frontend/program.hpp"
#include "idl/ast.hpp"

#include <unordered_map>

namespace hasten::frontend::pass
{

namespace ast = idl::ast;

struct CheckDuplicateIds {
    Program& program;
    DiagnosticSink& sink;

    void operator()(const ast::Struct& s)
    {
        std::unordered_map<std::uint64_t, const ast::Field*> seen;
        for (const auto& f : s.fields) {
            auto [it, inserted] = seen.emplace(f.id, &f);
            if (!inserted) {
                Severity severity = Severity::Error;
                // Field is PositionTaggedNode, so can use f.id() for location... later.
                SourceLocation location{"<unknown>", 0, 0};
                std::string message =
                    "Duplicate field id '" + std::to_string(f.id) + "' in struct '" + s.name + "'";
                sink.report(severity, location, message);
            }
        }
    }

    void operator()(const ast::Method& m)
    {
        std::unordered_map<std::uint64_t, const ast::Parameter*> seen;
        for (const auto& p : m.params) {
            auto [it, inserted] = seen.emplace(p.id, &p);
            if (!inserted) {
                Severity severity = Severity::Error;
                // Parameter is PositionTaggedNode, so can use p.id() for location... later.
                SourceLocation location{"<unknown>", 0, 0};
                std::string message =
                    "Duplicate parameter id '" + std::to_string(p.id) + "' in method '" + m.name + "'";
                sink.report(severity, location, message);
            }
        }

        if (m.result.has_value()) {
            std::unordered_map<std::uint64_t, const ast::Field*> seen;
            if (const auto* fields = boost::get<std::vector<ast::Field>>(&m.result.value())) {
                for (const auto& f : *fields) {
                    auto [it, inserted] = seen.emplace(f.id, &f);
                    if (!inserted) {
                        Severity severity = Severity::Error;
                        // Field is PositionTaggedNode, so can use f.id() for location... later.
                        SourceLocation location{"<unknown>", 0, 0};
                        std::string message = "Duplicate result field id '" + std::to_string(f.id) +
                                              "' in method '" + m.name + "'";
                        sink.report(severity, location, message);
                    }
                }
            }
        }
    }

    template <typename T>
    void operator()(T&)
    {
        // do nothing
    }
};

}  // namespace hasten::frontend::pass
