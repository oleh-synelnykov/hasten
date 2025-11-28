#pragma once

#include "semantic_pass.hpp"

namespace hasten::frontend::semantic
{

class StructValidationPass : public Pass
{
public:
    std::string name() const override
    {
        return "struct-validation";
    }

    void run(Context& context) override;
};

}  // namespace hasten::frontend::semantic
