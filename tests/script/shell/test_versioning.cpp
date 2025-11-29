/*
 * test_versioning.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_versioning.cpp
 * @brief Comprehensive tests for Version Manager
 */

#include <gtest/gtest.h>
#include "script/shell/versioning.hpp"

#include <chrono>
#include <thread>

using namespace lithium::shell;

// =============================================================================
// Test Fixture
// =============================================================================

class VersionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<VersionManager>(5);
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<VersionManager> manager_;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(VersionManagerTest, DefaultConstruction) {
    VersionManager manager;
    EXPECT_EQ(manager.getMaxVersions(), 10); // Default max versions
}

TEST_F(VersionManagerTest, ConstructionWithMaxVersions) {
    VersionManager manager(20);
    EXPECT_EQ(manager.getMaxVersions(), 20);
}

TEST_F(VersionManagerTest, MoveConstruction) {
    manager_->saveVersion("script", "content");
    VersionManager moved(std::move(*manager_));
    EXPECT_TRUE(moved.hasVersions("script"));
}

TEST_F(VersionManagerTest, MoveAssignment) {
    manager_->saveVersion("script", "content");
    VersionManager other;
    other = std::move(*manager_);
    EXPECT_TRUE(other.hasVersions("script"));
}

// =============================================================================
// Save Version Tests
// =============================================================================

TEST_F(VersionManagerTest, SaveVersionReturnsVersionNumber) {
    auto version = manager_->saveVersion("script", "content");
    EXPECT_EQ(version, 1);
}

TEST_F(VersionManagerTest, SaveVersionIncrementsNumber) {
    auto v1 = manager_->saveVersion("script", "content1");
    auto v2 = manager_->saveVersion("script", "content2");
    auto v3 = manager_->saveVersion("script", "content3");

    EXPECT_EQ(v1, 1);
    EXPECT_EQ(v2, 2);
    EXPECT_EQ(v3, 3);
}

TEST_F(VersionManagerTest, SaveVersionWithAuthor) {
    manager_->saveVersion("script", "content", "author_name");
    auto version = manager_->getLatestVersion("script");

    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version->author, "author_name");
}

TEST_F(VersionManagerTest, SaveVersionWithDescription) {
    manager_->saveVersion("script", "content", "author", "Fixed bug");
    auto version = manager_->getLatestVersion("script");

    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version->changeDescription, "Fixed bug");
}

TEST_F(VersionManagerTest, SaveVersionSetsTimestamp) {
    auto before = std::chrono::system_clock::now();
    manager_->saveVersion("script", "content");
    auto after = std::chrono::system_clock::now();

    auto version = manager_->getLatestVersion("script");
    ASSERT_TRUE(version.has_value());
    EXPECT_GE(version->timestamp, before);
    EXPECT_LE(version->timestamp, after);
}

// =============================================================================
// Get Version Tests
// =============================================================================

TEST_F(VersionManagerTest, GetVersionExisting) {
    manager_->saveVersion("script", "content1");
    manager_->saveVersion("script", "content2");

    auto version = manager_->getVersion("script", 1);
    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version->content, "content1");
}

TEST_F(VersionManagerTest, GetVersionNonexistent) {
    manager_->saveVersion("script", "content");
    auto version = manager_->getVersion("script", 999);
    EXPECT_FALSE(version.has_value());
}

TEST_F(VersionManagerTest, GetVersionNonexistentScript) {
    auto version = manager_->getVersion("nonexistent", 1);
    EXPECT_FALSE(version.has_value());
}

TEST_F(VersionManagerTest, GetLatestVersion) {
    manager_->saveVersion("script", "content1");
    manager_->saveVersion("script", "content2");
    manager_->saveVersion("script", "content3");

    auto version = manager_->getLatestVersion("script");
    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version->content, "content3");
    EXPECT_EQ(version->versionNumber, 3);
}

TEST_F(VersionManagerTest, GetLatestVersionNonexistentScript) {
    auto version = manager_->getLatestVersion("nonexistent");
    EXPECT_FALSE(version.has_value());
}

// =============================================================================
// Rollback Tests
// =============================================================================

TEST_F(VersionManagerTest, RollbackToVersion) {
    manager_->saveVersion("script", "content1");
    manager_->saveVersion("script", "content2");
    manager_->saveVersion("script", "content3");

    auto content = manager_->rollback("script", 1);
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(content.value(), "content1");
}

TEST_F(VersionManagerTest, RollbackToNonexistentVersion) {
    manager_->saveVersion("script", "content");
    auto content = manager_->rollback("script", 999);
    EXPECT_FALSE(content.has_value());
}

TEST_F(VersionManagerTest, RollbackNonexistentScript) {
    auto content = manager_->rollback("nonexistent", 1);
    EXPECT_FALSE(content.has_value());
}

// =============================================================================
// Version History Tests
// =============================================================================

TEST_F(VersionManagerTest, GetVersionHistory) {
    manager_->saveVersion("script", "content1");
    manager_->saveVersion("script", "content2");
    manager_->saveVersion("script", "content3");

    auto history = manager_->getVersionHistory("script");
    EXPECT_EQ(history.size(), 3);
}

