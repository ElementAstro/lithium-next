#ifndef TEST_EVENT_LOOP_HPP
#define TEST_EVENT_LOOP_HPP

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "server/eventloop.hpp"

using namespace lithium::app;
using namespace std::chrono_literals;

class EventLoopTest : public ::testing::Test {
protected:
    void SetUp() override {
        loop = std::make_unique<EventLoop>(2);  // Create with 2 worker threads
    }

    void TearDown() override {
        loop->stop();
        loop.reset();
    }

    std::unique_ptr<EventLoop> loop;
};

TEST_F(EventLoopTest, BasicConstruction) {
    EXPECT_NO_THROW(EventLoop(1));
    EXPECT_NO_THROW(EventLoop(4));
}

TEST_F(EventLoopTest, RunAndStop) {
    std::atomic<bool> running{false};

    loop->run();
    loop->post([&running]() { running = true; });

    // Wait for task execution
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(running);

    loop->stop();
}

TEST_F(EventLoopTest, TaskPosting) {
    std::atomic<int> counter{0};

    loop->run();
    auto future = loop->post([&counter]() {
        counter++;
        return 42;
    });

    EXPECT_EQ(future.get(), 42);
    EXPECT_EQ(counter, 1);
}

TEST_F(EventLoopTest, PriorityTaskPosting) {
    std::vector<int> execution_order;
    std::mutex mutex;

    loop->run();

    // Post tasks with different priorities
    loop->post(3, [&]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(3);
    });

    loop->post(1, [&]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(1);
    });

    loop->post(2, [&]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(2);
    });

    std::this_thread::sleep_for(200ms);

    EXPECT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[0], 1);
    EXPECT_EQ(execution_order[1], 2);
    EXPECT_EQ(execution_order[2], 3);
}

TEST_F(EventLoopTest, DelayedTaskExecution) {
    std::atomic<bool> executed{false};
    auto start = std::chrono::steady_clock::now();

    loop->run();
    loop->postDelayed(200ms, [&executed]() { executed = true; });

    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(executed);

    std::this_thread::sleep_for(200ms);
    EXPECT_TRUE(executed);
}

TEST_F(EventLoopTest, EventSubscription) {
    std::atomic<int> callback_count{0};

    loop->run();
    loop->subscribeEvent("test_event",
                         [&callback_count]() { callback_count++; });

    loop->emitEvent("test_event");
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(callback_count, 1);
}

TEST_F(EventLoopTest, MultipleEventSubscribers) {
    std::atomic<int> total_callbacks{0};

    loop->run();
    for (int i = 0; i < 3; i++) {
        loop->subscribeEvent("multi_event",
                             [&total_callbacks]() { total_callbacks++; });
    }

    loop->emitEvent("multi_event");
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(total_callbacks, 3);
}

TEST_F(EventLoopTest, TaskCancellation) {
    std::atomic<bool> cancel_flag{false};
    std::atomic<bool> task_executed{false};

    loop->run();
    auto future = loop->postCancelable(
        [&task_executed]() { task_executed = true; }, cancel_flag);

    cancel_flag = true;
    std::this_thread::sleep_for(100ms);

    EXPECT_FALSE(task_executed);
}

TEST_F(EventLoopTest, IntervalTimer) {
    std::atomic<int> tick_count{0};

    loop->run();
    loop->setInterval([&tick_count]() { tick_count++; }, 100ms);

    std::this_thread::sleep_for(350ms);
    EXPECT_GE(tick_count, 3);
}

#ifdef __linux__
TEST_F(EventLoopTest, SignalHandling) {
    std::atomic<bool> signal_handled{false};

    loop->run();
    loop->addSignalHandler(SIGUSR1,
                           [&signal_handled]() { signal_handled = true; });

    raise(SIGUSR1);
    std::this_thread::sleep_for(100ms);

    EXPECT_TRUE(signal_handled);
}
#endif

TEST_F(EventLoopTest, TaskDependency) {
    std::atomic<int> execution_order{0};
    std::promise<void> dependency;

    loop->run();
    loop->postWithDependency([&execution_order]() { execution_order = 2; },
                             [&dependency, &execution_order]() {
                                 execution_order = 1;
                                 dependency.set_value();
                             });

    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(execution_order, 2);
}

TEST_F(EventLoopTest, AdjustTaskPriority) {
    std::atomic<int> task_id{-1};
    std::atomic<bool> executed{false};

    loop->run();
    auto future = loop->post(2, [&executed]() {
        executed = true;
        return 42;
    });

    EXPECT_TRUE(loop->adjustTaskPriority(0, 1));
    std::this_thread::sleep_for(100ms);

    EXPECT_TRUE(executed);
}

#endif  // TEST_EVENT_LOOP_HPP