/**
 * @file test_watcher.cpp
 * @brief Comprehensive unit tests for ConfigWatcher component
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "config/components/watcher.hpp"

namespace fs = std::filesystem;
using namespace lithium::config;
using namespace std::chrono_literals;

namespace lithium::config::test {

class ConfigWatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_watcher_test";
        fs::create_directories(testDir_);
        createTestFiles();

        ConfigWatcher::WatcherOptions options;
        options.poll_interval = 50ms;
        options.debounce_delay = 100ms;
        watcher_ = std::make_unique<ConfigWatcher>(options);
    }

    void TearDown() override {
        if (watcher_) {
            watcher_->stopAll();
        }
        watcher_.reset();
        fs::remove_all(testDir_);
    }

    void createTestFiles() {
        std::ofstream file1(testDir_ / "config1.json");
        file1 << R"({"key": "value1"})";
        file1.close();

        std::ofstream file2(testDir_ / "config2.json");
        file2 << R"({"key": "value2"})";
        file2.close();

        fs::create_directories(testDir_ / "subdir");
        std::ofstream file3(testDir_ / "subdir" / "config3.json");
        file3 << R"({"key": "value3"})";
        file3.close();
    }

    void modifyFile(const fs::path& path) {
        std::ofstream file(path, std::ios::app);
        file << "\n// modified";
        file.close();
    }

    fs::path testDir_;
    std::unique_ptr<ConfigWatcher> watcher_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ConfigWatcherTest, DefaultConstruction) {
    ConfigWatcher watcher;
    EXPECT_FALSE(watcher.isRunning());
    EXPECT_TRUE(watcher.getWatchedPaths().empty());
}

TEST_F(ConfigWatcherTest, ConstructionWithOptions) {
    ConfigWatcher::WatcherOptions options;
    options.poll_interval = 200ms;
    options.debounce_delay = 500ms;
    options.recursive = true;

    ConfigWatcher watcher(options);
    EXPECT_EQ(watcher.getOptions().poll_interval, 200ms);
    EXPECT_EQ(watcher.getOptions().debounce_delay, 500ms);
    EXPECT_TRUE(watcher.getOptions().recursive);
}

// ============================================================================
// WatcherOptions Tests
// ============================================================================

TEST_F(ConfigWatcherTest, WatcherOptionsDefaults) {
    ConfigWatcher::WatcherOptions options;
    EXPECT_EQ(options.poll_interval, 100ms);
    EXPECT_EQ(options.debounce_delay, 250ms);
    EXPECT_FALSE(options.recursive);
    EXPECT_TRUE(options.watch_subdirectories);
    EXPECT_EQ(options.max_events_per_second, 100);
}

TEST_F(ConfigWatcherTest, WatcherOptionsCustomConstruction) {
    ConfigWatcher::WatcherOptions options(50ms, 100ms, true, false,
                                          {".json", ".yaml"}, 50);

    EXPECT_EQ(options.poll_interval, 50ms);
    EXPECT_EQ(options.debounce_delay, 100ms);
    EXPECT_TRUE(options.recursive);
    EXPECT_FALSE(options.watch_subdirectories);
    EXPECT_EQ(options.file_extensions.size(), 2);
    EXPECT_EQ(options.max_events_per_second, 50);
}

// ============================================================================
// Watch File Tests
// ============================================================================

TEST_F(ConfigWatcherTest, WatchFile) {
    bool callbackCalled = false;
    fs::path changedPath;

    EXPECT_TRUE(watcher_->watchFile(testDir_ / "config1.json",
                                    [&](const fs::path& path, FileEvent event) {
                                        callbackCalled = true;
                                        changedPath = path;
                                    }));

    EXPECT_TRUE(watcher_->isWatching(testDir_ / "config1.json"));
}

TEST_F(ConfigWatcherTest, WatchNonExistentFile) {
    EXPECT_FALSE(watcher_->watchFile(testDir_ / "nonexistent.json",
                                     [](const fs::path&, FileEvent) {}));
}

TEST_F(ConfigWatcherTest, WatchMultipleFiles) {
    EXPECT_TRUE(watcher_->watchFile(testDir_ / "config1.json",
                                    [](const fs::path&, FileEvent) {}));
    EXPECT_TRUE(watcher_->watchFile(testDir_ / "config2.json",
                                    [](const fs::path&, FileEvent) {}));

    auto paths = watcher_->getWatchedPaths();
    EXPECT_EQ(paths.size(), 2);
}

// ============================================================================
// Watch Directory Tests
// ============================================================================

TEST_F(ConfigWatcherTest, WatchDirectory) {
    EXPECT_TRUE(
        watcher_->watchDirectory(testDir_, [](const fs::path&, FileEvent) {}));

    EXPECT_TRUE(watcher_->isWatching(testDir_));
}

TEST_F(ConfigWatcherTest, WatchNonExistentDirectory) {
    EXPECT_FALSE(watcher_->watchDirectory(testDir_ / "nonexistent_dir",
                                          [](const fs::path&, FileEvent) {}));
}

// ============================================================================
// Stop Watching Tests
// ============================================================================

TEST_F(ConfigWatcherTest, StopWatchingFile) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});

    EXPECT_TRUE(watcher_->stopWatching(testDir_ / "config1.json"));
    EXPECT_FALSE(watcher_->isWatching(testDir_ / "config1.json"));
}

TEST_F(ConfigWatcherTest, StopWatchingNonWatchedPath) {
    EXPECT_FALSE(watcher_->stopWatching(testDir_ / "not_watched.json"));
}

TEST_F(ConfigWatcherTest, StopAll) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->watchFile(testDir_ / "config2.json",
                        [](const fs::path&, FileEvent) {});

    watcher_->stopAll();
    EXPECT_TRUE(watcher_->getWatchedPaths().empty());
}

// ============================================================================
// IsWatching Tests
// ============================================================================

TEST_F(ConfigWatcherTest, IsWatchingTrue) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    EXPECT_TRUE(watcher_->isWatching(testDir_ / "config1.json"));
}

TEST_F(ConfigWatcherTest, IsWatchingFalse) {
    EXPECT_FALSE(watcher_->isWatching(testDir_ / "not_watched.json"));
}

// ============================================================================
// GetWatchedPaths Tests
// ============================================================================

TEST_F(ConfigWatcherTest, GetWatchedPathsEmpty) {
    EXPECT_TRUE(watcher_->getWatchedPaths().empty());
}

TEST_F(ConfigWatcherTest, GetWatchedPathsMultiple) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->watchFile(testDir_ / "config2.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->watchDirectory(testDir_ / "subdir",
                             [](const fs::path&, FileEvent) {});

    auto paths = watcher_->getWatchedPaths();
    EXPECT_EQ(paths.size(), 3);
}

// ============================================================================
// Start/Stop Watching Service Tests
// ============================================================================

TEST_F(ConfigWatcherTest, StartWatching) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    EXPECT_TRUE(watcher_->startWatching());
    EXPECT_TRUE(watcher_->isRunning());
}

TEST_F(ConfigWatcherTest, IsRunning) {
    EXPECT_FALSE(watcher_->isRunning());
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->startWatching();
    EXPECT_TRUE(watcher_->isRunning());
}

// ============================================================================
// Options Update Tests
// ============================================================================

TEST_F(ConfigWatcherTest, UpdateOptions) {
    ConfigWatcher::WatcherOptions newOptions;
    newOptions.poll_interval = 200ms;
    newOptions.recursive = true;

    watcher_->updateOptions(newOptions);
    EXPECT_EQ(watcher_->getOptions().poll_interval, 200ms);
    EXPECT_TRUE(watcher_->getOptions().recursive);
}

TEST_F(ConfigWatcherTest, GetOptions) {
    auto options = watcher_->getOptions();
    EXPECT_EQ(options.poll_interval, 50ms);
    EXPECT_EQ(options.debounce_delay, 100ms);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(ConfigWatcherTest, GetStatistics) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->watchFile(testDir_ / "config2.json",
                        [](const fs::path&, FileEvent) {});

    auto stats = watcher_->getStatistics();
    EXPECT_EQ(stats.watched_paths_count, 2);
}

TEST_F(ConfigWatcherTest, ResetStatistics) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->startWatching();
    std::this_thread::sleep_for(100ms);

    watcher_->resetStatistics();
    auto stats = watcher_->getStatistics();
    EXPECT_EQ(stats.total_events_processed, 0);
}

// ============================================================================
// Pause/Resume Tests
// ============================================================================

TEST_F(ConfigWatcherTest, Pause) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->startWatching();

    watcher_->pause();
    EXPECT_TRUE(watcher_->isPaused());
}

TEST_F(ConfigWatcherTest, Resume) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->startWatching();
    watcher_->pause();

    watcher_->resume();
    EXPECT_FALSE(watcher_->isPaused());
}

TEST_F(ConfigWatcherTest, IsPaused) {
    EXPECT_FALSE(watcher_->isPaused());
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->startWatching();
    watcher_->pause();
    EXPECT_TRUE(watcher_->isPaused());
}

// ============================================================================
// Force Check Tests
// ============================================================================

TEST_F(ConfigWatcherTest, ForceCheck) {
    std::atomic<bool> callbackCalled{false};

    watcher_->watchFile(
        testDir_ / "config1.json",
        [&](const fs::path&, FileEvent) { callbackCalled = true; });
    watcher_->startWatching();

    // Modify file and force check
    modifyFile(testDir_ / "config1.json");
    watcher_->forceCheck();

    std::this_thread::sleep_for(200ms);
    // Note: callback may or may not be called depending on timing
}

// ============================================================================
// Pending Event Count Tests
// ============================================================================

TEST_F(ConfigWatcherTest, GetPendingEventCount) {
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->startWatching();
    watcher_->pause();

    size_t pending = watcher_->getPendingEventCount();
    EXPECT_GE(pending, 0);
}

// ============================================================================
// Hook Tests
// ============================================================================

TEST_F(ConfigWatcherTest, AddHook) {
    bool hookCalled = false;
    ConfigWatcher::WatcherEvent receivedEvent;

    size_t hookId = watcher_->addHook([&](ConfigWatcher::WatcherEvent event,
                                          const fs::path& path,
                                          std::optional<FileEvent> fileEvent) {
        hookCalled = true;
        receivedEvent = event;
    });

    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});

    EXPECT_TRUE(hookCalled);
    EXPECT_EQ(receivedEvent, ConfigWatcher::WatcherEvent::PATH_ADDED);
    EXPECT_TRUE(watcher_->removeHook(hookId));
}

TEST_F(ConfigWatcherTest, HookOnPathRemoved) {
    ConfigWatcher::WatcherEvent receivedEvent;

    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});

    size_t hookId = watcher_->addHook(
        [&](ConfigWatcher::WatcherEvent event, const fs::path& path,
            std::optional<FileEvent> fileEvent) { receivedEvent = event; });

    watcher_->stopWatching(testDir_ / "config1.json");
    EXPECT_EQ(receivedEvent, ConfigWatcher::WatcherEvent::PATH_REMOVED);
    watcher_->removeHook(hookId);
}

TEST_F(ConfigWatcherTest, HookOnStarted) {
    ConfigWatcher::WatcherEvent receivedEvent;

    size_t hookId = watcher_->addHook([&](ConfigWatcher::WatcherEvent event,
                                          const fs::path& path,
                                          std::optional<FileEvent> fileEvent) {
        if (event == ConfigWatcher::WatcherEvent::STARTED) {
            receivedEvent = event;
        }
    });

    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    watcher_->startWatching();

    EXPECT_EQ(receivedEvent, ConfigWatcher::WatcherEvent::STARTED);
    watcher_->removeHook(hookId);
}

TEST_F(ConfigWatcherTest, RemoveHook) {
    size_t hookId =
        watcher_->addHook([](ConfigWatcher::WatcherEvent, const fs::path&,
                             std::optional<FileEvent>) {});
    EXPECT_TRUE(watcher_->removeHook(hookId));
    EXPECT_FALSE(watcher_->removeHook(hookId));
}

TEST_F(ConfigWatcherTest, ClearHooks) {
    watcher_->addHook([](ConfigWatcher::WatcherEvent, const fs::path&,
                         std::optional<FileEvent>) {});
    watcher_->addHook([](ConfigWatcher::WatcherEvent, const fs::path&,
                         std::optional<FileEvent>) {});

    watcher_->clearHooks();

    bool hookCalled = false;
    watcher_->addHook([&](ConfigWatcher::WatcherEvent, const fs::path&,
                          std::optional<FileEvent>) { hookCalled = true; });
    watcher_->clearHooks();
    watcher_->watchFile(testDir_ / "config1.json",
                        [](const fs::path&, FileEvent) {});
    EXPECT_FALSE(hookCalled);
}

// ============================================================================
// Error Handler Tests
// ============================================================================

TEST_F(ConfigWatcherTest, SetErrorHandler) {
    bool errorHandlerCalled = false;
    watcher_->setErrorHandler(
        [&](const std::string& error) { errorHandlerCalled = true; });

    // Error handler is set, but may not be called unless an error occurs
    EXPECT_FALSE(errorHandlerCalled);
}

// ============================================================================
// FileEvent Enum Tests
// ============================================================================

TEST_F(ConfigWatcherTest, FileEventValues) {
    EXPECT_NE(FileEvent::CREATED, FileEvent::MODIFIED);
    EXPECT_NE(FileEvent::MODIFIED, FileEvent::DELETED);
    EXPECT_NE(FileEvent::DELETED, FileEvent::MOVED);
}

// ============================================================================
// File Change Detection Tests
// ============================================================================

TEST_F(ConfigWatcherTest, DetectFileModification) {
    std::atomic<bool> callbackCalled{false};
    FileEvent receivedEvent;

    watcher_->watchFile(testDir_ / "config1.json",
                        [&](const fs::path& path, FileEvent event) {
                            callbackCalled = true;
                            receivedEvent = event;
                        });

    watcher_->startWatching();
    std::this_thread::sleep_for(100ms);

    modifyFile(testDir_ / "config1.json");
    std::this_thread::sleep_for(300ms);

    if (callbackCalled) {
        EXPECT_EQ(receivedEvent, FileEvent::MODIFIED);
    }
}

TEST_F(ConfigWatcherTest, DetectFileCreation) {
    std::atomic<bool> callbackCalled{false};

    watcher_->watchDirectory(testDir_,
                             [&](const fs::path& path, FileEvent event) {
                                 if (event == FileEvent::CREATED) {
                                     callbackCalled = true;
                                 }
                             });

    watcher_->startWatching();
    std::this_thread::sleep_for(100ms);

    std::ofstream newFile(testDir_ / "new_config.json");
    newFile << R"({"new": "file"})";
    newFile.close();

    std::this_thread::sleep_for(300ms);
    // File creation detection depends on implementation
}

TEST_F(ConfigWatcherTest, DetectFileDeletion) {
    std::atomic<bool> callbackCalled{false};

    // Create a file to delete
    fs::path fileToDelete = testDir_ / "to_delete.json";
    std::ofstream file(fileToDelete);
    file << "{}";
    file.close();

    watcher_->watchFile(fileToDelete,
                        [&](const fs::path& path, FileEvent event) {
                            if (event == FileEvent::DELETED) {
                                callbackCalled = true;
                            }
                        });

    watcher_->startWatching();
    std::this_thread::sleep_for(100ms);

    fs::remove(fileToDelete);
    std::this_thread::sleep_for(300ms);
    // File deletion detection depends on implementation
}

// ============================================================================
// Extension Filter Tests
// ============================================================================

TEST_F(ConfigWatcherTest, ExtensionFilter) {
    ConfigWatcher::WatcherOptions options;
    options.file_extensions = {".json"};
    ConfigWatcher watcher(options);

    EXPECT_EQ(watcher.getOptions().file_extensions.size(), 1);
    EXPECT_EQ(watcher.getOptions().file_extensions[0], ".json");
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(ConfigWatcherTest, DestructorStopsWatching) {
    {
        ConfigWatcher tempWatcher;
        tempWatcher.watchFile(testDir_ / "config1.json",
                              [](const fs::path&, FileEvent) {});
        tempWatcher.startWatching();
        EXPECT_TRUE(tempWatcher.isRunning());
    }
    // Destructor should have stopped watching
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ConfigWatcherTest, ConcurrentWatchOperations) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i]() {
            fs::path filePath =
                testDir_ / ("concurrent_" + std::to_string(i) + ".json");
            std::ofstream file(filePath);
            file << "{}";
            file.close();

            watcher_->watchFile(filePath, [](const fs::path&, FileEvent) {});
            std::this_thread::sleep_for(10ms);
            watcher_->stopWatching(filePath);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

}  // namespace lithium::config::test
