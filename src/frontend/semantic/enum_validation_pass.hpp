#pragma once

#include "semantic_pass.hpp"

namespace hasten::frontend::semantic
{

class EnumValidationPass : public Pass
{
public:
    std::string name() const override
    {
        return "enum-validation";
    }

    void run(Context& context) override;
};

}  // namespace hasten::frontend::semantic
