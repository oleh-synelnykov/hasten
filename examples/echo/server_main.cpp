#include "sample/core/sample_core.gen.hpp"

#include <iostream>
#include <memory>

#include "hasten/runtime/context.hpp"

namespace sample::core
{

class EchoImpl : public Echo
{
public:
    hasten::runtime::Result<EchoPingResult> Ping(const Payload& payload) override
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
    using namespace sample::core;

    hasten::runtime::ContextConfig cfg;
    cfg.managed_reactor = true;
    hasten::runtime::Context ctx{cfg};

    auto impl = std::make_shared<EchoImpl>();
    bind_Echo(impl);

    const std::string endpoint = "/tmp/hasten-echo.sock";
    auto listen_result = ctx.listen(endpoint);
    if (!listen_result) {
        std::cerr << "Failed to listen on " << endpoint << ": " << listen_result.error().message << '\n';
        return 1;
    }

    std::cout << "Echo server listening on " << endpoint << std::endl;
    ctx.start();
    ctx.join();
    return 0;
}
