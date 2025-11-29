/*
 * test_hooks.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_hooks.cpp
 * @brief Comprehensive tests for Hook Manager
 */

#include <gtest/gtest.h>
#include "script/shell/hooks.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace lithium::shell;

// =============================================================================
// Test Fixture
// =============================================================================

class HookManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<HookManager>();
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<HookManager> manager_;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(HookManagerTest, DefaultConstruction) {
    HookManager manager;
    EXPECT_EQ(manager.getPreHookCount(), 0);
    EXPECT_EQ(manager.getPostHookCount(), 0);
    EXPECT_TRUE(manager.isEnabled());
}

// =============================================================================
// Pre-Hook Tests
// =============================================================================

TEST_F(HookManagerTest, AddPreHook) {
    bool result = manager_->addPreHook("test_hook", [](const std::string&) {});
    EXPECT_TRUE(result);
    EXPECT_EQ(manager_->getPreHookCount(), 1);
}

TEST_F(HookManagerTest, AddPreHookDuplicateId) {
    manager_->addPreHook("test_hook", [](const std::string&) {});
    bool result = manager_->addPreHook("test_hook", [](const std::string&) {});
    EXPECT_FALSE(result);
    EXPECT_EQ(manager_->getPreHookCount(), 1);
}

TEST_F(HookManagerTest, AddMultiplePreHooks) {
    manager_->addPreHook("hook1", [](const std::string&) {});
    manager_->addPreHook("hook2", [](const std::string&) {});
    manager_->addPreHook("hook3", [](const std::string&) {});
    EXPECT_EQ(manager_->getPreHookCount(), 3);
}

