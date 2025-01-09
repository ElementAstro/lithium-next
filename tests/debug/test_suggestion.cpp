#ifndef LITHIUM_DEBUG_TEST_SUGGESTION_HPP
#define LITHIUM_DEBUG_TEST_SUGGESTION_HPP

#include <gtest/gtest.h>
#include "debug/suggestion.hpp"

using namespace lithium::debug;

class SuggestionEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "banana", "grape", "orange", "watermelon"};
        engine = std::make_unique<SuggestionEngine>(dataset, 3);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineTest, SuggestPrefix) {
    auto suggestions =
        engine->suggest("ap", SuggestionEngine::MatchType::Prefix);
    ASSERT_EQ(suggestions.size(), 1);
    EXPECT_EQ(suggestions[0], "apple");
}

TEST_F(SuggestionEngineTest, SuggestSubstring) {
    auto suggestions =
        engine->suggest("an", SuggestionEngine::MatchType::Substring);
    ASSERT_EQ(suggestions.size(), 2);
    EXPECT_EQ(suggestions[0], "banana");
    EXPECT_EQ(suggestions[1], "orange");
}

TEST_F(SuggestionEngineTest, SuggestEmptyInput) {
    EXPECT_THROW(engine->suggest("", SuggestionEngine::MatchType::Prefix),
                 SuggestionException);
}

TEST_F(SuggestionEngineTest, UpdateDataset) {
    std::vector<std::string> newItems = {"kiwi", "mango"};
    engine->updateDataset(newItems);
    auto suggestions =
        engine->suggest("ki", SuggestionEngine::MatchType::Prefix);
    ASSERT_EQ(suggestions.size(), 1);
    EXPECT_EQ(suggestions[0], "kiwi");
}

TEST_F(SuggestionEngineTest, SetWeight) {
    engine->setWeight("banana", 2.0f);
    auto suggestions =
        engine->suggest("an", SuggestionEngine::MatchType::Substring);
    ASSERT_EQ(suggestions.size(), 2);
    EXPECT_EQ(suggestions[0], "banana");
    EXPECT_EQ(suggestions[1], "orange");
}

TEST_F(SuggestionEngineTest, AddFilter) {
    engine->addFilter([](const std::string& item) { return item != "banana"; });
    auto suggestions =
        engine->suggest("an", SuggestionEngine::MatchType::Substring);
    ASSERT_EQ(suggestions.size(), 1);
    EXPECT_EQ(suggestions[0], "orange");
}

TEST_F(SuggestionEngineTest, ClearCache) {
    engine->suggest("ap", SuggestionEngine::MatchType::Prefix);
    engine->clearCache();
    auto suggestions =
        engine->suggest("ap", SuggestionEngine::MatchType::Prefix);
    ASSERT_EQ(suggestions.size(), 1);
    EXPECT_EQ(suggestions[0], "apple");
}

#endif  // LITHIUM_DEBUG_TEST_SUGGESTION_HPP