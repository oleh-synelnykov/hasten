#pragma once

#include "semantic_pass.hpp"

namespace hasten::frontend::semantic {

class DeclarationIndexPass : public Pass
{
public:
    std::string name() const override
    {
        return "declaration-index";
    }

    void run(Context& context) override;
};

}  // namespace hasten::frontend::semantic
