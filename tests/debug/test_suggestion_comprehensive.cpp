/**
 * @file test_suggestion_comprehensive.cpp
 * @brief Comprehensive unit tests for SuggestionEngine
 *
 * Tests for:
 * - Construction and configuration
 * - Suggestion generation (prefix, substring, fuzzy, regex)
 * - Dataset management
 * - Weights and filters
 * - Cache management
 * - Statistics
 * - History optimization
 * - Detailed suggestions
 * - Error handling
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "debug/suggestion.hpp"

namespace lithium::debug::test {

// ============================================================================
// SuggestionException Tests
// ============================================================================

class SuggestionExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SuggestionExceptionTest, ExceptionConstruction) {
    SuggestionException ex("Test error message");
    EXPECT_STREQ(ex.what(), "Test error message");
}

TEST_F(SuggestionExceptionTest, ExceptionInheritance) {
    try {
        throw SuggestionException("Test");
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Test");
    }
}

// ============================================================================
// SuggestionConfig Tests
// ============================================================================

class SuggestionConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SuggestionConfigTest, DefaultConstruction) {
    SuggestionConfig config;
    EXPECT_EQ(config.maxSuggestions, 5);
    EXPECT_FLOAT_EQ(config.fuzzyMatchThreshold, 0.5F);
    EXPECT_EQ(config.maxCacheSize, 1000);
    EXPECT_FLOAT_EQ(config.historyWeightFactor, 1.5F);
    EXPECT_FALSE(config.caseSensitive);
    EXPECT_TRUE(config.useTransposition);
    EXPECT_EQ(config.maxEditDistance, 3);
}

TEST_F(SuggestionConfigTest, CustomConfiguration) {
    SuggestionConfig config;
    config.maxSuggestions = 10;
    config.fuzzyMatchThreshold = 0.7F;
    config.caseSensitive = true;
    config.maxEditDistance = 5;

    EXPECT_EQ(config.maxSuggestions, 10);
    EXPECT_FLOAT_EQ(config.fuzzyMatchThreshold, 0.7F);
    EXPECT_TRUE(config.caseSensitive);
    EXPECT_EQ(config.maxEditDistance, 5);
}

// ============================================================================
// SuggestionDetail Tests
// ============================================================================

class SuggestionDetailTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SuggestionDetailTest, DefaultConstruction) {
    SuggestionDetail detail;
    EXPECT_TRUE(detail.suggestion.empty());
    EXPECT_FLOAT_EQ(detail.confidence, 0.0F);
    EXPECT_FLOAT_EQ(detail.editDistance, 0.0F);
    EXPECT_TRUE(detail.matchType.empty());
}

TEST_F(SuggestionDetailTest, PopulatedDetail) {
    SuggestionDetail detail;
    detail.suggestion = "help";
    detail.confidence = 0.95F;
    detail.editDistance = 1.0F;
    detail.matchType = "prefix";

    EXPECT_EQ(detail.suggestion, "help");
    EXPECT_FLOAT_EQ(detail.confidence, 0.95F);
    EXPECT_FLOAT_EQ(detail.editDistance, 1.0F);
    EXPECT_EQ(detail.matchType, "prefix");
}

TEST_F(SuggestionDetailTest, ComparisonOperators) {
    SuggestionDetail detail1, detail2;
    detail1.confidence = 0.8F;
    detail2.confidence = 0.9F;

    EXPECT_TRUE(detail1 < detail2);
    EXPECT_TRUE(detail2 > detail1);
    EXPECT_FALSE(detail1 > detail2);
    EXPECT_FALSE(detail2 < detail1);
}

// ============================================================================
// SuggestionStats Tests
// ============================================================================

class SuggestionStatsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SuggestionStatsTest, DefaultConstruction) {
    SuggestionStats stats;
    EXPECT_EQ(stats.totalSuggestionCalls, 0);
    EXPECT_EQ(stats.cacheHits, 0);
    EXPECT_EQ(stats.cacheMisses, 0);
    EXPECT_EQ(stats.totalProcessingTime.count(), 0);
    EXPECT_EQ(stats.itemsFiltered, 0);
    EXPECT_EQ(stats.datasetSize, 0);
    EXPECT_EQ(stats.cacheSize, 0);
}

// ============================================================================
// SuggestionEngine Basic Tests
// ============================================================================

class SuggestionEngineBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "banana", "grape",   "orange", "watermelon",
                   "help",  "hello",  "history", "exit",   "clear"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineBasicTest, ConstructWithDataset) {
    EXPECT_NE(engine, nullptr);
}

TEST_F(SuggestionEngineBasicTest, ConstructWithMaxSuggestions) {
    SuggestionEngine engineWithMax(dataset, 3);
    auto config = engineWithMax.getConfig();
    EXPECT_EQ(config.maxSuggestions, 3);
}

TEST_F(SuggestionEngineBasicTest, ConstructWithConfig) {
    SuggestionConfig config;
    config.maxSuggestions = 10;
    config.caseSensitive = true;

    SuggestionEngine engineWithConfig(dataset, config);
    auto retrievedConfig = engineWithConfig.getConfig();
    EXPECT_EQ(retrievedConfig.maxSuggestions, 10);
    EXPECT_TRUE(retrievedConfig.caseSensitive);
}

// ============================================================================
// SuggestionEngine Prefix Match Tests
// ============================================================================

class SuggestionEnginePrefixTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "application", "apply", "banana", "band"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEnginePrefixTest, PrefixMatch) {
    auto suggestions =
        engine->suggest("app", SuggestionEngine::MatchType::Prefix);
    EXPECT_EQ(suggestions.size(), 3);  // apple, application, apply
}

TEST_F(SuggestionEnginePrefixTest, PrefixMatchSingleResult) {
    auto suggestions =
        engine->suggest("ban", SuggestionEngine::MatchType::Prefix);
    EXPECT_EQ(suggestions.size(), 2);  // banana, band
}

TEST_F(SuggestionEnginePrefixTest, PrefixMatchNoResult) {
    auto suggestions =
        engine->suggest("xyz", SuggestionEngine::MatchType::Prefix);
    EXPECT_TRUE(suggestions.empty());
}

TEST_F(SuggestionEnginePrefixTest, PrefixMatchExact) {
    auto suggestions =
        engine->suggest("apple", SuggestionEngine::MatchType::Prefix);
    EXPECT_EQ(suggestions.size(), 1);
    EXPECT_EQ(suggestions[0], "apple");
}

// ============================================================================
// SuggestionEngine Substring Match Tests
// ============================================================================

class SuggestionEngineSubstringTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"banana", "orange", "mango", "watermelon"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineSubstringTest, SubstringMatch) {
    auto suggestions =
        engine->suggest("an", SuggestionEngine::MatchType::Substring);
    EXPECT_EQ(suggestions.size(), 3);  // banana, orange, mango
}

TEST_F(SuggestionEngineSubstringTest, SubstringMatchMiddle) {
    auto suggestions =
        engine->suggest("mel", SuggestionEngine::MatchType::Substring);
    EXPECT_EQ(suggestions.size(), 1);  // watermelon
}

TEST_F(SuggestionEngineSubstringTest, SubstringMatchNoResult) {
    auto suggestions =
        engine->suggest("xyz", SuggestionEngine::MatchType::Substring);
    EXPECT_TRUE(suggestions.empty());
}

// ============================================================================
// SuggestionEngine Fuzzy Match Tests
// ============================================================================

class SuggestionEngineFuzzyTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"help", "hello", "history", "exit", "clear"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineFuzzyTest, FuzzyMatchTypo) {
    auto suggestions =
        engine->suggest("hlep", SuggestionEngine::MatchType::Fuzzy);
    // Should find "help" despite typo
    bool found = false;
    for (const auto& s : suggestions) {
        if (s == "help") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SuggestionEngineFuzzyTest, FuzzyMatchMissingChar) {
    auto suggestions =
        engine->suggest("helo", SuggestionEngine::MatchType::Fuzzy);
    // Should find "hello"
    bool found = false;
    for (const auto& s : suggestions) {
        if (s == "hello") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// SuggestionEngine Regex Match Tests
// ============================================================================

class SuggestionEngineRegexTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"file1.txt", "file2.txt", "image.png", "doc.pdf"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineRegexTest, RegexMatchPattern) {
    auto suggestions =
        engine->suggest(".*\\.txt", SuggestionEngine::MatchType::Regex);
    EXPECT_EQ(suggestions.size(), 2);  // file1.txt, file2.txt
}

TEST_F(SuggestionEngineRegexTest, RegexMatchWildcard) {
    auto suggestions =
        engine->suggest("file.*", SuggestionEngine::MatchType::Regex);
    EXPECT_EQ(suggestions.size(), 2);
}

// ============================================================================
// SuggestionEngine Empty Input Tests
// ============================================================================

class SuggestionEngineEmptyInputTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "banana"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineEmptyInputTest, EmptyInputThrows) {
    EXPECT_THROW(engine->suggest("", SuggestionEngine::MatchType::Prefix),
                 SuggestionException);
}

// ============================================================================
// SuggestionEngine Dataset Management Tests
// ============================================================================

class SuggestionEngineDatasetTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "banana"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineDatasetTest, UpdateDataset) {
    std::vector<std::string> newItems = {"cherry", "date"};
    engine->updateDataset(newItems);

    auto suggestions =
        engine->suggest("ch", SuggestionEngine::MatchType::Prefix);
    EXPECT_EQ(suggestions.size(), 1);
    EXPECT_EQ(suggestions[0], "cherry");
}

TEST_F(SuggestionEngineDatasetTest, SetDataset) {
    std::vector<std::string> newDataset = {"x", "y", "z"};
    engine->setDataset(newDataset);

    auto suggestions =
        engine->suggest("a", SuggestionEngine::MatchType::Prefix);
    EXPECT_TRUE(suggestions.empty());  // Old data should be replaced

    suggestions = engine->suggest("x", SuggestionEngine::MatchType::Prefix);
    EXPECT_EQ(suggestions.size(), 1);
}

// ============================================================================
// SuggestionEngine Weight Tests
// ============================================================================

class SuggestionEngineWeightTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "apricot", "avocado"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineWeightTest, SetWeight) {
    engine->setWeight("avocado", 10.0F);

    auto suggestions =
        engine->suggest("a", SuggestionEngine::MatchType::Prefix);
    // Avocado should be first due to higher weight
    EXPECT_EQ(suggestions[0], "avocado");
}

TEST_F(SuggestionEngineWeightTest, MultipleWeights) {
    engine->setWeight("apricot", 5.0F);
    engine->setWeight("avocado", 10.0F);

    auto suggestions =
        engine->suggest("a", SuggestionEngine::MatchType::Prefix);
    // Order should be: avocado, apricot, apple
    EXPECT_EQ(suggestions[0], "avocado");
    EXPECT_EQ(suggestions[1], "apricot");
}

// ============================================================================
// SuggestionEngine Filter Tests
// ============================================================================

class SuggestionEngineFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "banana", "cherry", "date"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineFilterTest, AddFilter) {
    engine->addFilter([](const std::string& item) {
        return item != "banana";  // Filter out banana
    });

    auto suggestions =
        engine->suggest("b", SuggestionEngine::MatchType::Prefix);
    EXPECT_TRUE(suggestions.empty());  // banana filtered out
}

TEST_F(SuggestionEngineFilterTest, MultipleFilters) {
    engine->addFilter([](const std::string& item) {
        return item.length() > 4;  // Only items longer than 4 chars
    });
    engine->addFilter([](const std::string& item) {
        return item[0] != 'd';  // No items starting with 'd'
    });

    auto suggestions = engine->suggest("", SuggestionEngine::MatchType::Prefix);
    // Should have: apple, banana, cherry (date filtered by both)
}

TEST_F(SuggestionEngineFilterTest, ClearFilters) {
    engine->addFilter([](const std::string& item) { return false; });

    engine->clearFilters();

    auto suggestions =
        engine->suggest("a", SuggestionEngine::MatchType::Prefix);
    EXPECT_EQ(suggestions.size(), 1);  // apple should be found
}

// ============================================================================
// SuggestionEngine Cache Tests
// ============================================================================

class SuggestionEngineCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "banana", "cherry"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineCacheTest, CacheHit) {
    // First call - cache miss
    engine->suggest("app", SuggestionEngine::MatchType::Prefix);

    // Second call - should be cache hit
    engine->suggest("app", SuggestionEngine::MatchType::Prefix);

    auto stats = engine->getStats();
    EXPECT_GT(stats.cacheHits, 0);
}

TEST_F(SuggestionEngineCacheTest, ClearCache) {
    engine->suggest("app", SuggestionEngine::MatchType::Prefix);
    engine->clearCache();

    auto stats = engine->getStats();
    EXPECT_EQ(stats.cacheSize, 0);
}

// ============================================================================
// SuggestionEngine Configuration Tests
// ============================================================================

class SuggestionEngineConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "banana"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineConfigTest, SetFuzzyMatchThreshold) {
    engine->setFuzzyMatchThreshold(0.8F);
    auto config = engine->getConfig();
    EXPECT_FLOAT_EQ(config.fuzzyMatchThreshold, 0.8F);
}

TEST_F(SuggestionEngineConfigTest, SetFuzzyMatchThresholdInvalid) {
    EXPECT_THROW(engine->setFuzzyMatchThreshold(1.5F), SuggestionException);
    EXPECT_THROW(engine->setFuzzyMatchThreshold(-0.1F), SuggestionException);
}

TEST_F(SuggestionEngineConfigTest, SetMaxSuggestions) {
    engine->setMaxSuggestions(10);
    auto config = engine->getConfig();
    EXPECT_EQ(config.maxSuggestions, 10);
}

TEST_F(SuggestionEngineConfigTest, SetCaseSensitivity) {
    engine->setCaseSensitivity(true);
    auto config = engine->getConfig();
    EXPECT_TRUE(config.caseSensitive);
}

TEST_F(SuggestionEngineConfigTest, UpdateConfig) {
    SuggestionConfig newConfig;
    newConfig.maxSuggestions = 20;
    newConfig.caseSensitive = true;
    newConfig.fuzzyMatchThreshold = 0.6F;

    engine->updateConfig(newConfig);

    auto config = engine->getConfig();
    EXPECT_EQ(config.maxSuggestions, 20);
    EXPECT_TRUE(config.caseSensitive);
    EXPECT_FLOAT_EQ(config.fuzzyMatchThreshold, 0.6F);
}

TEST_F(SuggestionEngineConfigTest, GetConfig) {
    auto config = engine->getConfig();
    EXPECT_EQ(config.maxSuggestions, 5);  // Default
}

// ============================================================================
// SuggestionEngine History Tests
// ============================================================================

class SuggestionEngineHistoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"help", "hello", "history", "exit"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineHistoryTest, UpdateFromHistory) {
    std::vector<std::string> history = {"help", "help", "help", "exit"};
    engine->updateFromHistory(history);

    auto suggestions =
        engine->suggest("h", SuggestionEngine::MatchType::Prefix);
    // "help" should be ranked higher due to history
    EXPECT_EQ(suggestions[0], "help");
}

// ============================================================================
// SuggestionEngine Detailed Suggestions Tests
// ============================================================================

class SuggestionEngineDetailTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"help", "hello", "history"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineDetailTest, GetSuggestionDetails) {
    auto details = engine->getSuggestionDetails(
        "hel", SuggestionEngine::MatchType::Prefix);

    EXPECT_FALSE(details.empty());
    for (const auto& detail : details) {
        EXPECT_FALSE(detail.suggestion.empty());
        EXPECT_GE(detail.confidence, 0.0F);
        EXPECT_LE(detail.confidence, 1.0F);
    }
}

TEST_F(SuggestionEngineDetailTest, DetailsContainMatchType) {
    auto details = engine->getSuggestionDetails(
        "hel", SuggestionEngine::MatchType::Prefix);

    for (const auto& detail : details) {
        EXPECT_FALSE(detail.matchType.empty());
    }
}

// ============================================================================
// SuggestionEngine Statistics Tests
// ============================================================================

class SuggestionEngineStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"apple", "banana", "cherry"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineStatsTest, GetStats) {
    engine->suggest("app", SuggestionEngine::MatchType::Prefix);
    engine->suggest("ban", SuggestionEngine::MatchType::Prefix);

    auto stats = engine->getStats();
    EXPECT_EQ(stats.totalSuggestionCalls, 2);
    EXPECT_EQ(stats.datasetSize, 3);
}

TEST_F(SuggestionEngineStatsTest, GetStatisticsText) {
    engine->suggest("app", SuggestionEngine::MatchType::Prefix);

    std::string text = engine->getStatisticsText();
    EXPECT_FALSE(text.empty());
}

TEST_F(SuggestionEngineStatsTest, ResetStats) {
    engine->suggest("app", SuggestionEngine::MatchType::Prefix);
    engine->resetStats();

    auto stats = engine->getStats();
    EXPECT_EQ(stats.totalSuggestionCalls, 0);
}

// ============================================================================
// SuggestionEngine Move Semantics Tests
// ============================================================================

class SuggestionEngineMoveTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SuggestionEngineMoveTest, MoveConstruction) {
    std::vector<std::string> dataset = {"apple", "banana"};
    SuggestionEngine original(dataset);

    SuggestionEngine moved(std::move(original));

    auto suggestions =
        moved.suggest("app", SuggestionEngine::MatchType::Prefix);
    EXPECT_EQ(suggestions.size(), 1);
}

TEST_F(SuggestionEngineMoveTest, MoveAssignment) {
    std::vector<std::string> dataset1 = {"apple"};
    std::vector<std::string> dataset2 = {"banana"};

    SuggestionEngine original(dataset1);
    SuggestionEngine target(dataset2);

    target = std::move(original);

    auto suggestions =
        target.suggest("app", SuggestionEngine::MatchType::Prefix);
    EXPECT_EQ(suggestions.size(), 1);
}

// ============================================================================
// SuggestionEngine Case Sensitivity Tests
// ============================================================================

class SuggestionEngineCaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"Apple", "BANANA", "cherry"};
        engine = std::make_unique<SuggestionEngine>(dataset);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineCaseTest, CaseInsensitiveDefault) {
    auto suggestions =
        engine->suggest("apple", SuggestionEngine::MatchType::Prefix);
    EXPECT_EQ(suggestions.size(), 1);
    EXPECT_EQ(suggestions[0], "Apple");
}

TEST_F(SuggestionEngineCaseTest, CaseSensitiveEnabled) {
    engine->setCaseSensitivity(true);

    auto suggestions =
        engine->suggest("apple", SuggestionEngine::MatchType::Prefix);
    EXPECT_TRUE(suggestions.empty());  // "Apple" won't match "apple"
}

// ============================================================================
// SuggestionEngine Max Suggestions Tests
// ============================================================================

class SuggestionEngineMaxTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataset = {"a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "a10"};
        engine = std::make_unique<SuggestionEngine>(dataset, 3);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SuggestionEngine> engine;
    std::vector<std::string> dataset;
};

TEST_F(SuggestionEngineMaxTest, LimitedSuggestions) {
    auto suggestions =
        engine->suggest("a", SuggestionEngine::MatchType::Prefix);
    EXPECT_LE(suggestions.size(), 3);
}

TEST_F(SuggestionEngineMaxTest, ChangeMaxSuggestions) {
    engine->setMaxSuggestions(5);

    auto suggestions =
        engine->suggest("a", SuggestionEngine::MatchType::Prefix);
    EXPECT_LE(suggestions.size(), 5);
}

}  // namespace lithium::debug::test
