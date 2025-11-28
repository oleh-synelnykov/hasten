#pragma once

#include "frontend/diagnostic.hpp"
#include "frontend/program.hpp"
#include "idl/ast.hpp"

#include <unordered_map>

namespace hasten::frontend::semantic {

namespace ast = idl::ast;

enum class DeclKind { Struct, Enum, Interface };

struct DeclInfo {
    DeclKind kind;
    const SourceFile* file = nullptr;
};

class Context
{
public:
    using ModuleIndex = std::unordered_map<std::string, const SourceFile*>;
    using DeclarationIndex = std::unordered_map<std::string, DeclInfo>;

    Context(Program& program, DiagnosticSink& sink);

    Program& program();
    const Program& program() const;

    DiagnosticSink& diagnostics();

    ModuleIndex& module_index();
    const ModuleIndex& module_index() const;

    DeclarationIndex& declaration_index();
    const DeclarationIndex& declaration_index() const;

    const DeclInfo* resolve_user_type(const ast::UserType& user_type, const std::string& module_name,
                                      const SourceFile& file, const std::string& usage) const;

     std::string qualified_name(const std::string& module_name, const std::string& decl_name) const;

    void report_error(const SourceFile& file, const ast::PositionTaggedNode& node,
                      const std::string& message) const;
    void report_warning(const SourceFile& file, const ast::PositionTaggedNode& node,
                        const std::string& message) const;
    void report_note(const SourceFile& file, const ast::PositionTaggedNode& node,
                     const std::string& message) const;
private:

    void report(Severity severity, const SourceFile& file, const ast::PositionTaggedNode& node,
                const std::string& message) const;

    Program& program_;
    DiagnosticSink& sink_;
    ModuleIndex module_index_;
    DeclarationIndex declarations_;
};

}  // namespace hasten::frontend::semantic
