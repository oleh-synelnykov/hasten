#include "semantic_context.hpp"

namespace hasten::frontend::semantic
{

Context::Context(Program& program, DiagnosticSink& sink)
    : program_(program)
    , sink_(sink)
{
}

Program& Context::program()
{
    return program_;
}

const Program& Context::program() const
{
    return program_;
}

DiagnosticSink& Context::diagnostics()
{
    return sink_;
}

Context::ModuleIndex& Context::module_index()
{
    return module_index_;
}

const Context::ModuleIndex& Context::module_index() const
{
    return module_index_;
}

Context::DeclarationIndex& Context::declaration_index()
{
    return declarations_;
}

const Context::DeclarationIndex& Context::declaration_index() const
{
    return declarations_;
}

const DeclInfo* Context::resolve_user_type(const ast::UserType& user_type,
                                                   const std::string& module_name, const SourceFile& file,
                                                   const std::string& usage) const
{
    std::string name = user_type.name.to_string();
    auto it = declarations_.find(name);
    if (it == declarations_.end() && user_type.name.parts.size() == 1) {
        auto fq = qualified_name(module_name, name);
        it = declarations_.find(fq);
    }
    if (it == declarations_.end()) {
        report_error(file, user_type, "Unknown type '" + name + "' referenced in " + usage);
        return nullptr;
    }
    return &it->second;
}

void Context::report(Severity severity, const SourceFile& file, const ast::PositionTaggedNode& node,
                             const std::string& message) const
{
    sink_.report(severity, locate(node, file), message);
}

void Context::report_error(const SourceFile& file, const ast::PositionTaggedNode& node,
                                   const std::string& message) const
{
    report(Severity::Error, file, node, message);
}

void Context::report_warning(const SourceFile& file, const ast::PositionTaggedNode& node,
                                     const std::string& message) const
{
    report(Severity::Warning, file, node, message);
}

void Context::report_note(const SourceFile& file, const ast::PositionTaggedNode& node,
                                  const std::string& message) const
{
    report(Severity::Note, file, node, message);
}

std::string Context::qualified_name(const std::string& module_name,
                                            const std::string& decl_name) const
{
    if (module_name.empty()) {
        return decl_name;
    }
    return module_name + "." + decl_name;
}

}  // namespace hasten::frontend::semantic
