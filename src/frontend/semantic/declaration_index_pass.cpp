#include "declaration_index_pass.hpp"

#include "semantic_context.hpp"

#include <boost/variant/get.hpp>

namespace hasten::frontend::semantic
{

namespace
{

template <typename Node>
void register_declaration(Context& context, const std::string& module_name, const SourceFile& file,
                          const Node& node, DeclKind kind)
{
    std::string fq = context.qualified_name(module_name, node.name);
    auto [it, inserted] = context.declaration_index().emplace(fq, DeclInfo{kind, &file});
    if (!inserted) {
        context.report_error(file, node,
                             "Declaration '" + fq + "' already defined in " + it->second.file->path);
    }
}

}  // namespace

void DeclarationIndexPass::run(Context& context)
{
    auto& decls = context.declaration_index();
    decls.clear();

    for (const auto& [_, file] : context.program().files) {
        const auto& module = file.module;
        auto module_name = module.name.to_string();

        for (const auto& decl : module.decls) {
            if (const auto* s = boost::get<ast::Struct>(&decl)) {
                register_declaration(context, module_name, file, *s, DeclKind::Struct);
            } else if (const auto* e = boost::get<ast::Enum>(&decl)) {
                register_declaration(context, module_name, file, *e, DeclKind::Enum);
            } else if (const auto* i = boost::get<ast::Interface>(&decl)) {
                register_declaration(context, module_name, file, *i, DeclKind::Interface);
            }
        }
    }
}

}  // namespace hasten::frontend::semantic
