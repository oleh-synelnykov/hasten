#include "module_index_pass.hpp"

#include "semantic_context.hpp"

namespace hasten::frontend::semantic
{

void ModuleIndexPass::run(Context& context)
{
    auto& index = context.module_index();
    index.clear();

    for (const auto& [_, file] : context.program().files) {
        const auto& module = file.module;
        auto module_name = module.name.to_string();
        auto [it, inserted] = index.emplace(module_name, &file);
        if (!inserted) {
            context.report_error(file, module,
                                 "Module '" + module_name + "' already defined in " + it->second->path);
        }
    }
}

}  // namespace hasten::frontend::semantic
