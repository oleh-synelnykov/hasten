#include "interface_validation_pass.hpp"

#include "semantic_context.hpp"
#include "type_validator.hpp"
#include "utility.hpp"

#include <boost/variant/get.hpp>

namespace hasten::frontend::semantic
{

void InterfaceValidationPass::run(Context& context)
{
    TypeValidator type_validator(context);
    for (const auto& [_, file] : context.program().files) {
        const auto& module = file.module;
        auto module_name = module.name.to_string();
        for (const auto& decl : module.decls) {
            if (const auto* iface = boost::get<ast::Interface>(&decl)) {
                const auto interface_owner = "interface '" + iface->name + "'";
                check_unique_names(context, iface->methods, file, interface_owner, "method");

                for (const auto& method : iface->methods) {
                    const auto method_owner = "method '" + method.name + "'";
                    check_unique_names(context, method.params, file, method_owner, "parameter");
                    check_id_collection(context, method.params, file, method_owner, "parameter");
                    for (const auto& param : method.params) {
                        type_validator.validate(
                            param.type, file, param, module_name,
                            "parameter '" + param.name + "' of method '" + method.name + "'");
                    }

                    if (method.result) {
                        if (const auto* result_fields =
                                boost::get<std::vector<ast::Field>>(&*method.result)) {
                            check_unique_names(context, *result_fields, file, method_owner + " result",
                                               "field");
                            check_id_collection(context, *result_fields, file, method_owner, "result field");
                            for (const auto& field : *result_fields) {
                                type_validator.validate(
                                    field.type, file, field, module_name,
                                    "result field '" + field.name + "' of method '" + method.name + "'");
                            }
                        } else if (const auto* result_type = boost::get<ast::Type>(&*method.result)) {
                            type_validator.validate(*result_type, file, method, module_name,
                                                    "result of method '" + method.name + "'");
                        }
                    }
                }
            }
        }
    }
}

}  // namespace hasten::frontend::semantic
