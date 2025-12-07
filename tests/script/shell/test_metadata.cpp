/*
 * test_metadata.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_metadata.cpp
 * @brief Comprehensive tests for Script Metadata Manager
 */

#include <gtest/gtest.h>
#include "script/shell/metadata.hpp"

#include <chrono>

using namespace lithium::shell;

// =============================================================================
// ScriptMetadata Tests
// =============================================================================

class ScriptMetadataTest : public ::testing::Test {};

TEST_F(ScriptMetadataTest, CreateDefault) {
    auto meta = ScriptMetadata::create();

    EXPECT_FALSE(meta.name.empty() || meta.name == "");
    EXPECT_EQ(meta.language, ScriptLanguage::Auto);
}

TEST_F(ScriptMetadataTest, TouchUpdatesTimestamp) {
    auto meta = ScriptMetadata::create();
    auto originalTime = meta.lastModified;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    meta.touch();

    EXPECT_GT(meta.lastModified, originalTime);
}

// =============================================================================
// MetadataManager Tests
// =============================================================================

class MetadataManagerTest : public ::testing::Test {
protected:
    void SetUp() override { manager_ = std::make_unique<MetadataManager>(); }

    void TearDown() override { manager_.reset(); }

    std::unique_ptr<MetadataManager> manager_;
};

TEST_F(MetadataManagerTest, SetMetadata) {
    ScriptMetadata meta = ScriptMetadata::create();
    meta.name = "test_script";

    EXPECT_NO_THROW(manager_->setMetadata("test_script", meta));
}

TEST_F(MetadataManagerTest, GetMetadataExisting) {
    ScriptMetadata meta = ScriptMetadata::create();
    meta.name = "test_script";
    meta.language = ScriptLanguage::Python;

    manager_->setMetadata("test_script", meta);

    auto retrieved = manager_->getMetadata("test_script");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "test_script");
    EXPECT_EQ(retrieved->language, ScriptLanguage::Python);
}

TEST_F(MetadataManagerTest, GetMetadataNonexistent) {
    auto retrieved = manager_->getMetadata("nonexistent");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(MetadataManagerTest, RemoveMetadata) {
    ScriptMetadata meta = ScriptMetadata::create();
    manager_->setMetadata("test_script", meta);

    manager_->removeMetadata("test_script");

    auto retrieved = manager_->getMetadata("test_script");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(MetadataManagerTest, RemoveMetadataNonexistent) {
    // Should not throw
    EXPECT_NO_THROW(manager_->removeMetadata("nonexistent"));
}

TEST_F(MetadataManagerTest, HasMetadataTrue) {
    ScriptMetadata meta = ScriptMetadata::create();
    manager_->setMetadata("test_script", meta);

    EXPECT_TRUE(manager_->hasMetadata("test_script"));
}

TEST_F(MetadataManagerTest, HasMetadataFalse) {
    EXPECT_FALSE(manager_->hasMetadata("nonexistent"));
}

TEST_F(MetadataManagerTest, UpdateMetadata) {
    ScriptMetadata meta = ScriptMetadata::create();
    meta.language = ScriptLanguage::Shell;
    manager_->setMetadata("test_script", meta);

    meta.language = ScriptLanguage::Python;
    manager_->setMetadata("test_script", meta);

    auto retrieved = manager_->getMetadata("test_script");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->language, ScriptLanguage::Python);
}

TEST_F(MetadataManagerTest, MultipleScripts) {
    ScriptMetadata meta1 = ScriptMetadata::create();
    meta1.name = "script1";
    ScriptMetadata meta2 = ScriptMetadata::create();
    meta2.name = "script2";

    manager_->setMetadata("script1", meta1);
    manager_->setMetadata("script2", meta2);

    EXPECT_TRUE(manager_->hasMetadata("script1"));
    EXPECT_TRUE(manager_->hasMetadata("script2"));
}

TEST_F(MetadataManagerTest, ClearAllMetadata) {
    ScriptMetadata meta = ScriptMetadata::create();
    manager_->setMetadata("script1", meta);
    manager_->setMetadata("script2", meta);

    manager_->clearAll();

    EXPECT_FALSE(manager_->hasMetadata("script1"));
    EXPECT_FALSE(manager_->hasMetadata("script2"));
}

TEST_F(MetadataManagerTest, GetAllScriptNames) {
    ScriptMetadata meta = ScriptMetadata::create();
    manager_->setMetadata("script1", meta);
    manager_->setMetadata("script2", meta);
    manager_->setMetadata("script3", meta);

    auto names = manager_->getAllScriptNames();
    EXPECT_EQ(names.size(), 3);
}