TEST_F(VersionManagerTest, GetVersionHistoryOrder) {
    manager_->saveVersion("script", "content1");
    manager_->saveVersion("script", "content2");
    manager_->saveVersion("script", "content3");

    auto history = manager_->getVersionHistory("script");
    ASSERT_EQ(history.size(), 3);

    // Should be in chronological order (oldest first)
    EXPECT_EQ(history[0].versionNumber, 1);
    EXPECT_EQ(history[1].versionNumber, 2);
    EXPECT_EQ(history[2].versionNumber, 3);
}

TEST_F(VersionManagerTest, GetVersionHistoryNonexistentScript) {
    auto history = manager_->getVersionHistory("nonexistent");
    EXPECT_TRUE(history.empty());
}

// =============================================================================
// Version Count Tests
// =============================================================================

TEST_F(VersionManagerTest, GetVersionCount) {
    manager_->saveVersion("script", "content1");
    manager_->saveVersion("script", "content2");

    EXPECT_EQ(manager_->getVersionCount("script"), 2);
}

TEST_F(VersionManagerTest, GetVersionCountNonexistentScript) {
    EXPECT_EQ(manager_->getVersionCount("nonexistent"), 0);
}

// =============================================================================
// Max Versions Tests
// =============================================================================

TEST_F(VersionManagerTest, SetMaxVersions) {
    manager_->setMaxVersions(3);
    EXPECT_EQ(manager_->getMaxVersions(), 3);
}

TEST_F(VersionManagerTest, MaxVersionsPrunesOld) {
    manager_->setMaxVersions(3);

    manager_->saveVersion("script", "content1");
    manager_->saveVersion("script", "content2");
    manager_->saveVersion("script", "content3");
    manager_->saveVersion("script", "content4");
    manager_->saveVersion("script", "content5");

    EXPECT_EQ(manager_->getVersionCount("script"), 3);

    // Oldest versions should be pruned
    auto v1 = manager_->getVersion("script", 1);
    auto v2 = manager_->getVersion("script", 2);
    EXPECT_FALSE(v1.has_value());
    EXPECT_FALSE(v2.has_value());

    // Newest versions should remain
    auto v5 = manager_->getVersion("script", 5);
    EXPECT_TRUE(v5.has_value());
}

// =============================================================================
// Clear Version History Tests
// =============================================================================

TEST_F(VersionManagerTest, ClearVersionHistory) {
    manager_->saveVersion("script", "content1");
    manager_->saveVersion("script", "content2");

    manager_->clearVersionHistory("script");
    EXPECT_EQ(manager_->getVersionCount("script"), 0);
    EXPECT_FALSE(manager_->hasVersions("script"));
}

TEST_F(VersionManagerTest, ClearVersionHistoryNonexistentScript) {
    // Should not throw
    EXPECT_NO_THROW(manager_->clearVersionHistory("nonexistent"));
}

TEST_F(VersionManagerTest, ClearAllVersionHistory) {
    manager_->saveVersion("script1", "content");
    manager_->saveVersion("script2", "content");
    manager_->saveVersion("script3", "content");

    manager_->clearAllVersionHistory();

    EXPECT_FALSE(manager_->hasVersions("script1"));
    EXPECT_FALSE(manager_->hasVersions("script2"));
    EXPECT_FALSE(manager_->hasVersions("script3"));
}

// =============================================================================
// Has Versions Tests
// =============================================================================

TEST_F(VersionManagerTest, HasVersionsTrue) {
    manager_->saveVersion("script", "content");
    EXPECT_TRUE(manager_->hasVersions("script"));
}

TEST_F(VersionManagerTest, HasVersionsFalse) {
    EXPECT_FALSE(manager_->hasVersions("nonexistent"));
}

TEST_F(VersionManagerTest, HasVersionsAfterClear) {
    manager_->saveVersion("script", "content");
    manager_->clearVersionHistory("script");
    EXPECT_FALSE(manager_->hasVersions("script"));
}

// =============================================================================
// Get All Versioned Scripts Tests
// =============================================================================

TEST_F(VersionManagerTest, GetAllVersionedScripts) {
    manager_->saveVersion("script1", "content");
    manager_->saveVersion("script2", "content");
    manager_->saveVersion("script3", "content");

    auto scripts = manager_->getAllVersionedScripts();
    EXPECT_EQ(scripts.size(), 3);

    // Check all scripts are present
    std::set<std::string> scriptSet(scripts.begin(), scripts.end());
    EXPECT_TRUE(scriptSet.count("script1") > 0);
    EXPECT_TRUE(scriptSet.count("script2") > 0);
    EXPECT_TRUE(scriptSet.count("script3") > 0);
}

TEST_F(VersionManagerTest, GetAllVersionedScriptsEmpty) {
    auto scripts = manager_->getAllVersionedScripts();
    EXPECT_TRUE(scripts.empty());
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(VersionManagerTest, ConcurrentSaveVersions) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 10; ++j) {
                manager_->saveVersion("script", "content_" + std::to_string(i * 10 + j));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have at most maxVersions (5)
    EXPECT_LE(manager_->getVersionCount("script"), 5);
}

TEST_F(VersionManagerTest, ConcurrentReadWrite) {
    std::atomic<bool> running{true};

    std::thread writer([&]() {
        int i = 0;
        while (running) {
            manager_->saveVersion("script", "content_" + std::to_string(i++));
        }
    });

    std::thread reader([&]() {
        while (running) {
            manager_->getLatestVersion("script");
            manager_->getVersionHistory("script");
            manager_->getVersionCount("script");
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    writer.join();
    reader.join();

    // Just verify no crashes
    SUCCEED();
}
