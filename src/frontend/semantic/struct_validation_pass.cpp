#include "struct_validation_pass.hpp"

#include "semantic_context.hpp"
#include "type_validator.hpp"
#include "utility.hpp"

#include <boost/variant/get.hpp>

namespace hasten::frontend::semantic
{

void StructValidationPass::run(Context& context)
{
    TypeValidator type_validator(context);
    for (const auto& [_, file] : context.program().files) {
        const auto& module = file.module;
        auto module_name = module.name.to_string();
        for (const auto& decl : module.decls) {
            if (const auto* s = boost::get<ast::Struct>(&decl)) {
                const auto owner = "struct '" + s->name + "'";
                check_unique_names(context, s->fields, file, owner, "field");
                check_id_collection(context, s->fields, file, owner, "field");
                for (const auto& field : s->fields) {
                    type_validator.validate(field.type, file, field, module_name,
                                            "field '" + field.name + "' of struct '" + s->name + "'");
                }
            }
        }
    }
}

}  // namespace hasten::frontend::semantic
