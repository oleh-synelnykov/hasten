#pragma once

#include "hasten/runtime/result.hpp"
#include "hasten/runtime/executor.hpp"

#include <memory>
#include <cstddef>
#include <string>

namespace hasten::runtime
{

class Channel;
class Dispatcher;

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
    void set_executor(std::shared_ptr<Executor> exec);
    std::shared_ptr<Dispatcher> get_dispatcher() const;

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
