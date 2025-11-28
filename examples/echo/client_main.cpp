#include "sample/core/sample_core.gen.hpp"

#include <iostream>
#include <memory>

int main()
{
    using sample::core::Payload;

    Payload payload;
    payload.message = "Hello from client";

    std::cout << "Payload prepared with message: " << payload.message << '\n';
    std::cout << "TODO: instantiate hasten runtime context/channel and call EchoClient::Ping()\n";

    return 0;
}
