#include "enum_validation_pass.hpp"

#include "semantic_context.hpp"
#include "utility.hpp"

#include <boost/variant/get.hpp>

namespace hasten::frontend::semantic
{

void EnumValidationPass::run(Context& context)
{
    for (const auto& [_, file] : context.program().files) {
        const auto& module = file.module;
        for (const auto& decl : module.decls) {
            if (const auto* e = boost::get<ast::Enum>(&decl)) {
                check_unique_names(context, e->items, file, "enum '" + e->name + "'", "enumerator");
            }
        }
    }
}

}  // namespace hasten::frontend::semantic
