// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for CelestialRepository
 */

#include <gtest/gtest.h>
#include <filesystem>
#include "target/celestial_repository.hpp"

using namespace lithium::target;
namespace fs = std::filesystem;

class CelestialRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDbPath_ = "test_celestial_repo.db";
        // Remove existing test database
        if (fs::exists(testDbPath_)) {
            fs::remove(testDbPath_);
        }
        repository_ = std::make_unique<CelestialRepository>(testDbPath_);
        repository_->initializeSchema();
    }

    void TearDown() override {
        repository_.reset();
        if (fs::exists(testDbPath_)) {
            fs::remove(testDbPath_);
        }
    }

    CelestialObjectModel createTestObject(const std::string& id) {
        CelestialObjectModel obj;
        obj.identifier = id;
        obj.type = "Galaxy";
        obj.radJ2000 = 10.6847;
        obj.decDJ2000 = 41.2689;
        obj.visualMagnitudeV = 3.44;
        return obj;
    }

    std::string testDbPath_;
    std::unique_ptr<CelestialRepository> repository_;
};

TEST_F(CelestialRepositoryTest, InitializeSchema) {
    EXPECT_TRUE(repository_->initializeSchema());
}

TEST_F(CelestialRepositoryTest, InsertAndFindById) {
    auto obj = createTestObject("M31");
    int64_t id = repository_->insert(obj);
    EXPECT_GT(id, 0);

    auto found = repository_->findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->identifier, "M31");
}

TEST_F(CelestialRepositoryTest, FindByIdentifier) {
    auto obj = createTestObject("NGC224");
    repository_->insert(obj);

    auto found = repository_->findByIdentifier("NGC224");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->identifier, "NGC224");
}

TEST_F(CelestialRepositoryTest, Update) {
    auto obj = createTestObject("M42");
    int64_t id = repository_->insert(obj);

    obj.id = id;
    obj.type = "Nebula";
    EXPECT_TRUE(repository_->update(obj));

    auto found = repository_->findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->type, "Nebula");
}

TEST_F(CelestialRepositoryTest, Remove) {
    auto obj = createTestObject("M45");
    int64_t id = repository_->insert(obj);
    EXPECT_TRUE(repository_->remove(id));

    auto found = repository_->findById(id);
    EXPECT_FALSE(found.has_value());
}

TEST_F(CelestialRepositoryTest, SearchByName) {
    repository_->insert(createTestObject("M31"));
    repository_->insert(createTestObject("M32"));
    repository_->insert(createTestObject("M33"));

    auto results = repository_->searchByName("M3%", 10);
    EXPECT_GE(results.size(), 3);
}

TEST_F(CelestialRepositoryTest, SearchByCoordinates) {
    auto obj = createTestObject("M31");
    obj.radJ2000 = 10.6847;
    obj.decDJ2000 = 41.2689;
    repository_->insert(obj);

    auto results = repository_->searchByCoordinates(10.0, 41.0, 5.0, 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(CelestialRepositoryTest, GetByType) {
    repository_->insert(createTestObject("M31"));

    auto obj2 = createTestObject("M42");
    obj2.type = "Nebula";
    repository_->insert(obj2);

    auto galaxies = repository_->getByType("Galaxy", 10);
    EXPECT_GE(galaxies.size(), 1);
    for (const auto& g : galaxies) {
        EXPECT_EQ(g.type, "Galaxy");
    }
}

TEST_F(CelestialRepositoryTest, GetByMagnitudeRange) {
    auto obj = createTestObject("M31");
    obj.visualMagnitudeV = 3.44;
    repository_->insert(obj);

    auto results = repository_->getByMagnitudeRange(0.0, 5.0, 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(CelestialRepositoryTest, BatchInsert) {
    std::vector<CelestialObjectModel> objects;
    for (int i = 0; i < 10; ++i) {
        objects.push_back(createTestObject("BATCH" + std::to_string(i)));
    }

    int inserted = repository_->batchInsert(objects);
    EXPECT_EQ(inserted, 10);
}

TEST_F(CelestialRepositoryTest, Count) {
    repository_->insert(createTestObject("M31"));
    repository_->insert(createTestObject("M32"));

    EXPECT_GE(repository_->count(), 2);
}

TEST_F(CelestialRepositoryTest, CountByType) {
    repository_->insert(createTestObject("M31"));

    auto obj2 = createTestObject("M42");
    obj2.type = "Nebula";
    repository_->insert(obj2);

    auto counts = repository_->countByType();
    EXPECT_GE(counts["Galaxy"], 1);
}

TEST_F(CelestialRepositoryTest, IncrementClickCount) {
    auto obj = createTestObject("M31");
    repository_->insert(obj);

    EXPECT_TRUE(repository_->incrementClickCount("M31"));

    auto found = repository_->findByIdentifier("M31");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->clickCount, 1);
}

TEST_F(CelestialRepositoryTest, GetMostPopular) {
    auto obj = createTestObject("M31");
    repository_->insert(obj);
    repository_->incrementClickCount("M31");
    repository_->incrementClickCount("M31");

    auto popular = repository_->getMostPopular(10);
    EXPECT_GE(popular.size(), 1);
}

TEST_F(CelestialRepositoryTest, Autocomplete) {
    repository_->insert(createTestObject("M31"));
    repository_->insert(createTestObject("M32"));
    repository_->insert(createTestObject("NGC224"));

    auto suggestions = repository_->autocomplete("M3", 10);
    EXPECT_GE(suggestions.size(), 2);
}

TEST_F(CelestialRepositoryTest, FuzzySearch) {
    repository_->insert(createTestObject("M31"));

    auto results = repository_->fuzzySearch("M30", 2, 10);
    EXPECT_GE(results.size(), 1);
}

TEST_F(CelestialRepositoryTest, ClearAll) {
    repository_->insert(createTestObject("M31"));
    repository_->clearAll(true);
    EXPECT_EQ(repository_->count(), 0);
}
