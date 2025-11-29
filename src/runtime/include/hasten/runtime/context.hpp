#pragma once

#include "hasten/runtime/result.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace hasten::runtime
{

class Channel;

struct ContextConfig {
    bool managed_reactor = true;
    std::size_t worker_threads = 0;
};

class Context
{
public:
    explicit Context(ContextConfig config = {});
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Result<void> listen(const std::string& path);
    Result<void> connect(const std::string& path);
    Result<void> attach_channel(std::shared_ptr<Channel> channel, bool server_side);

    void start();
    void stop();
    void join();

    std::size_t run();
    std::size_t run_one();
    std::size_t poll();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hasten::runtime
