#ifndef LITHIUM_ADDON_TEST_TRACKER_HPP
#define LITHIUM_ADDON_TEST_TRACKER_HPP

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include "atom/type/json.hpp"
#include "components/tracker.hpp"

namespace lithium::test {

class FileTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directory("test_directory");
        const std::array<std::string, 1> fileTypes = {"txt"};
        tracker = std::make_unique<FileTracker>(
            "test_directory", "test.json",
            std::span<const std::string>(fileTypes), true);
    }

    void TearDown() override {
        std::filesystem::remove_all("test_directory");
        std::filesystem::remove("test.json");
        tracker.reset();
    }

    std::unique_ptr<FileTracker> tracker;
};

TEST_F(FileTrackerTest, ScanDirectory) {
    std::ofstream("test_directory/test_file.txt") << "Test content";
    tracker->scan();
    auto differences = tracker->getDifferences();
    EXPECT_TRUE(differences.empty());
}

TEST_F(FileTrackerTest, CompareDirectory) {
    std::ofstream("test_directory/test_file.txt") << "Test content";
    tracker->scan();
    std::ofstream("test_directory/test_file.txt") << "Modified content";
    tracker->compare();
    auto differences = tracker->getDifferences();
    EXPECT_FALSE(differences.empty());
    EXPECT_EQ(differences["test_directory/test_file.txt"]["status"],
              "modified");
}

TEST_F(FileTrackerTest, LogDifferences) {
    std::ofstream("test_directory/test_file.txt") << "Test content";
    tracker->scan();
    std::ofstream("test_directory/test_file.txt") << "Modified content";
    tracker->compare();
    tracker->logDifferences("log.txt");
    std::ifstream logFile("log.txt");
    std::string logContent((std::istreambuf_iterator<char>(logFile)),
                           std::istreambuf_iterator<char>());
    EXPECT_NE(logContent.find("test_directory/test_file.txt"),
              std::string::npos);
    std::filesystem::remove("log.txt");
}

TEST_F(FileTrackerTest, RecoverFiles) {
    std::ofstream("test_directory/test_file.txt") << "Test content";
    tracker->scan();
    std::filesystem::remove("test_directory/test_file.txt");
    tracker->recover("test.json");
    EXPECT_TRUE(std::filesystem::exists("test_directory/test_file.txt"));
}

TEST_F(FileTrackerTest, AsyncScan) {
    std::ofstream("test_directory/test_file.txt") << "Test content";
    auto future = tracker->asyncScan();
    future.wait();
    auto differences = tracker->getDifferences();
    EXPECT_TRUE(differences.empty());
}

TEST_F(FileTrackerTest, AsyncCompare) {
    std::ofstream("test_directory/test_file.txt") << "Test content";
    tracker->scan();
    std::ofstream("test_directory/test_file.txt") << "Modified content";
    auto future = tracker->asyncCompare();
    future.wait();
    auto differences = tracker->getDifferences();
    EXPECT_FALSE(differences.empty());
    EXPECT_EQ(differences["test_directory/test_file.txt"]["status"],
              "modified");
}

TEST_F(FileTrackerTest, GetTrackedFileTypes) {
    auto fileTypes = tracker->getTrackedFileTypes();
    EXPECT_EQ(fileTypes.size(), 1);
    EXPECT_EQ(fileTypes[0], "txt");
}

TEST_F(FileTrackerTest, GetFileInfo) {
    std::ofstream("test_directory/test_file.txt") << "Test content";
    tracker->scan();
    auto fileInfo = tracker->getFileInfo("test_directory/test_file.txt");
    EXPECT_TRUE(fileInfo.has_value());
    EXPECT_EQ((*fileInfo)["type"], ".txt");
}

TEST_F(FileTrackerTest, AddFileType) {
    tracker->addFileType("log");
    auto fileTypes = tracker->getTrackedFileTypes();
    EXPECT_EQ(fileTypes.size(), 2);
    EXPECT_EQ(fileTypes[1], "log");
}

TEST_F(FileTrackerTest, RemoveFileType) {
    tracker->removeFileType("txt");
    auto fileTypes = tracker->getTrackedFileTypes();
    EXPECT_TRUE(fileTypes.empty());
}

TEST_F(FileTrackerTest, SetEncryptionKey) {
    tracker->setEncryptionKey("test_key");
    std::ofstream("test_directory/test_file.txt") << "Test content";
    tracker->scan();
    auto differences = tracker->getDifferences();
    EXPECT_TRUE(differences.empty());
}

TEST_F(FileTrackerTest, StartStopWatching) {
    tracker->startWatching();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    tracker->stopWatching();
    EXPECT_FALSE(tracker->getStatistics()["watching"]);
}

TEST_F(FileTrackerTest, SetChangeCallback) {
    bool callbackCalled = false;
    tracker->setChangeCallback(
        [&callbackCalled](const fs::path&, const std::string&) {
            callbackCalled = true;
        });
    tracker->startWatching();
    std::ofstream("test_directory/test_file.txt") << "Test content";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    tracker->stopWatching();
    EXPECT_TRUE(callbackCalled);
}

TEST_F(FileTrackerTest, BatchProcess) {
    std::vector<fs::path> files = {"test_directory/test_file1.txt",
                                   "test_directory/test_file2.txt"};
    for (const auto& file : files) {
        std::ofstream(file) << "Test content";
    }
    tracker->batchProcess(files, [](const fs::path& file) {
        std::ofstream(file, std::ios_base::app) << " Processed";
    });
    for (const auto& file : files) {
        std::ifstream inFile(file);
        std::string content((std::istreambuf_iterator<char>(inFile)),
                            std::istreambuf_iterator<char>());
        EXPECT_NE(content.find("Processed"), std::string::npos);
    }
}

TEST_F(FileTrackerTest, GetStatistics) {
    std::ofstream("test_directory/test_file.txt") << "Test content";
    tracker->scan();
    auto stats = tracker->getStatistics();
    EXPECT_EQ(stats["total_files"], 1);
}

TEST_F(FileTrackerTest, EnableCache) {
    tracker->enableCache(true);
    EXPECT_TRUE(tracker->getStatistics()["cache_enabled"]);
    tracker->enableCache(false);
    EXPECT_FALSE(tracker->getStatistics()["cache_enabled"]);
}

TEST_F(FileTrackerTest, SetCacheSize) {
    tracker->setCacheSize(10);
    EXPECT_EQ(tracker->getStatistics()["cache_size"], 0);
}

TEST_F(FileTrackerTest, GetCurrentStats) {
    std::ofstream("test_directory/test_file.txt") << "Test content";
    tracker->scan();
    auto stats = tracker->getCurrentStats();
    EXPECT_EQ(stats.totalFiles, 1);
}

}  // namespace lithium::test

#endif  // LITHIUM_ADDON_TEST_TRACKER_HPP
