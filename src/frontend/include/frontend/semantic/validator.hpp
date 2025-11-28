#pragma once

#include "frontend/diagnostic.hpp"
#include "frontend/program.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace hasten::frontend::semantic
{

class Pass;

class Validator
{
public:
    using PassFactory = std::function<std::unique_ptr<Pass>()>;

    Validator(Program& program, DiagnosticSink& sink);

    template <typename Pass>
    void add_pass()
    {
        add_pass_factory([] {
            return std::make_unique<Pass>();
        });
    }

    void add_pass_factory(PassFactory factory);
    void clear_passes();
    void use_default_passes();
    void run();

private:
    void register_default_passes();
    std::vector<std::unique_ptr<Pass>> instantiate_passes() const;

    Program& program_;
    DiagnosticSink& sink_;
    std::vector<PassFactory> pass_factories_;
};

}  // namespace hasten::frontend::semantic
