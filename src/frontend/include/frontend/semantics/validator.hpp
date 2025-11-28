#pragma once

#include "frontend/diagnostic.hpp"
#include "frontend/program.hpp"

namespace hasten::frontend
{

class SemanticsValidator
{
public:
    SemanticsValidator(Program& program, DiagnosticSink& sink);

    void run();

private:
    Program& program_;
    DiagnosticSink& sink_;
};

}  // namespace hasten::frontend
