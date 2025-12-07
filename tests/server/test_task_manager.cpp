/*
 * test_task_manager.cpp - Tests for Task Manager
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "server/eventloop.hpp"
#include "server/task_manager.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace lithium::server;
using namespace lithium::app;
using namespace std::chrono_literals;
using json = nlohmann::json;

class TaskManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        eventLoop_ = std::make_shared<EventLoop>(2);
        eventLoop_->run();
        taskManager_ = std::make_unique<TaskManager>(eventLoop_);
    }

    void TearDown() override {
        taskManager_.reset();
        eventLoop_->stop();
        eventLoop_.reset();
    }

    std::shared_ptr<EventLoop> eventLoop_;
    std::unique_ptr<TaskManager> taskManager_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(TaskManagerTest, BasicConstruction) {
    auto loop = std::make_shared<EventLoop>(1);
    loop->run();
    EXPECT_NO_THROW(TaskManager(loop));
    loop->stop();
}

TEST_F(TaskManagerTest, GetEventLoop) {
    auto loop = taskManager_->getEventLoop();
    EXPECT_NE(loop, nullptr);
    EXPECT_EQ(loop, eventLoop_);
}

// ============================================================================
// Task Submission Tests
// ============================================================================

TEST_F(TaskManagerTest, SubmitTaskReturnsId) {
    std::string taskId =
        taskManager_->submitTask("test_type", json{{"key", "value"}},
                                 [](const TaskManager::TaskInfoPtr&) {});

    EXPECT_FALSE(taskId.empty());
}

TEST_F(TaskManagerTest, SubmitTaskExecutes) {
    std::atomic<bool> executed{false};

    taskManager_->submitTask(
        "exec_test", json{},
        [&executed](const TaskManager::TaskInfoPtr&) { executed = true; });

    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(executed);
}

TEST_F(TaskManagerTest, SubmitTaskWithParams) {
    std::atomic<int> received_value{0};
    json params = {{"value", 42}};

    taskManager_->submitTask(
        "params_test", params,
        [&received_value](const TaskManager::TaskInfoPtr& task) {
            received_value = task->params["value"].get<int>();
        });

    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(received_value, 42);
}

TEST_F(TaskManagerTest, SubmitTaskWithPriority) {
    std::vector<int> execution_order;
    std::mutex mutex;

    // Submit tasks with different priorities
    taskManager_->submitTask(
        "low_priority", json{},
        [&](const TaskManager::TaskInfoPtr&) {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(3);
        },
        3);

    taskManager_->submitTask(
        "high_priority", json{},
        [&](const TaskManager::TaskInfoPtr&) {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(1);
        },
        1);

    taskManager_->submitTask(
        "medium_priority", json{},
        [&](const TaskManager::TaskInfoPtr&) {
            std::lock_guard<std::mutex> lock(mutex);
            execution_order.push_back(2);
        },
        2);

    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(execution_order.size(), 3);
}

// ============================================================================
// Delayed Task Tests
// ============================================================================

TEST_F(TaskManagerTest, SubmitDelayedTask) {
    std::atomic<bool> executed{false};
    auto start = std::chrono::steady_clock::now();

    taskManager_->submitDelayedTask(
        "delayed_test", json{},
        [&executed](const TaskManager::TaskInfoPtr&) { executed = true; },
        200ms);

    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(executed);

    std::this_thread::sleep_for(200ms);
    EXPECT_TRUE(executed);
}

TEST_F(TaskManagerTest, DelayedTaskWithPriority) {
    std::atomic<bool> executed{false};

    std::string taskId = taskManager_->submitDelayedTask(
        "delayed_priority", json{},
        [&executed](const TaskManager::TaskInfoPtr&) { executed = true; },
        100ms, 5);

    EXPECT_FALSE(taskId.empty());

    std::this_thread::sleep_for(200ms);
    EXPECT_TRUE(executed);
}

// ============================================================================
// Task Retrieval Tests
// ============================================================================

TEST_F(TaskManagerTest, GetTaskById) {
    std::string taskId =
        taskManager_->submitTask("get_test", json{{"data", "test"}},
                                 [](const TaskManager::TaskInfoPtr&) {
                                     std::this_thread::sleep_for(100ms);
                                 });

    auto task = taskManager_->getTask(taskId);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->id, taskId);
    EXPECT_EQ(task->type, "get_test");
}

TEST_F(TaskManagerTest, GetTaskNotFound) {
    auto task = taskManager_->getTask("nonexistent_id");
    EXPECT_EQ(task, nullptr);
}

TEST_F(TaskManagerTest, ListActiveTasks) {
    // Submit a long-running task
    taskManager_->submitTask("active_test", json{},
                             [](const TaskManager::TaskInfoPtr&) {
                                 std::this_thread::sleep_for(500ms);
                             });

    std::this_thread::sleep_for(50ms);

    auto activeTasks = taskManager_->listActiveTasks();
    EXPECT_GE(activeTasks.size(), 1);
}

TEST_F(TaskManagerTest, ListAllTasks) {
    // Submit multiple tasks
    for (int i = 0; i < 5; ++i) {
        taskManager_->submitTask("all_test_" + std::to_string(i), json{},
                                 [](const TaskManager::TaskInfoPtr&) {});
    }

    std::this_thread::sleep_for(200ms);

    auto allTasks = taskManager_->listAllTasks();
    EXPECT_EQ(allTasks.size(), 5);
}

TEST_F(TaskManagerTest, ListAllTasksWithLimit) {
    for (int i = 0; i < 10; ++i) {
        taskManager_->submitTask("limit_test", json{},
                                 [](const TaskManager::TaskInfoPtr&) {});
    }

    std::this_thread::sleep_for(200ms);

    auto limitedTasks = taskManager_->listAllTasks(5, 0);
    EXPECT_EQ(limitedTasks.size(), 5);
}

TEST_F(TaskManagerTest, ListAllTasksWithOffset) {
    for (int i = 0; i < 10; ++i) {
        taskManager_->submitTask("offset_test", json{},
                                 [](const TaskManager::TaskInfoPtr&) {});
    }

    std::this_thread::sleep_for(200ms);

    auto offsetTasks = taskManager_->listAllTasks(0, 3);
    EXPECT_EQ(offsetTasks.size(), 7);
}

TEST_F(TaskManagerTest, ListTasksByStatus) {
    // Submit tasks that will complete
    for (int i = 0; i < 3; ++i) {
        taskManager_->submitTask("status_test", json{},
                                 [](const TaskManager::TaskInfoPtr&) {});
    }

    std::this_thread::sleep_for(200ms);

    auto completedTasks =
        taskManager_->listTasksByStatus(TaskManager::Status::Completed);
    EXPECT_EQ(completedTasks.size(), 3);
}

TEST_F(TaskManagerTest, ListTasksByType) {
    taskManager_->submitTask("type_a", json{},
                             [](const TaskManager::TaskInfoPtr&) {});
    taskManager_->submitTask("type_a", json{},
                             [](const TaskManager::TaskInfoPtr&) {});
    taskManager_->submitTask("type_b", json{},
                             [](const TaskManager::TaskInfoPtr&) {});

    std::this_thread::sleep_for(200ms);

    auto typeATasks = taskManager_->listTasksByType("type_a");
    auto typeBTasks = taskManager_->listTasksByType("type_b");

    EXPECT_EQ(typeATasks.size(), 2);
    EXPECT_EQ(typeBTasks.size(), 1);
}

// ============================================================================
// Task Cancellation Tests
// ============================================================================

TEST_F(TaskManagerTest, CancelPendingTask) {
    std::atomic<bool> executed{false};

    std::string taskId = taskManager_->submitDelayedTask(
        "cancel_test", json{},
        [&executed](const TaskManager::TaskInfoPtr&) { executed = true; },
        500ms);

    bool cancelled = taskManager_->cancelTask(taskId);
    EXPECT_TRUE(cancelled);

    std::this_thread::sleep_for(600ms);
    EXPECT_FALSE(executed);

    auto task = taskManager_->getTask(taskId);
    EXPECT_EQ(task->status, TaskManager::Status::Cancelled);
}

TEST_F(TaskManagerTest, CancelRunningTask) {
    std::atomic<bool> cancel_checked{false};

    std::string taskId = taskManager_->submitTask(
        "running_cancel", json{},
        [&cancel_checked](const TaskManager::TaskInfoPtr& task) {
            while (!task->cancelRequested.load()) {
                std::this_thread::sleep_for(10ms);
            }
            cancel_checked = true;
        });

    std::this_thread::sleep_for(50ms);
    bool cancelled = taskManager_->cancelTask(taskId);
    EXPECT_TRUE(cancelled);

    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(cancel_checked);
}

TEST_F(TaskManagerTest, CancelNonexistentTask) {
    bool result = taskManager_->cancelTask("nonexistent");
    EXPECT_FALSE(result);
}

// ============================================================================
// Progress Update Tests
// ============================================================================

TEST_F(TaskManagerTest, UpdateProgress) {
    std::string taskId = taskManager_->submitTask(
        "progress_test", json{}, [](const TaskManager::TaskInfoPtr& task) {
            std::this_thread::sleep_for(500ms);
        });

    std::this_thread::sleep_for(50ms);

    bool updated = taskManager_->updateProgress(taskId, 50.0, "Halfway done");
    EXPECT_TRUE(updated);

    auto task = taskManager_->getTask(taskId);
    EXPECT_DOUBLE_EQ(task->progress, 50.0);
    EXPECT_EQ(task->progressMessage, "Halfway done");
}

TEST_F(TaskManagerTest, UpdateProgressClamps) {
    std::string taskId = taskManager_->submitTask(
        "clamp_test", json{}, [](const TaskManager::TaskInfoPtr&) {
            std::this_thread::sleep_for(200ms);
        });

    std::this_thread::sleep_for(50ms);

    taskManager_->updateProgress(taskId, 150.0);
    auto task = taskManager_->getTask(taskId);
    EXPECT_DOUBLE_EQ(task->progress, 100.0);

    taskManager_->updateProgress(taskId, -50.0);
    task = taskManager_->getTask(taskId);
    EXPECT_DOUBLE_EQ(task->progress, 0.0);
}

TEST_F(TaskManagerTest, UpdateProgressNonexistent) {
    bool result = taskManager_->updateProgress("nonexistent", 50.0);
    EXPECT_FALSE(result);
}

// ============================================================================
// Result and Error Tests
// ============================================================================

TEST_F(TaskManagerTest, SetResult) {
    std::string taskId = taskManager_->submitTask(
        "result_test", json{}, [](const TaskManager::TaskInfoPtr&) {
            std::this_thread::sleep_for(200ms);
        });

    std::this_thread::sleep_for(50ms);

    json result = {{"output", "success"}, {"count", 42}};
    bool set = taskManager_->setResult(taskId, result);
    EXPECT_TRUE(set);

    auto task = taskManager_->getTask(taskId);
    EXPECT_EQ(task->result["output"], "success");
    EXPECT_EQ(task->result["count"], 42);
}

TEST_F(TaskManagerTest, SetResultNonexistent) {
    bool result = taskManager_->setResult("nonexistent", json{});
    EXPECT_FALSE(result);
}

TEST_F(TaskManagerTest, FailTask) {
    std::string taskId = taskManager_->submitTask(
        "fail_test", json{}, [](const TaskManager::TaskInfoPtr&) {
            std::this_thread::sleep_for(200ms);
        });

    std::this_thread::sleep_for(50ms);

    bool failed = taskManager_->failTask(taskId, "Test error message");
    EXPECT_TRUE(failed);

    auto task = taskManager_->getTask(taskId);
    EXPECT_EQ(task->status, TaskManager::Status::Failed);
    EXPECT_EQ(task->error, "Test error message");
}

TEST_F(TaskManagerTest, FailTaskNonexistent) {
    bool result = taskManager_->failTask("nonexistent", "error");
    EXPECT_FALSE(result);
}

TEST_F(TaskManagerTest, TaskExceptionHandling) {
    std::string taskId = taskManager_->submitTask(
        "exception_test", json{}, [](const TaskManager::TaskInfoPtr&) {
            throw std::runtime_error("Test exception");
        });

    std::this_thread::sleep_for(100ms);

    auto task = taskManager_->getTask(taskId);
    EXPECT_EQ(task->status, TaskManager::Status::Failed);
    EXPECT_FALSE(task->error.empty());
}

// ============================================================================
// Cleanup Tests
// ============================================================================

TEST_F(TaskManagerTest, CleanupOldTasks) {
    // Submit tasks that complete quickly
    for (int i = 0; i < 5; ++i) {
        taskManager_->submitTask("cleanup_test", json{},
                                 [](const TaskManager::TaskInfoPtr&) {});
    }

    std::this_thread::sleep_for(200ms);

    // Cleanup tasks older than 0 seconds (all completed tasks)
    size_t removed = taskManager_->cleanupOldTasks(0s);
    EXPECT_EQ(removed, 5);

    auto allTasks = taskManager_->listAllTasks();
    EXPECT_EQ(allTasks.size(), 0);
}

TEST_F(TaskManagerTest, CleanupKeepsActiveTasks) {
    // Submit a long-running task
    taskManager_->submitTask("active_cleanup", json{},
                             [](const TaskManager::TaskInfoPtr&) {
                                 std::this_thread::sleep_for(500ms);
                             });

    std::this_thread::sleep_for(50ms);

    size_t removed = taskManager_->cleanupOldTasks(0s);
    EXPECT_EQ(removed, 0);

    auto activeTasks = taskManager_->listActiveTasks();
    EXPECT_GE(activeTasks.size(), 1);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(TaskManagerTest, GetStatsEmpty) {
    auto stats = taskManager_->getStats();

    EXPECT_EQ(stats["total_tasks"], 0);
    EXPECT_EQ(stats["pending"], 0);
    EXPECT_EQ(stats["running"], 0);
    EXPECT_EQ(stats["completed"], 0);
    EXPECT_EQ(stats["failed"], 0);
    EXPECT_EQ(stats["cancelled"], 0);
}

TEST_F(TaskManagerTest, GetStatsAfterTasks) {
    // Submit and complete tasks
    for (int i = 0; i < 3; ++i) {
        taskManager_->submitTask("stats_test", json{},
                                 [](const TaskManager::TaskInfoPtr&) {});
    }

    std::this_thread::sleep_for(200ms);

    auto stats = taskManager_->getStats();
    EXPECT_EQ(stats["total_tasks"], 3);
    EXPECT_EQ(stats["completed"], 3);
    EXPECT_EQ(stats["total_submitted"], 3);
    EXPECT_EQ(stats["total_completed"], 3);
}

TEST_F(TaskManagerTest, GetStatsWithFailedTasks) {
    taskManager_->submitTask(
        "fail_stats", json{}, [](const TaskManager::TaskInfoPtr&) {
            throw std::runtime_error("Intentional failure");
        });

    std::this_thread::sleep_for(100ms);

    auto stats = taskManager_->getStats();
    EXPECT_EQ(stats["failed"], 1);
    EXPECT_EQ(stats["total_failed"], 1);
}

// ============================================================================
// Status Callback Tests
// ============================================================================

TEST_F(TaskManagerTest, StatusCallbackCalled) {
    std::atomic<int> callback_count{0};
    std::vector<TaskManager::Status> statuses;
    std::mutex mutex;

    taskManager_->setStatusCallback([&](const TaskManager::TaskInfo& info) {
        std::lock_guard<std::mutex> lock(mutex);
        statuses.push_back(info.status);
        ++callback_count;
    });

    taskManager_->submitTask("callback_test", json{},
                             [](const TaskManager::TaskInfoPtr&) {});

    std::this_thread::sleep_for(200ms);

    // Should have received: Pending, Running, Completed
    EXPECT_GE(callback_count.load(), 2);
}

// ============================================================================
// Periodic Task Tests
// ============================================================================

TEST_F(TaskManagerTest, SchedulePeriodicTask) {
    std::atomic<int> execution_count{0};

    std::string periodicId = taskManager_->schedulePeriodicTask(
        "periodic_test", 50ms, [&execution_count]() { ++execution_count; });

    EXPECT_FALSE(periodicId.empty());

    std::this_thread::sleep_for(200ms);

    taskManager_->cancelPeriodicTask(periodicId);

    EXPECT_GE(execution_count.load(), 2);
}

TEST_F(TaskManagerTest, CancelPeriodicTask) {
    std::atomic<int> execution_count{0};

    std::string periodicId = taskManager_->schedulePeriodicTask(
        "cancel_periodic", 50ms, [&execution_count]() { ++execution_count; });

    std::this_thread::sleep_for(100ms);

    bool cancelled = taskManager_->cancelPeriodicTask(periodicId);
    EXPECT_TRUE(cancelled);

    int count_at_cancel = execution_count.load();
    std::this_thread::sleep_for(200ms);

    // Should not have executed more after cancellation
    EXPECT_LE(execution_count.load(), count_at_cancel + 1);
}

TEST_F(TaskManagerTest, CancelNonexistentPeriodicTask) {
    bool result = taskManager_->cancelPeriodicTask("nonexistent");
    EXPECT_FALSE(result);
}

TEST_F(TaskManagerTest, PeriodicTaskExceptionHandling) {
    std::atomic<int> execution_count{0};

    std::string periodicId = taskManager_->schedulePeriodicTask(
        "exception_periodic", 50ms, [&execution_count]() {
            ++execution_count;
            if (execution_count == 2) {
                throw std::runtime_error("Periodic exception");
            }
        });

    std::this_thread::sleep_for(300ms);

    taskManager_->cancelPeriodicTask(periodicId);

    // Should continue executing despite exception
    EXPECT_GE(execution_count.load(), 3);
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

TEST_F(TaskManagerTest, ConcurrentTaskSubmission) {
    std::atomic<int> completed_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &completed_count]() {
            for (int j = 0; j < 10; ++j) {
                taskManager_->submitTask(
                    "concurrent_submit", json{},
                    [&completed_count](const TaskManager::TaskInfoPtr&) {
                        ++completed_count;
                    });
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(completed_count.load(), 100);
}

TEST_F(TaskManagerTest, ConcurrentTaskRetrieval) {
    // Submit some tasks first
    std::vector<std::string> taskIds;
    for (int i = 0; i < 20; ++i) {
        taskIds.push_back(taskManager_->submitTask(
            "retrieve_test", json{}, [](const TaskManager::TaskInfoPtr&) {
                std::this_thread::sleep_for(100ms);
            }));
    }

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &taskIds, &success_count]() {
            for (const auto& id : taskIds) {
                if (taskManager_->getTask(id) != nullptr) {
                    ++success_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 100);  // 5 threads * 20 tasks
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(TaskManagerTest, EmptyTaskType) {
    std::string taskId = taskManager_->submitTask(
        "", json{}, [](const TaskManager::TaskInfoPtr&) {});

    EXPECT_FALSE(taskId.empty());

    auto task = taskManager_->getTask(taskId);
    EXPECT_EQ(task->type, "");
}

TEST_F(TaskManagerTest, NullJsonParams) {
    std::string taskId = taskManager_->submitTask(
        "null_params", json{}, [](const TaskManager::TaskInfoPtr&) {});

    auto task = taskManager_->getTask(taskId);
    EXPECT_TRUE(task->params.empty());
}

TEST_F(TaskManagerTest, LargeJsonParams) {
    json largeParams;
    for (int i = 0; i < 1000; ++i) {
        largeParams["key_" + std::to_string(i)] = "value_" + std::to_string(i);
    }

    std::string taskId = taskManager_->submitTask(
        "large_params", largeParams, [](const TaskManager::TaskInfoPtr&) {});

    auto task = taskManager_->getTask(taskId);
    EXPECT_EQ(task->params.size(), 1000);
}
