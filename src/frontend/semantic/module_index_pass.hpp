#pragma once

#include "semantic_pass.hpp"

namespace hasten::frontend::semantic
{

class ModuleIndexPass : public Pass
{
public:
    std::string name() const override
    {
        return "module-index";
    }

    void run(Context& context) override;
};

}  // namespace hasten::frontend::semantic
