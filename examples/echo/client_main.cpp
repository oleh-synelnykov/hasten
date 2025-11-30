#include "sample/core/sample_core.gen.hpp"

#include <iostream>
#include <memory>

#include "hasten/runtime/context.hpp"
#include "hasten/runtime/uds.hpp"

int main()
{
    using namespace sample::core;

    hasten::runtime::ContextConfig cfg;
    cfg.managed_reactor = true;
    hasten::runtime::Context ctx{cfg};

    auto dispatcher = ctx.get_dispatcher();
    if (!dispatcher) {
        std::cerr << "client: dispatcher unavailable\n";
        return 1;
    }

    const std::string endpoint = "/tmp/hasten-echo.sock";
    auto channel = hasten::runtime::uds::connect(endpoint);
    if (!channel) {
        std::cerr << "Failed to connect to " << endpoint << ": " << channel.error().message << '\n';
        return 1;
    }

    auto attach = ctx.attach_channel(channel.value(), false);
    if (!attach) {
        std::cerr << "Failed to attach channel: " << attach.error().message << '\n';
        return 1;
    }

    ctx.start();

    auto client = make_Echo_client(channel.value(), dispatcher);

    Payload payload;
    payload.message = "Hello from client";

    auto result = client->Ping_sync(payload);
    if (!result) {
        std::cerr << "RPC failed: " << result.error().message << '\n';
        ctx.stop();
        ctx.join();
        return 1;
    }

    std::cout << "Server replied: " << result->reply.message << '\n';

    ctx.stop();
    ctx.join();
    return 0;
}
