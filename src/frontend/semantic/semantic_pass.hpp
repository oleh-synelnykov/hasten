#pragma once

#include <string>

namespace hasten::frontend::semantic {

class Context;

class Pass
{
public:
    virtual ~Pass() = default;
    virtual std::string name() const = 0;
    virtual void run(Context& context) = 0;
};

} // namespace hasten::frontend::semantic