TEST_F(HookManagerTest, ExecutePreHooks) {
    std::atomic<int> callCount{0};

    manager_->addPreHook("hook1", [&](const std::string&) { ++callCount; });
    manager_->addPreHook("hook2", [&](const std::string&) { ++callCount; });

    auto results = manager_->executePreHooks("test_script");

    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(HookManagerTest, PreHookReceivesScriptId) {
    std::string receivedId;

    manager_->addPreHook("test", [&](const std::string& scriptId) {
        receivedId = scriptId;
    });

    manager_->executePreHooks("my_script");
    EXPECT_EQ(receivedId, "my_script");
}

TEST_F(HookManagerTest, PreHookExceptionHandled) {
    manager_->addPreHook("throwing", [](const std::string&) {
        throw std::runtime_error("Test exception");
    });
    manager_->addPreHook("normal", [](const std::string&) {});

    auto results = manager_->executePreHooks("test");

    // Both hooks should have results, one failed
    EXPECT_EQ(results.size(), 2);
    bool foundFailure = false;
    for (const auto& result : results) {
        if (!result.success) {
            foundFailure = true;
            EXPECT_FALSE(result.errorMessage.empty());
        }
    }
    EXPECT_TRUE(foundFailure);
}

// =============================================================================
// Post-Hook Tests
// =============================================================================

TEST_F(HookManagerTest, AddPostHook) {
    bool result = manager_->addPostHook("test_hook", [](const std::string&, int) {});
    EXPECT_TRUE(result);
    EXPECT_EQ(manager_->getPostHookCount(), 1);
}

TEST_F(HookManagerTest, AddPostHookDuplicateId) {
    manager_->addPostHook("test_hook", [](const std::string&, int) {});
    bool result = manager_->addPostHook("test_hook", [](const std::string&, int) {});
    EXPECT_FALSE(result);
    EXPECT_EQ(manager_->getPostHookCount(), 1);
}

TEST_F(HookManagerTest, ExecutePostHooks) {
    std::atomic<int> callCount{0};

    manager_->addPostHook("hook1", [&](const std::string&, int) { ++callCount; });
    manager_->addPostHook("hook2", [&](const std::string&, int) { ++callCount; });

    auto results = manager_->executePostHooks("test_script", 0);

    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(results.size(), 2);
}

TEST_F(HookManagerTest, PostHookReceivesExitCode) {
    int receivedExitCode = -1;

    manager_->addPostHook("test", [&](const std::string&, int exitCode) {
        receivedExitCode = exitCode;
    });

    manager_->executePostHooks("script", 42);
    EXPECT_EQ(receivedExitCode, 42);
}

TEST_F(HookManagerTest, PostHookExceptionHandled) {
    manager_->addPostHook("throwing", [](const std::string&, int) {
        throw std::runtime_error("Test exception");
    });

    auto results = manager_->executePostHooks("test", 0);

    EXPECT_EQ(results.size(), 1);
    EXPECT_FALSE(results[0].success);
}

// =============================================================================
// Remove Hook Tests
// =============================================================================

TEST_F(HookManagerTest, RemovePreHook) {
    manager_->addPreHook("test", [](const std::string&) {});
    EXPECT_EQ(manager_->getPreHookCount(), 1);

    bool result = manager_->removePreHook("test");
    EXPECT_TRUE(result);
    EXPECT_EQ(manager_->getPreHookCount(), 0);
}

TEST_F(HookManagerTest, RemovePostHook) {
    manager_->addPostHook("test", [](const std::string&, int) {});
    EXPECT_EQ(manager_->getPostHookCount(), 1);

    bool result = manager_->removePostHook("test");
    EXPECT_TRUE(result);
    EXPECT_EQ(manager_->getPostHookCount(), 0);
}

TEST_F(HookManagerTest, RemoveHookGeneric) {
    manager_->addPreHook("pre", [](const std::string&) {});
    manager_->addPostHook("post", [](const std::string&, int) {});

    EXPECT_TRUE(manager_->removeHook("pre"));
    EXPECT_TRUE(manager_->removeHook("post"));
    EXPECT_EQ(manager_->getPreHookCount(), 0);
    EXPECT_EQ(manager_->getPostHookCount(), 0);
}

TEST_F(HookManagerTest, RemoveNonexistentHook) {
    bool result = manager_->removeHook("nonexistent");
    EXPECT_FALSE(result);
}

// =============================================================================
// Has Hook Tests
// =============================================================================

TEST_F(HookManagerTest, HasHookTrue) {
    manager_->addPreHook("test", [](const std::string&) {});
    EXPECT_TRUE(manager_->hasHook("test"));
}

TEST_F(HookManagerTest, HasHookFalse) {
    EXPECT_FALSE(manager_->hasHook("nonexistent"));
}

TEST_F(HookManagerTest, HasHookAfterRemove) {
    manager_->addPreHook("test", [](const std::string&) {});
    manager_->removeHook("test");
    EXPECT_FALSE(manager_->hasHook("test"));
}

// =============================================================================
// Clear Hooks Tests
// =============================================================================

TEST_F(HookManagerTest, ClearPreHooks) {
    manager_->addPreHook("hook1", [](const std::string&) {});
    manager_->addPreHook("hook2", [](const std::string&) {});

    manager_->clearPreHooks();
    EXPECT_EQ(manager_->getPreHookCount(), 0);
}

TEST_F(HookManagerTest, ClearPostHooks) {
    manager_->addPostHook("hook1", [](const std::string&, int) {});
    manager_->addPostHook("hook2", [](const std::string&, int) {});

    manager_->clearPostHooks();
    EXPECT_EQ(manager_->getPostHookCount(), 0);
}

TEST_F(HookManagerTest, ClearAllHooks) {
    manager_->addPreHook("pre", [](const std::string&) {});
    manager_->addPostHook("post", [](const std::string&, int) {});

    manager_->clearAllHooks();
    EXPECT_EQ(manager_->getPreHookCount(), 0);
    EXPECT_EQ(manager_->getPostHookCount(), 0);
}

// =============================================================================
// Enable/Disable Tests
// =============================================================================

TEST_F(HookManagerTest, DisableHooks) {
    manager_->setEnabled(false);
    EXPECT_FALSE(manager_->isEnabled());
}

TEST_F(HookManagerTest, EnableHooks) {
    manager_->setEnabled(false);
    manager_->setEnabled(true);
    EXPECT_TRUE(manager_->isEnabled());
}

TEST_F(HookManagerTest, DisabledHooksNotExecuted) {
    std::atomic<int> callCount{0};
    manager_->addPreHook("test", [&](const std::string&) { ++callCount; });

    manager_->setEnabled(false);
    auto results = manager_->executePreHooks("script");

    EXPECT_EQ(callCount, 0);
    EXPECT_TRUE(results.empty());
}

// =============================================================================
// History Tests
// =============================================================================

TEST_F(HookManagerTest, ExecutionHistoryRecorded) {
    manager_->addPreHook("test", [](const std::string&) {});
    manager_->executePreHooks("script1");
    manager_->executePreHooks("script2");

    auto history = manager_->getExecutionHistory();
    EXPECT_EQ(history.size(), 2);
}

TEST_F(HookManagerTest, GetScriptHistory) {
    manager_->addPreHook("test", [](const std::string&) {});
    manager_->executePreHooks("script1");
    manager_->executePreHooks("script2");
    manager_->executePreHooks("script1");

    auto history = manager_->getScriptHistory("script1");
    EXPECT_EQ(history.size(), 2);
}

TEST_F(HookManagerTest, ClearHistory) {
    manager_->addPreHook("test", [](const std::string&) {});
    manager_->executePreHooks("script");

    manager_->clearHistory();
    EXPECT_EQ(manager_->getHistorySize(), 0);
}

TEST_F(HookManagerTest, HistoryLimitedByMaxEntries) {
    manager_->addPreHook("test", [](const std::string&) {});

    for (int i = 0; i < 200; ++i) {
        manager_->executePreHooks("script");
    }

    auto history = manager_->getExecutionHistory(50);
    EXPECT_LE(history.size(), 50);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(HookManagerTest, ConcurrentHookExecution) {
    std::atomic<int> callCount{0};
    manager_->addPreHook("test", [&](const std::string&) {
        ++callCount;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            manager_->executePreHooks("script");
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(callCount, 10);
}

TEST_F(HookManagerTest, ConcurrentAddRemove) {
    std::atomic<bool> running{true};

    std::thread adder([&]() {
        int id = 0;
        while (running) {
            manager_->addPreHook("hook_" + std::to_string(id++), [](const std::string&) {});
            if (id > 100) id = 0;
        }
    });

    std::thread remover([&]() {
        int id = 0;
        while (running) {
            manager_->removeHook("hook_" + std::to_string(id++));
            if (id > 100) id = 0;
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    adder.join();
    remover.join();

    // Just verify no crashes
    SUCCEED();
}

// =============================================================================
// Hook Result Tests
// =============================================================================

TEST_F(HookManagerTest, HookResultContainsTimestamp) {
    manager_->addPreHook("test", [](const std::string&) {});

    auto before = std::chrono::system_clock::now();
    auto results = manager_->executePreHooks("script");
    auto after = std::chrono::system_clock::now();

    ASSERT_EQ(results.size(), 1);
    EXPECT_GE(results[0].timestamp, before);
    EXPECT_LE(results[0].timestamp, after);
}

TEST_F(HookManagerTest, HookResultContainsExecutionTime) {
    manager_->addPreHook("test", [](const std::string&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });

    auto results = manager_->executePreHooks("script");

    ASSERT_EQ(results.size(), 1);
    EXPECT_GE(results[0].executionTime.count(), 10);
}

TEST_F(HookManagerTest, HookResultContainsHookId) {
    manager_->addPreHook("my_hook", [](const std::string&) {});

    auto results = manager_->executePreHooks("script");

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].hookId, "my_hook");
}

TEST_F(HookManagerTest, HookResultContainsScriptId) {
    manager_->addPreHook("test", [](const std::string&) {});

    auto results = manager_->executePreHooks("my_script");

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].scriptId, "my_script");
}

TEST_F(HookManagerTest, HookResultContainsHookType) {
    manager_->addPreHook("pre", [](const std::string&) {});
    manager_->addPostHook("post", [](const std::string&, int) {});

    auto preResults = manager_->executePreHooks("script");
    auto postResults = manager_->executePostHooks("script", 0);

    ASSERT_EQ(preResults.size(), 1);
    ASSERT_EQ(postResults.size(), 1);
    EXPECT_EQ(preResults[0].hookType, "pre");
    EXPECT_EQ(postResults[0].hookType, "post");
}
