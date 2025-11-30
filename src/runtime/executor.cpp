#include "hasten/runtime/executor.hpp"

#include "hasten/runtime/error.hpp"

#include <cstdio>

namespace hasten::runtime
{

ThreadPoolExecutor::ThreadPoolExecutor(std::size_t thread_count)
{
    if (thread_count == 0) {
        thread_count = 1;
    }
    workers_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPoolExecutor::~ThreadPoolExecutor()
{
    stop();
}

void ThreadPoolExecutor::stop()
{
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void ThreadPoolExecutor::schedule(std::function<void()> fn)
{
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            return;
        }
        tasks_.push(std::move(fn));
    }
    cv_.notify_one();
}

void ThreadPoolExecutor::worker_loop()
{
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            task();
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "hasten executor task threw exception: %s\n", ex.what());
        } catch (...) {
            std::fprintf(stderr, "hasten executor task threw unknown exception\n");
        }
    }
}

std::shared_ptr<Executor> make_default_executor(std::size_t thread_count)
{
    if (thread_count == 0) {
        thread_count = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    }
    return std::make_shared<ThreadPoolExecutor>(thread_count);
}

}  // namespace hasten::runtime
