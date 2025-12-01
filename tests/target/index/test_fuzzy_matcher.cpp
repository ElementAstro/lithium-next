#include <gtest/gtest.h>

#include "src/target/index/fuzzy_matcher.hpp"

using namespace lithium::target::index;

class FuzzyMatcherTest : public ::testing::Test {
protected:
    FuzzyMatcher matcher;

    void SetUp() override {
        matcher.clear();
    }
};

TEST_F(FuzzyMatcherTest, SingleTermInsertion) {
    matcher.addTerm("Andromeda", "M31");
    EXPECT_EQ(matcher.size(), 1);
    EXPECT_TRUE(matcher.contains("Andromeda"));
}

TEST_F(FuzzyMatcherTest, ExactMatch) {
    matcher.addTerm("Andromeda", "M31");
    auto results = matcher.match("Andromeda", 0);

    EXPECT_GE(results.size(), 1);
    EXPECT_EQ(results[0].first, "M31");
    EXPECT_EQ(results[0].second, 0);
}

TEST_F(FuzzyMatcherTest, FuzzyMatchTypo) {
    matcher.addTerm("Andromeda", "M31");
    matcher.addTerm("Androemda", "M31_typo");  // Typo: o and e swapped

    auto results = matcher.match("Andromeda", 2);

    // Should find both within distance 2
    EXPECT_GE(results.size(), 1);
    EXPECT_EQ(results[0].second, 0);  // Exact match should be first
}

TEST_F(FuzzyMatcherTest, GetObjectId) {
    matcher.addTerm("Sirius", "alpha_cma");
    matcher.addTerm("Dog Star", "alpha_cma");

    EXPECT_EQ(matcher.getObjectId("Sirius"), "alpha_cma");
    EXPECT_EQ(matcher.getObjectId("Dog Star"), "alpha_cma");
}

TEST_F(FuzzyMatcherTest, CaseInsensitive) {
    matcher.addTerm("Orion", "M42");
    auto results1 = matcher.match("orion", 0);
    auto results2 = matcher.match("ORION", 0);

    EXPECT_GE(results1.size(), 1);
    EXPECT_GE(results2.size(), 1);
}

TEST_F(FuzzyMatcherTest, MultipleMatches) {
    matcher.addTerm("Betelgeuse", "alpha_ori");
    matcher.addTerm("Rigel", "beta_ori");
    matcher.addTerm("Bellatrix", "gamma_ori");

    auto results = matcher.match("Bet", 1);

    // Should find at least Betelgeuse
    EXPECT_GE(results.size(), 1);
}

TEST_F(FuzzyMatcherTest, BatchInsertion) {
    std::vector<std::pair<std::string, std::string>> terms = {
        {"Polaris", "alpha_umi"},
        {"Altair", "alpha_aql"},
        {"Vega", "alpha_lyr"},
    };

    matcher.addTerms(terms);
    EXPECT_EQ(matcher.size(), 3);
}

TEST_F(FuzzyMatcherTest, NoMatches) {
    matcher.addTerm("Galaxy", "G001");
    auto results = matcher.match("Completely different", 1);

    EXPECT_EQ(results.size(), 0);
}

TEST_F(FuzzyMatcherTest, EditDistanceVariations) {
    matcher.addTerm("Sirius", "alpha_cma");

    // Test different edit distances
    auto exact = matcher.match("Sirius", 0);
    auto distance1 = matcher.match("Srius", 1);  // 1 deletion
    auto distance2 = matcher.match("Sris", 2);   // 2 deletions

    EXPECT_GE(exact.size(), 1);
    EXPECT_GE(distance1.size(), 1);
    EXPECT_GE(distance2.size(), 1);
}

TEST_F(FuzzyMatcherTest, ResultLimit) {
    matcher.addTerm("Star1", "id1");
    matcher.addTerm("Star2", "id2");
    matcher.addTerm("Star3", "id3");
    matcher.addTerm("Star4", "id4");

    auto results = matcher.match("Star", 1, 2);
    EXPECT_LE(results.size(), 2);
}

TEST_F(FuzzyMatcherTest, Clear) {
    matcher.addTerm("Test", "test_id");
    EXPECT_EQ(matcher.size(), 1);

    matcher.clear();
    EXPECT_EQ(matcher.size(), 0);
}

TEST_F(FuzzyMatcherTest, DuplicateTerm) {
    matcher.addTerm("Duplicate", "id1");
    matcher.addTerm("Duplicate", "id2");  // Should be skipped

    EXPECT_EQ(matcher.size(), 1);
    EXPECT_EQ(matcher.getObjectId("Duplicate"), "id1");
}

TEST_F(FuzzyMatcherTest, GetStats) {
    matcher.addTerm("Test1", "test_id_1");
    matcher.addTerm("Test2", "test_id_2");
    matcher.addTerm("Test3", "test_id_3");

    auto stats = matcher.getStats();
    EXPECT_TRUE(stats.find("FuzzyMatcher Statistics") != std::string::npos);
    EXPECT_TRUE(stats.find("Terms:") != std::string::npos);
}

TEST_F(FuzzyMatcherTest, LongTerms) {
    std::string longTerm =
        "VeryLongAstronomicalObjectNameWithManyCharacters";
    matcher.addTerm(longTerm, "long_obj_id");

    auto results = matcher.match(longTerm, 0);
    EXPECT_GE(results.size(), 1);
}

TEST_F(FuzzyMatcherTest, SpecialCharacters) {
    matcher.addTerm("M31-Andromeda", "M31");
    matcher.addTerm("NGC 224", "NGC224");

    auto results1 = matcher.match("M31", 1);
    auto results2 = matcher.match("NGC", 1);

    EXPECT_GE(results1.size(), 1);
    EXPECT_GE(results2.size(), 1);
}
