#pragma once

#include <functional>

namespace hasten::runtime
{

class Executor
{
public:
    virtual ~Executor() = default;
    virtual void schedule(std::function<void()> fn) = 0;
};

}  // namespace hasten::runtime
