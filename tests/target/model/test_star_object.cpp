// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for StarObject model
 */

#include <gtest/gtest.h>
#include "target/model/star_object.hpp"

using namespace lithium::target::model;

class StarObjectTest : public ::testing::Test {
protected:
    void SetUp() override {
        aliases_ = {"NGC224", "Andromeda Galaxy", "Great Andromeda Nebula"};
        testStar_ = StarObject("M31", aliases_, 100);
    }

    std::vector<std::string> aliases_;
    StarObject testStar_;
};

TEST_F(StarObjectTest, Construction) {
    EXPECT_EQ(testStar_.getName(), "M31");
    EXPECT_EQ(testStar_.getClickCount(), 100);
}

TEST_F(StarObjectTest, DefaultConstruction) {
    StarObject star;
    EXPECT_TRUE(star.getName().empty());
    EXPECT_EQ(star.getClickCount(), 0);
}

TEST_F(StarObjectTest, AliasesAreStored) {
    const auto& storedAliases = testStar_.getAliases();
    EXPECT_EQ(storedAliases.size(), 3);
    EXPECT_EQ(storedAliases[0], "NGC224");
}

TEST_F(StarObjectTest, SetName) {
    testStar_.setName("NGC224");
    EXPECT_EQ(testStar_.getName(), "NGC224");
}

TEST_F(StarObjectTest, SetAliases) {
    std::vector<std::string> newAliases = {"M31", "Andromeda"};
    testStar_.setAliases(newAliases);
    EXPECT_EQ(testStar_.getAliases().size(), 2);
}

TEST_F(StarObjectTest, SetClickCount) {
    testStar_.setClickCount(200);
    EXPECT_EQ(testStar_.getClickCount(), 200);
}

TEST_F(StarObjectTest, IncrementClickCount) {
    int initial = testStar_.getClickCount();
    testStar_.setClickCount(initial + 1);
    EXPECT_EQ(testStar_.getClickCount(), initial + 1);
}

TEST_F(StarObjectTest, CelestialObjectAssociation) {
    CelestialObject celestial;
    celestial.identifier = "M31";
    celestial.type = "Galaxy";

    testStar_.setCelestialObject(celestial);
    auto retrieved = testStar_.getCelestialObject();

    EXPECT_EQ(retrieved.identifier, "M31");
    EXPECT_EQ(retrieved.type, "Galaxy");
}

TEST_F(StarObjectTest, JsonSerialization) {
    auto j = testStar_.toJson();
    EXPECT_EQ(j["name"], "M31");
    EXPECT_EQ(j["clickCount"], 100);
    EXPECT_TRUE(j["aliases"].is_array());
}

TEST_F(StarObjectTest, EmptyAliases) {
    StarObject star("Polaris", {}, 50);
    EXPECT_TRUE(star.getAliases().empty());
}
