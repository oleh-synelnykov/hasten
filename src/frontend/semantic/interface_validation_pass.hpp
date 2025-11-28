#pragma once

#include "semantic_pass.hpp"

namespace hasten::frontend::semantic
{

class InterfaceValidationPass : public Pass
{
public:
    std::string name() const override
    {
        return "interface-validation";
    }

    void run(Context& context) override;
};

}  // namespace hasten::frontend::semantic
