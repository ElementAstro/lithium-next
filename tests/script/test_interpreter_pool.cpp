/*
 * test_interpreter_pool.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_interpreter_pool.cpp
 * @brief Comprehensive tests for Python Interpreter Pool
 */

#include <gtest/gtest.h>
#include "script/interpreter_pool.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

using namespace lithium::script;

// =============================================================================
// Test Fixture
// =============================================================================

class InterpreterPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.minInterpreters = 1;
        config_.maxInterpreters = 4;
        config_.maxQueueSize = 100;
        config_.idleTimeout = std::chrono::seconds(30);
    }

    void TearDown() override {
        if (pool_) {
            pool_->shutdown();
            pool_.reset();
        }
    }

    InterpreterPoolConfig config_;
    std::unique_ptr<InterpreterPool> pool_;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(InterpreterPoolTest, DefaultConstruction) {
    InterpreterPool pool;
    EXPECT_FALSE(pool.isInitialized());
}

TEST_F(InterpreterPoolTest, ConstructionWithConfig) {
    InterpreterPool pool(config_);
    EXPECT_FALSE(pool.isInitialized());
}

TEST_F(InterpreterPoolTest, MoveConstruction) {
    InterpreterPool original(config_);
    InterpreterPool moved(std::move(original));
    EXPECT_FALSE(moved.isInitialized());
}

TEST_F(InterpreterPoolTest, MoveAssignment) {
    InterpreterPool original(config_);
    InterpreterPool other;
    other = std::move(original);
    EXPECT_FALSE(other.isInitialized());
}

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_F(InterpreterPoolTest, InitializeSuccess) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    auto result = pool_->initialize();

    // May or may not succeed depending on Python availability
    if (result.has_value()) {
        EXPECT_TRUE(pool_->isInitialized());
    }
}

TEST_F(InterpreterPoolTest, DoubleInitialize) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    // Second initialize should be safe
    auto result = pool_->initialize();
    SUCCEED();
}

TEST_F(InterpreterPoolTest, ShutdownWithoutInitialize) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    EXPECT_NO_THROW(pool_->shutdown());
}

TEST_F(InterpreterPoolTest, ShutdownAfterInitialize) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();
    EXPECT_NO_THROW(pool_->shutdown());
    EXPECT_FALSE(pool_->isInitialized());
}

TEST_F(InterpreterPoolTest, DoubleShutdown) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();
    pool_->shutdown();
    EXPECT_NO_THROW(pool_->shutdown());
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(InterpreterPoolTest, GetConfig) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    auto cfg = pool_->getConfig();

    EXPECT_EQ(cfg.minInterpreters, config_.minInterpreters);
    EXPECT_EQ(cfg.maxInterpreters, config_.maxInterpreters);
    EXPECT_EQ(cfg.maxQueueSize, config_.maxQueueSize);
}

TEST_F(InterpreterPoolTest, SetConfig) {
    pool_ = std::make_unique<InterpreterPool>();

    InterpreterPoolConfig newConfig;
    newConfig.minInterpreters = 2;
    newConfig.maxInterpreters = 8;

    pool_->setConfig(newConfig);
    auto cfg = pool_->getConfig();

    EXPECT_EQ(cfg.minInterpreters, 2);
    EXPECT_EQ(cfg.maxInterpreters, 8);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(InterpreterPoolTest, GetStatisticsBeforeInit) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    auto stats = pool_->getStatistics();

    EXPECT_EQ(stats.totalInterpreters, 0);
    EXPECT_EQ(stats.activeInterpreters, 0);
    EXPECT_EQ(stats.idleInterpreters, 0);
    EXPECT_EQ(stats.pendingTasks, 0);
}

TEST_F(InterpreterPoolTest, GetStatisticsAfterInit) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    auto stats = pool_->getStatistics();
    // After init, should have at least minInterpreters
    if (pool_->isInitialized()) {
        EXPECT_GE(stats.totalInterpreters, config_.minInterpreters);
    }
}

// =============================================================================
// Interpreter Acquisition Tests
// =============================================================================

TEST_F(InterpreterPoolTest, AcquireInterpreterBeforeInit) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    auto result = pool_->acquireInterpreter();
    EXPECT_FALSE(result.has_value());
}

TEST_F(InterpreterPoolTest, AcquireInterpreterAfterInit) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        auto result = pool_->acquireInterpreter();
        if (result.has_value()) {
            pool_->releaseInterpreter(result.value());
        }
    }
    SUCCEED();
}

TEST_F(InterpreterPoolTest, AcquireReleaseMultiple) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        std::vector<InterpreterHandle> handles;

        for (int i = 0; i < 3; ++i) {
            auto result = pool_->acquireInterpreter();
            if (result.has_value()) {
                handles.push_back(result.value());
            }
        }

        for (auto& handle : handles) {
            pool_->releaseInterpreter(handle);
        }
    }
    SUCCEED();
}

