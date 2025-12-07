// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for CachedRepository
 */

#include <gtest/gtest.h>
#include <filesystem>
#include "target/repository/cached_repository.hpp"

using namespace lithium::target::repository;
namespace fs = std::filesystem;

class CachedRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDbPath_ = "test_cached_repo.db";
        if (fs::exists(testDbPath_)) {
            fs::remove(testDbPath_);
        }
        repository_ = std::make_unique<CachedRepository>(testDbPath_, 100);
    }

    void TearDown() override {
        repository_.reset();
        if (fs::exists(testDbPath_)) {
            fs::remove(testDbPath_);
        }
    }

    std::string testDbPath_;
    std::unique_ptr<CachedRepository> repository_;
};

TEST_F(CachedRepositoryTest, CacheHit) {
    CelestialObjectModel obj;
    obj.identifier = "M31";
    obj.type = "Galaxy";

    int64_t id = repository_->insert(obj);

    // First access - cache miss
    auto found1 = repository_->findById(id);
    ASSERT_TRUE(found1.has_value());

    // Second access - should be cache hit
    auto found2 = repository_->findById(id);
    ASSERT_TRUE(found2.has_value());
    EXPECT_EQ(found1->identifier, found2->identifier);
}

TEST_F(CachedRepositoryTest, CacheInvalidationOnUpdate) {
    CelestialObjectModel obj;
    obj.identifier = "M42";
    obj.type = "Nebula";

    int64_t id = repository_->insert(obj);
    auto found = repository_->findById(id);

    // Update should invalidate cache
    obj.id = id;
    obj.type = "Emission Nebula";
    repository_->update(obj);

    auto updated = repository_->findById(id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->type, "Emission Nebula");
}

TEST_F(CachedRepositoryTest, CacheInvalidationOnDelete) {
    CelestialObjectModel obj;
    obj.identifier = "M45";

    int64_t id = repository_->insert(obj);
    repository_->findById(id);  // Populate cache

    repository_->remove(id);

    auto found = repository_->findById(id);
    EXPECT_FALSE(found.has_value());
}

TEST_F(CachedRepositoryTest, ClearCache) {
    CelestialObjectModel obj;
    obj.identifier = "M31";

    repository_->insert(obj);
    repository_->findByIdentifier("M31");  // Populate cache

    repository_->clearCache();

    // Should still work after cache clear
    auto found = repository_->findByIdentifier("M31");
    ASSERT_TRUE(found.has_value());
}

TEST_F(CachedRepositoryTest, CacheSize) {
    // Insert more items than cache size
    for (int i = 0; i < 150; ++i) {
        CelestialObjectModel obj;
        obj.identifier = "OBJ" + std::to_string(i);
        repository_->insert(obj);
    }

    // Cache should evict old entries
    auto stats = repository_->getCacheStats();
    EXPECT_FALSE(stats.empty());
}
