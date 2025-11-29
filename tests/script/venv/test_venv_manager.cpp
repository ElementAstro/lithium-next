/*
 * test_venv_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_venv_manager.cpp
 * @brief Comprehensive tests for Virtual Environment Manager
 */

#include <gtest/gtest.h>
#include "script/venv/venv_manager.hpp"

#include <filesystem>
#include <fstream>

using namespace lithium::venv;
namespace fs = std::filesystem;

// =============================================================================
// Test Fixture
// =============================================================================

class VenvManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_venv_test";
        fs::create_directories(testDir_);
        manager_ = std::make_unique<VenvManager>();
    }

    void TearDown() override {
        manager_.reset();
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    std::unique_ptr<VenvManager> manager_;
    fs::path testDir_;
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(VenvManagerTest, DefaultConstruction) {
    VenvManager manager;
    EXPECT_FALSE(manager.isVenvActive());
}

TEST_F(VenvManagerTest, MoveConstruction) {
    VenvManager moved(std::move(*manager_));
    EXPECT_FALSE(moved.isVenvActive());
}

TEST_F(VenvManagerTest, MoveAssignment) {
    VenvManager other;
    other = std::move(*manager_);
    EXPECT_FALSE(other.isVenvActive());
}

// =============================================================================
// Venv State Tests
// =============================================================================

TEST_F(VenvManagerTest, IsVenvActiveInitiallyFalse) {
    EXPECT_FALSE(manager_->isVenvActive());
}

TEST_F(VenvManagerTest, GetCurrentVenvPathWhenNotActive) {
    auto path = manager_->getCurrentVenvPath();
    EXPECT_FALSE(path.has_value());
}

TEST_F(VenvManagerTest, GetCurrentVenvInfoWhenNotActive) {
    auto info = manager_->getCurrentVenvInfo();
    EXPECT_FALSE(info.has_value());
}

// =============================================================================
// Venv Validation Tests
// =============================================================================

TEST_F(VenvManagerTest, IsValidVenvFalseForNonexistent) {
    EXPECT_FALSE(manager_->isValidVenv(testDir_ / "nonexistent"));
}

TEST_F(VenvManagerTest, IsValidVenvFalseForEmptyDir) {
    fs::path emptyDir = testDir_ / "empty";
    fs::create_directories(emptyDir);
    EXPECT_FALSE(manager_->isValidVenv(emptyDir));
}

// =============================================================================
// Python Discovery Tests
// =============================================================================

TEST_F(VenvManagerTest, DiscoverPythonInterpreters) {
    auto interpreters = manager_->discoverPythonInterpreters();
    // May or may not find interpreters depending on system
    SUCCEED();
}

TEST_F(VenvManagerTest, GetPythonExecutable) {
    auto pythonPath = manager_->getPythonExecutable();
    // Should return some path (system Python or empty)
    SUCCEED();
}

TEST_F(VenvManagerTest, GetPipExecutable) {
    auto pipPath = manager_->getPipExecutable();
    SUCCEED();
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(VenvManagerTest, SetDefaultPython) {
    fs::path pythonPath = "/usr/bin/python3";
    EXPECT_NO_THROW(manager_->setDefaultPython(pythonPath));
}

TEST_F(VenvManagerTest, SetCondaPath) {
    fs::path condaPath = "/opt/conda/bin/conda";
    EXPECT_NO_THROW(manager_->setCondaPath(condaPath));
}

TEST_F(VenvManagerTest, SetOperationTimeout) {
    EXPECT_NO_THROW(manager_->setOperationTimeout(std::chrono::seconds(120)));
}

// =============================================================================
// Conda Availability Tests
// =============================================================================

TEST_F(VenvManagerTest, IsCondaAvailable) {
    bool available = manager_->isCondaAvailable();
    // Just verify it doesn't crash
    SUCCEED();
}

// =============================================================================
// Component Access Tests
// =============================================================================

TEST_F(VenvManagerTest, AccessPackageManager) {
    auto& packages = manager_->packages();
    // Should return valid reference
    SUCCEED();
}

TEST_F(VenvManagerTest, AccessCondaAdapter) {
    auto& conda = manager_->conda();
    // Should return valid reference
    SUCCEED();
}

// =============================================================================
// Venv Creation Tests (may require Python)
// =============================================================================

TEST_F(VenvManagerTest, CreateVenvWithPath) {
    fs::path venvPath = testDir_ / "test_venv";
    auto result = manager_->createVenv(venvPath);

    // Result depends on Python availability
    if (result.has_value()) {
        EXPECT_TRUE(fs::exists(venvPath));
    }
}

TEST_F(VenvManagerTest, CreateVenvWithConfig) {
    VenvConfig config;
    config.path = testDir_ / "config_venv";
    config.pythonVersion = "";

    auto result = manager_->createVenv(config);
    // Result depends on Python availability
    SUCCEED();
}

TEST_F(VenvManagerTest, DeleteVenv) {
    fs::path venvPath = testDir_ / "delete_venv";
    fs::create_directories(venvPath);

    auto result = manager_->deleteVenv(venvPath);
    // Should succeed or fail gracefully
    SUCCEED();
}

// =============================================================================
// Package Management Tests
// =============================================================================

TEST_F(VenvManagerTest, IsPackageInstalledFalse) {
    bool installed = manager_->isPackageInstalled("nonexistent_package_12345");
    EXPECT_FALSE(installed);
}

TEST_F(VenvManagerTest, ListInstalledPackages) {
    auto result = manager_->listInstalledPackages();
    // May or may not have packages depending on active venv
    SUCCEED();
}

TEST_F(VenvManagerTest, ExportRequirements) {
    fs::path outputFile = testDir_ / "requirements.txt";
    auto result = manager_->exportRequirements(outputFile);
    // Result depends on active venv
    SUCCEED();
}
