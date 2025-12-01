#include <gtest/gtest.h>

#include "src/target/index/spatial_index.hpp"

using namespace lithium::target::index;

class SpatialIndexTest : public ::testing::Test {
protected:
    SpatialIndex index;
};

TEST_F(SpatialIndexTest, SingleInsertion) {
    index.insert("Orion M42", 85.375, -2.27);
    EXPECT_EQ(index.size(), 1);
    EXPECT_TRUE(index.contains("Orion M42"));
}

TEST_F(SpatialIndexTest, MultipleInsertions) {
    index.insert("Sirius", 101.287, -16.716);
    index.insert("Vega", 279.235, 38.783);
    index.insert("Altair", 297.696, 8.861);

    EXPECT_EQ(index.size(), 3);
}

TEST_F(SpatialIndexTest, GetCoordinates) {
    index.insert("Polaris", 37.954, 89.264);
    auto coords = index.getCoordinates("Polaris");

    EXPECT_DOUBLE_EQ(coords.first, 37.954);
    EXPECT_DOUBLE_EQ(coords.second, 89.264);
}

TEST_F(SpatialIndexTest, GetCoordinatesNotFound) {
    auto coords = index.getCoordinates("NonExistent");
    EXPECT_DOUBLE_EQ(coords.first, 0.0);
    EXPECT_DOUBLE_EQ(coords.second, 0.0);
}

TEST_F(SpatialIndexTest, RadiusSearch) {
    // Add nearby objects
    index.insert("Target1", 85.0, 0.0);
    index.insert("Target2", 86.0, 1.0);
    index.insert("Target3", 90.0, 5.0);

    // Search within 5 degrees of 85.0, 0.0
    auto results = index.searchRadius(85.0, 0.0, 5.0);

    // Should find at least Target1 and Target2
    EXPECT_GE(results.size(), 2);

    // Results should be sorted by distance
    if (results.size() >= 2) {
        EXPECT_LE(results[0].second, results[1].second);
    }
}

TEST_F(SpatialIndexTest, RadiusSearchEmpty) {
    auto results = index.searchRadius(0.0, 0.0, 10.0, 100);
    EXPECT_EQ(results.size(), 0);
}

TEST_F(SpatialIndexTest, BoxSearch) {
    index.insert("Inside", 85.0, -2.0);
    index.insert("Outside", 90.0, 10.0);

    auto results = index.searchBox(80.0, 90.0, -5.0, 0.0);

    EXPECT_GE(results.size(), 1);
    EXPECT_TRUE(std::find(results.begin(), results.end(), "Inside") !=
                results.end());
}

TEST_F(SpatialIndexTest, Remove) {
    index.insert("ToRemove", 50.0, 50.0);
    EXPECT_TRUE(index.contains("ToRemove"));

    index.remove("ToRemove");
    EXPECT_FALSE(index.contains("ToRemove"));
    EXPECT_EQ(index.size(), 0);
}

TEST_F(SpatialIndexTest, Clear) {
    index.insert("Star1", 0.0, 0.0);
    index.insert("Star2", 90.0, 45.0);
    index.insert("Star3", 180.0, -45.0);

    index.clear();
    EXPECT_EQ(index.size(), 0);
}

TEST_F(SpatialIndexTest, BatchInsertion) {
    std::vector<std::tuple<std::string, double, double>> objects = {
        {"Sirius", 101.287, -16.716},
        {"Vega", 279.235, 38.783},
        {"Altair", 297.696, 8.861},
    };

    index.insertBatch(objects);
    EXPECT_EQ(index.size(), 3);
}

TEST_F(SpatialIndexTest, RadiusSearchLimit) {
    // Add many objects
    for (int i = 0; i < 50; ++i) {
        index.insert("Star" + std::to_string(i), static_cast<double>(i),
                     static_cast<double>(i % 10));
    }

    auto results = index.searchRadius(25.0, 5.0, 20.0, 10);
    EXPECT_LE(results.size(), 10);
}

TEST_F(SpatialIndexTest, CoordinateRanges) {
    // Test edge cases for coordinates
    index.insert("North Pole", 0.0, 90.0);
    index.insert("South Pole", 180.0, -90.0);
    index.insert("Equator", 90.0, 0.0);

    EXPECT_EQ(index.size(), 3);
    EXPECT_TRUE(index.contains("North Pole"));
    EXPECT_TRUE(index.contains("South Pole"));
}
