#include "hasten/runtime/executor.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <future>

using namespace hasten::runtime;

TEST(InlineExecutorTest, RunsTaskImmediately)
{
    InlineExecutor exec;
    std::atomic<int> counter{0};
    exec.schedule([&] { counter.fetch_add(1, std::memory_order_relaxed); });
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 1);
}

TEST(ThreadPoolExecutorTest, ExecutesScheduledTasks)
{
    ThreadPoolExecutor exec(2);
    std::atomic<int> counter{0};
    std::promise<void> done;
    auto fut = done.get_future();

    for (int i = 0; i < 5; ++i) {
        exec.schedule([&] {
            if (counter.fetch_add(1, std::memory_order_relaxed) == 4) {
                done.set_value();
            }
        });
    }

    EXPECT_EQ(fut.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    exec.stop();
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 5);
}
