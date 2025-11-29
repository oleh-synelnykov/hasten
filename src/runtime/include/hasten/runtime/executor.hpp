#pragma once

#include <functional>
#include <condition_variable>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

namespace hasten::runtime
{

class Executor
{
public:
    virtual ~Executor() = default;
    virtual void schedule(std::function<void()> fn) = 0;
};

class InlineExecutor : public Executor
{
public:
    void schedule(std::function<void()> fn) override
    {
        fn();
    }
};

class ThreadPoolExecutor : public Executor
{
public:
    explicit ThreadPoolExecutor(std::size_t thread_count);
    ~ThreadPoolExecutor() override;

    void schedule(std::function<void()> fn) override;
    void stop();

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

std::shared_ptr<Executor> make_default_executor();

}  // namespace hasten::runtime
