// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for MemoryRepository
 */

#include <gtest/gtest.h>
#include "target/repository/memory_repository.hpp"

using namespace lithium::target::repository;

class MemoryRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        repository_ = std::make_unique<MemoryRepository>();
    }

    std::unique_ptr<MemoryRepository> repository_;
};

TEST_F(MemoryRepositoryTest, InsertAndFind) {
    CelestialObjectModel obj;
    obj.identifier = "M31";
    obj.type = "Galaxy";

    int64_t id = repository_->insert(obj);
    EXPECT_GT(id, 0);

    auto found = repository_->findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->identifier, "M31");
}

TEST_F(MemoryRepositoryTest, FindByIdentifier) {
    CelestialObjectModel obj;
    obj.identifier = "NGC224";
    repository_->insert(obj);

    auto found = repository_->findByIdentifier("NGC224");
    ASSERT_TRUE(found.has_value());
}

TEST_F(MemoryRepositoryTest, Update) {
    CelestialObjectModel obj;
    obj.identifier = "M42";
    int64_t id = repository_->insert(obj);

    obj.id = id;
    obj.type = "Nebula";
    EXPECT_TRUE(repository_->update(obj));

    auto found = repository_->findById(id);
    EXPECT_EQ(found->type, "Nebula");
}

TEST_F(MemoryRepositoryTest, Remove) {
    CelestialObjectModel obj;
    obj.identifier = "M45";
    int64_t id = repository_->insert(obj);

    EXPECT_TRUE(repository_->remove(id));
    EXPECT_FALSE(repository_->findById(id).has_value());
}

TEST_F(MemoryRepositoryTest, SearchByName) {
    CelestialObjectModel obj1, obj2;
    obj1.identifier = "M31";
    obj2.identifier = "M32";
    repository_->insert(obj1);
    repository_->insert(obj2);

    auto results = repository_->searchByName("M3", 10);
    EXPECT_GE(results.size(), 2);
}

TEST_F(MemoryRepositoryTest, GetAll) {
    for (int i = 0; i < 5; ++i) {
        CelestialObjectModel obj;
        obj.identifier = "OBJ" + std::to_string(i);
        repository_->insert(obj);
    }

    auto all = repository_->getAll();
    EXPECT_EQ(all.size(), 5);
}

TEST_F(MemoryRepositoryTest, Clear) {
    CelestialObjectModel obj;
    obj.identifier = "M31";
    repository_->insert(obj);

    repository_->clear();
    EXPECT_EQ(repository_->count(), 0);
}

TEST_F(MemoryRepositoryTest, Count) {
    for (int i = 0; i < 3; ++i) {
        CelestialObjectModel obj;
        obj.identifier = "OBJ" + std::to_string(i);
        repository_->insert(obj);
    }

    EXPECT_EQ(repository_->count(), 3);
}

TEST_F(MemoryRepositoryTest, ThreadSafety) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i]() {
            CelestialObjectModel obj;
            obj.identifier = "THREAD" + std::to_string(i);
            repository_->insert(obj);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(repository_->count(), 10);
}
