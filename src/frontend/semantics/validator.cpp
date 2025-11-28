#include "frontend/semantics/validator.hpp"

#include "validator_impl.hpp"

namespace hasten::frontend
{

SemanticsValidator::SemanticsValidator(Program& program, DiagnosticSink& sink)
    : program_(program)
    , sink_(sink)
{
}

void SemanticsValidator::run()
{
    ValidateSemanticsImpl state{program_, sink_};
    state.run();
}

}  // namespace hasten::frontend
