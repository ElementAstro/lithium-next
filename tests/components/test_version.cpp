#include "components/version.hpp"

#include <gtest/gtest.h>

namespace lithium::test {

class VersionTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(VersionTest, DefaultConstructor) {
    Version version;
    EXPECT_EQ(version.major, 0);
    EXPECT_EQ(version.minor, 0);
    EXPECT_EQ(version.patch, 0);
    EXPECT_TRUE(version.prerelease.empty());
    EXPECT_TRUE(version.build.empty());
}

TEST_F(VersionTest, ParameterizedConstructor) {
    Version version{1, 2, 3, "alpha", "build123"};
    EXPECT_EQ(version.major, 1);
    EXPECT_EQ(version.minor, 2);
    EXPECT_EQ(version.patch, 3);
    EXPECT_EQ(version.prerelease, "alpha");
    EXPECT_EQ(version.build, "build123");
}

TEST_F(VersionTest, ToString) {
    Version version{1, 2, 3, "alpha", "build123"};
    EXPECT_EQ(version.toString(), "1.2.3-alpha+build123");
}

TEST_F(VersionTest, ToShortString) {
    Version version{1, 2, 3};
    EXPECT_EQ(version.toShortString(), "1.2");
}

TEST_F(VersionTest, ParseValidVersion) {
    auto version = Version::parse("1.2.3-alpha+build123");
    EXPECT_EQ(version.major, 1);
    EXPECT_EQ(version.minor, 2);
    EXPECT_EQ(version.patch, 3);
    EXPECT_EQ(version.prerelease, "alpha");
    EXPECT_EQ(version.build, "build123");
}

TEST_F(VersionTest, ParseInvalidVersion) {
    EXPECT_THROW(Version::parse("1.2"), std::invalid_argument);
    EXPECT_THROW(Version::parse("1.2.3.4"), std::invalid_argument);
    EXPECT_THROW(Version::parse("1.2.a"), std::invalid_argument);
}

TEST_F(VersionTest, ComparisonOperators) {
    Version v1{1, 2, 3};
    Version v2{1, 2, 4};
    Version v3{1, 2, 3, "alpha"};
    Version v4{1, 2, 3, "beta"};

    EXPECT_TRUE(v1 < v2);
    EXPECT_TRUE(v2 > v1);
    EXPECT_TRUE(v1 == v1);
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v2 >= v1);
    EXPECT_TRUE(v3 < v4);
}

TEST_F(VersionTest, IsCompatibleWith) {
    Version v1{1, 2, 3};
    Version v2{1, 2, 4};
    Version v3{1, 3, 0};
    Version v4{2, 0, 0};

    EXPECT_TRUE(v1.isCompatibleWith(v2));
    EXPECT_TRUE(v1.isCompatibleWith(v3));
    EXPECT_FALSE(v1.isCompatibleWith(v4));
}

TEST_F(VersionTest, SatisfiesRange) {
    Version v1{1, 2, 3};
    Version min{1, 0, 0};
    Version max{1, 3, 0};

    EXPECT_TRUE(v1.satisfiesRange(min, max));
    EXPECT_FALSE(v1.satisfiesRange(Version{1, 3, 1}, max));
}

TEST_F(VersionTest, IsPrerelease) {
    Version v1{1, 2, 3, "alpha"};
    Version v2{1, 2, 3};

    EXPECT_TRUE(v1.isPrerelease());
    EXPECT_FALSE(v2.isPrerelease());
}

TEST_F(VersionTest, HasBuildMetadata) {
    Version v1{1, 2, 3, "", "build123"};
    Version v2{1, 2, 3};

    EXPECT_TRUE(v1.hasBuildMetadata());
    EXPECT_FALSE(v2.hasBuildMetadata());
}

TEST_F(VersionTest, CheckVersion) {
    Version actual{1, 2, 3};
    EXPECT_TRUE(checkVersion(actual, ">=1.2.0"));
    EXPECT_FALSE(checkVersion(actual, ">1.2.3"));
    EXPECT_TRUE(checkVersion(actual, "=1.2.3"));
    EXPECT_FALSE(checkVersion(actual, "<1.2.3"));
}

TEST_F(VersionTest, DateVersionComparison) {
    DateVersion d1{2021, 5, 20};
    DateVersion d2{2021, 6, 20};

    EXPECT_TRUE(d1 < d2);
    EXPECT_TRUE(d2 > d1);
    EXPECT_TRUE(d1 == d1);
    EXPECT_TRUE(d1 <= d2);
    EXPECT_TRUE(d2 >= d1);
}

TEST_F(VersionTest, CheckDateVersion) {
    DateVersion actual{2021, 5, 20};
    EXPECT_TRUE(checkDateVersion(actual, ">=2021-05-01"));
    EXPECT_FALSE(checkDateVersion(actual, ">2021-05-20"));
    EXPECT_TRUE(checkDateVersion(actual, "=2021-05-20"));
    EXPECT_FALSE(checkDateVersion(actual, "<2021-05-20"));
}

TEST_F(VersionTest, VersionRangeContains) {
    VersionRange range{Version{1, 0, 0}, Version{2, 0, 0}};
    EXPECT_TRUE(range.contains(Version{1, 5, 0}));
    EXPECT_FALSE(range.contains(Version{2, 1, 0}));
}

TEST_F(VersionTest, VersionRangeParse) {
    auto range = VersionRange::parse("[1.0.0,2.0.0]");
    EXPECT_TRUE(range.includeMin);
    EXPECT_TRUE(range.includeMax);
}

}  // namespace lithium::test
