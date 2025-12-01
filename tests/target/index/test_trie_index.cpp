#include <gtest/gtest.h>

#include "src/target/index/trie_index.hpp"

using namespace lithium::target::index;

class TrieIndexTest : public ::testing::Test {
protected:
    TrieIndex& index = TrieIndex::instance();

    void SetUp() override {
        index.clear();
    }
};

TEST_F(TrieIndexTest, SingleInsertion) {
    index.insert("Orion");
    EXPECT_EQ(index.size(), 6);  // "O", "Or", "Ori", "Orio", "Orion", root
}

TEST_F(TrieIndexTest, DuplicateInsertion) {
    index.insert("Sirius");
    index.insert("Sirius");  // Duplicate
    EXPECT_GT(index.size(), 0);
}

TEST_F(TrieIndexTest, Autocomplete) {
    index.insert("Orion");
    index.insert("Orionids");
    index.insert("Orange");

    auto results = index.autocomplete("Ori");
    EXPECT_GE(results.size(), 2);
    EXPECT_TRUE(std::find(results.begin(), results.end(), "Orion") !=
                results.end());
}

TEST_F(TrieIndexTest, AutocompleteLimit) {
    index.insert("Apple");
    index.insert("Application");
    index.insert("Apply");
    index.insert("Approach");

    auto results = index.autocomplete("Ap", 2);
    EXPECT_LE(results.size(), 2);
}

TEST_F(TrieIndexTest, BatchInsertion) {
    std::vector<std::string> words = {"Sirius", "Vega", "Altair", "Polaris"};
    index.insertBatch(words);

    auto results = index.autocomplete("Si");
    EXPECT_TRUE(std::find(results.begin(), results.end(), "Sirius") !=
                results.end());
}

TEST_F(TrieIndexTest, Clear) {
    index.insert("Betelgeuse");
    index.insert("Rigel");
    index.clear();

    EXPECT_EQ(index.size(), 1);  // Only root remains
    auto results = index.autocomplete("Be");
    EXPECT_EQ(results.size(), 0);
}

TEST_F(TrieIndexTest, PrefixNotFound) {
    index.insert("Galaxy");
    auto results = index.autocomplete("Xyz");
    EXPECT_EQ(results.size(), 0);
}

TEST_F(TrieIndexTest, EmptyPrefix) {
    index.insert("Star");
    auto results = index.autocomplete("");
    EXPECT_GE(results.size(), 1);
}
