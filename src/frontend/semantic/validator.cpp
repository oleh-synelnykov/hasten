#include "frontend/semantic/validator.hpp"

#include "declaration_index_pass.hpp"
#include "enum_validation_pass.hpp"
#include "interface_validation_pass.hpp"
#include "module_index_pass.hpp"
#include "semantic_context.hpp"
#include "struct_validation_pass.hpp"

#include <utility>

namespace hasten::frontend::semantic
{

Validator::Validator(Program& program, DiagnosticSink& sink)
    : program_(program)
    , sink_(sink)
{
    register_default_passes();
}

void Validator::add_pass_factory(PassFactory factory)
{
    pass_factories_.push_back(std::move(factory));
}

void Validator::clear_passes()
{
    pass_factories_.clear();
}

void Validator::use_default_passes()
{
    clear_passes();
    register_default_passes();
}

std::vector<std::unique_ptr<Pass>> Validator::instantiate_passes() const
{
    std::vector<std::unique_ptr<Pass>> instances;
    instances.reserve(pass_factories_.size());
    for (const auto& factory : pass_factories_) {
        if (factory) {
            instances.push_back(factory());
        }
    }
    return instances;
}

void Validator::register_default_passes()
{
    add_pass<ModuleIndexPass>();
    add_pass<DeclarationIndexPass>();
    add_pass<EnumValidationPass>();
    add_pass<StructValidationPass>();
    add_pass<InterfaceValidationPass>();
}

void Validator::run()
{
    auto passes = instantiate_passes();
    Context context{program_, sink_};
    for (auto& pass : passes) {
        if (pass) {
            pass->run(context);
        }
    }
}

}  // namespace hasten::frontend::semantic
