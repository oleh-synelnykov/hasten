#include "sample/core/sample_core.gen.hpp"

#include <iostream>
#include <memory>

namespace sample::core
{

class EchoImpl : public Echo
{
public:
    hasten::runtime::result<EchoPingResult> Ping(const Payload& payload) override
    {
        std::cout << "Server received: " << payload.message << '\n';
        Payload reply = payload;
        reply.message = "Echo: " + payload.message;
        return EchoPingResult{reply};
    }
};

}  // namespace sample::core

int main()
{
    std::cout << "TODO: instantiate hasten runtime dispatcher and bind EchoImpl\n";
    auto impl = std::make_shared<sample::core::EchoImpl>();
    (void)impl;
    return 0;
}
