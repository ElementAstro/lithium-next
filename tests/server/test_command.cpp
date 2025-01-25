#ifndef TEST_COMMAND_HPP
#define TEST_COMMAND_HPP

#include <gtest/gtest.h>
#include "server/command.hpp"
#include "server/eventloop.hpp"

using namespace lithium::app;
using namespace std::chrono_literals;

class CommandDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        eventLoop = std::make_shared<EventLoop>(2);
        CommandDispatcher::Config config;
        config.maxHistorySize = 10;
        config.defaultTimeout = 1000ms;
        dispatcher = std::make_unique<CommandDispatcher>(eventLoop, config);
        eventLoop->run();
    }

    void TearDown() override {
        eventLoop->stop();
        dispatcher.reset();
        eventLoop.reset();
    }

    std::shared_ptr<EventLoop> eventLoop;
    std::unique_ptr<CommandDispatcher> dispatcher;
};

// Basic command structure for testing
struct TestCommand {
    int value;
    std::string data;
};

TEST_F(CommandDispatcherTest, BasicCommandRegistration) {
    EXPECT_NO_THROW(dispatcher->registerCommand<TestCommand>(
        "test_cmd", [](const TestCommand&) { /* Do nothing */ }));
}

TEST_F(CommandDispatcherTest, CommandDispatchExecution) {
    std::atomic<bool> executed{false};
    TestCommand cmd{42, "test"};

    dispatcher->registerCommand<TestCommand>(
        "test_cmd", [&executed](const TestCommand& cmd) {
            executed = true;
            EXPECT_EQ(cmd.value, 42);
            EXPECT_EQ(cmd.data, "test");
        });

    auto future = dispatcher->dispatch("test_cmd", cmd);
    future.wait();

    EXPECT_TRUE(executed);
    EXPECT_EQ(dispatcher->getCommandStatus("test_cmd"),
              CommandStatus::COMPLETED);
}

TEST_F(CommandDispatcherTest, CommandPriorityExecution) {
    std::vector<int> execution_order;
    std::mutex mutex;

    dispatcher->registerCommand<int>("priority_cmd", [&](const int& value) {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(value);
    });

    dispatcher->dispatch("priority_cmd", 3, 3);
    dispatcher->dispatch("priority_cmd", 1, 1);
    dispatcher->dispatch("priority_cmd", 2, 2);

    std::this_thread::sleep_for(200ms);

    EXPECT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[0], 1);
    EXPECT_EQ(execution_order[1], 2);
    EXPECT_EQ(execution_order[2], 3);
}

TEST_F(CommandDispatcherTest, DelayedCommandExecution) {
    std::atomic<bool> executed{false};
    TestCommand cmd{42, "delayed"};

    dispatcher->registerCommand<TestCommand>(
        "delayed_cmd", [&executed](const TestCommand&) { executed = true; });

    dispatcher->dispatch("delayed_cmd", cmd, 0, 200ms);

    EXPECT_FALSE(executed);
    std::this_thread::sleep_for(300ms);
    EXPECT_TRUE(executed);
}

TEST_F(CommandDispatcherTest, CommandCancellation) {
    std::atomic<bool> executed{false};
    TestCommand cmd{42, "cancel"};

    dispatcher->registerCommand<TestCommand>(
        "cancel_cmd", [&executed](const TestCommand&) { executed = true; });

    auto future = dispatcher->dispatch("cancel_cmd", cmd, 0, 200ms);
    dispatcher->cancelCommand("cancel_cmd");

    std::this_thread::sleep_for(300ms);
    EXPECT_FALSE(executed);
    EXPECT_EQ(dispatcher->getCommandStatus("cancel_cmd"),
              CommandStatus::CANCELLED);
}

TEST_F(CommandDispatcherTest, CommandHistory) {
    TestCommand cmd1{1, "first"};
    TestCommand cmd2{2, "second"};

    dispatcher->registerCommand<TestCommand>(
        "history_cmd", [](const TestCommand&) { /* Do nothing */ });

    dispatcher->dispatch("history_cmd", cmd1).wait();
    dispatcher->dispatch("history_cmd", cmd2).wait();

    auto history = dispatcher->getCommandHistory<TestCommand>("history_cmd");
    EXPECT_EQ(history.size(), 2);
    EXPECT_EQ(history[0].value, 1);
    EXPECT_EQ(history[1].value, 2);
}

TEST_F(CommandDispatcherTest, EventSubscription) {
    std::atomic<int> callback_count{0};

    int token = dispatcher->subscribe(
        "subscription_cmd",
        [&callback_count](const std::string&, const std::any&) {
            callback_count++;
        });

    TestCommand cmd{42, "event"};
    dispatcher->registerCommand<TestCommand>(
        "subscription_cmd", [](const TestCommand&) { /* Do nothing */ });

    dispatcher->dispatch("subscription_cmd", cmd).wait();
    EXPECT_EQ(callback_count, 1);

    dispatcher->unsubscribe("subscription_cmd", token);
    dispatcher->dispatch("subscription_cmd", cmd).wait();
    EXPECT_EQ(callback_count, 1);
}

TEST_F(CommandDispatcherTest, BatchCommandExecution) {
    std::atomic<int> execution_count{0};

    dispatcher->registerCommand<TestCommand>(
        "batch_cmd",
        [&execution_count](const TestCommand&) { execution_count++; });

    std::vector<std::pair<std::string, TestCommand>> commands;
    commands.push_back({"batch_cmd", TestCommand{1, "first"}});
    commands.push_back({"batch_cmd", TestCommand{2, "second"}});
    commands.push_back({"batch_cmd", TestCommand{3, "third"}});

    auto futures = dispatcher->batchDispatch(commands);
    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(execution_count, 3);
}

TEST_F(CommandDispatcherTest, CommandTimeout) {
    TestCommand cmd{42, "timeout"};

    dispatcher->registerCommand<TestCommand>(
        "timeout_cmd",
        [](const TestCommand&) { std::this_thread::sleep_for(2000ms); });

    dispatcher->setTimeout("timeout_cmd", 100ms);
    auto future = dispatcher->dispatch("timeout_cmd", cmd);

    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(dispatcher->getCommandStatus("timeout_cmd"),
              CommandStatus::FAILED);
}

TEST_F(CommandDispatcherTest, UndoRedoOperation) {
    std::atomic<int> value{0};
    TestCommand cmd{42, "undo"};

    dispatcher->registerCommand<TestCommand>(
        "undo_cmd", [&value](const TestCommand& cmd) { value = cmd.value; },
        [&value](const TestCommand&) { value = 0; });

    dispatcher->dispatch("undo_cmd", cmd).wait();
    EXPECT_EQ(value, 42);

    dispatcher->undo("undo_cmd", cmd);
    EXPECT_EQ(value, 0);

    dispatcher->redo("undo_cmd", cmd);
    EXPECT_EQ(value, 42);
}

#endif  // TEST_COMMAND_HPP