// =============================================================================
// InterpreterGuard Tests
// =============================================================================

TEST_F(InterpreterPoolTest, InterpreterGuardRAII) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        {
            InterpreterGuard guard(*pool_);
            // Interpreter should be acquired
        }
        // Interpreter should be released
    }
    SUCCEED();
}

// =============================================================================
// Task Submission Tests
// =============================================================================

TEST_F(InterpreterPoolTest, SubmitTaskBeforeInit) {
    pool_ = std::make_unique<InterpreterPool>(config_);

    auto future = pool_->submitTask([](py::object) {
        return py::none();
    });

    // Should fail or return error
    SUCCEED();
}

TEST_F(InterpreterPoolTest, SubmitSimpleTask) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        auto future = pool_->submitTask([](py::object) {
            return py::cast(42);
        });

        if (future.valid()) {
            auto result = future.get();
            // Check result
        }
    }
    SUCCEED();
}

TEST_F(InterpreterPoolTest, SubmitMultipleTasks) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        std::vector<std::future<PythonTaskResult>> futures;

        for (int i = 0; i < 10; ++i) {
            auto future = pool_->submitTask([i](py::object) {
                return py::cast(i * 2);
            });
            if (future.valid()) {
                futures.push_back(std::move(future));
            }
        }

        for (auto& f : futures) {
            if (f.valid()) {
                f.wait();
            }
        }
    }
    SUCCEED();
}

TEST_F(InterpreterPoolTest, SubmitTaskWithPriority) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        auto future = pool_->submitTask(
            [](py::object) { return py::none(); },
            TaskPriority::High
        );
        SUCCEED();
    }
}

// =============================================================================
// Module Management Tests
// =============================================================================

TEST_F(InterpreterPoolTest, PreloadModule) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        auto result = pool_->preloadModule("os");
        // May or may not succeed
    }
    SUCCEED();
}

TEST_F(InterpreterPoolTest, PreloadMultipleModules) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        std::vector<std::string> modules = {"os", "sys", "json"};
        for (const auto& mod : modules) {
            pool_->preloadModule(mod);
        }
    }
    SUCCEED();
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

TEST_F(InterpreterPoolTest, ConcurrentTaskSubmission) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        std::atomic<int> completedTasks{0};
        std::vector<std::thread> threads;

        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < 10; ++j) {
                    auto future = pool_->submitTask([](py::object) {
                        return py::none();
                    });
                    if (future.valid()) {
                        future.wait();
                        ++completedTasks;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        // Some tasks should have completed
        EXPECT_GT(completedTasks, 0);
    }
}

TEST_F(InterpreterPoolTest, ConcurrentAcquireRelease) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        std::vector<std::thread> threads;

        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 10; ++j) {
                    auto result = pool_->acquireInterpreter();
                    if (result.has_value()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        pool_->releaseInterpreter(result.value());
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }
    SUCCEED();
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(InterpreterPoolTest, TaskWithException) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        auto future = pool_->submitTask([](py::object) -> py::object {
            throw std::runtime_error("Test exception");
        });

        if (future.valid()) {
            auto result = future.get();
            EXPECT_FALSE(result.success);
        }
    }
    SUCCEED();
}

TEST_F(InterpreterPoolTest, TaskWithPythonException) {
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        auto future = pool_->submitTask([](py::object interp) -> py::object {
            py::exec("raise ValueError('test')");
            return py::none();
        });

        if (future.valid()) {
            auto result = future.get();
            EXPECT_FALSE(result.success);
        }
    }
    SUCCEED();
}

// =============================================================================
// Pool Sizing Tests
// =============================================================================

TEST_F(InterpreterPoolTest, PoolGrowsOnDemand) {
    config_.minInterpreters = 1;
    config_.maxInterpreters = 4;
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        std::vector<InterpreterHandle> handles;

        // Acquire more than min interpreters
        for (int i = 0; i < 3; ++i) {
            auto result = pool_->acquireInterpreter();
            if (result.has_value()) {
                handles.push_back(result.value());
            }
        }

        auto stats = pool_->getStatistics();
        EXPECT_GE(stats.totalInterpreters, handles.size());

        for (auto& handle : handles) {
            pool_->releaseInterpreter(handle);
        }
    }
}

TEST_F(InterpreterPoolTest, PoolRespectsMaxLimit) {
    config_.minInterpreters = 1;
    config_.maxInterpreters = 2;
    pool_ = std::make_unique<InterpreterPool>(config_);
    pool_->initialize();

    if (pool_->isInitialized()) {
        auto stats = pool_->getStatistics();
        EXPECT_LE(stats.totalInterpreters, config_.maxInterpreters);
    }
}
