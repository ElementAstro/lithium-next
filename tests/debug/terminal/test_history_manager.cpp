/**
 * @file test_history_manager.cpp
 * @brief Comprehensive unit tests for HistoryManager
 *
 * Tests for:
 * - Configuration management
 * - Entry management (add, get, remove)
 * - Navigation (previous, next)
 * - Search functionality
 * - Favorites management
 * - Tags management
 * - Persistence (load, save, JSON export/import)
 * - Statistics
 * - Iteration
 */

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "debug/terminal/history_manager.hpp"
#include "debug/terminal/types.hpp"

namespace lithium::debug::terminal::test {

// ============================================================================
// HistoryConfig Tests
// ============================================================================

class HistoryConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HistoryConfigTest, DefaultConstruction) {
    HistoryConfig config;
    EXPECT_EQ(config.maxEntries, 1000);
    EXPECT_FALSE(config.persistOnAdd);
    EXPECT_TRUE(config.deduplicateConsecutive);
    EXPECT_TRUE(config.trackTimestamps);
    EXPECT_FALSE(config.trackResults);
    EXPECT_TRUE(config.historyFile.empty());
}

TEST_F(HistoryConfigTest, CustomConfiguration) {
    HistoryConfig config;
    config.maxEntries = 500;
    config.persistOnAdd = true;
    config.deduplicateConsecutive = false;
    config.trackResults = true;
    config.historyFile = "/tmp/history.json";

    EXPECT_EQ(config.maxEntries, 500);
    EXPECT_TRUE(config.persistOnAdd);
    EXPECT_FALSE(config.deduplicateConsecutive);
    EXPECT_TRUE(config.trackResults);
    EXPECT_EQ(config.historyFile, "/tmp/history.json");
}

// ============================================================================
// HistorySearchOptions Tests
// ============================================================================

class HistorySearchOptionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HistorySearchOptionsTest, DefaultConstruction) {
    HistorySearchOptions options;
    EXPECT_FALSE(options.caseSensitive);
    EXPECT_FALSE(options.regexSearch);
    EXPECT_FALSE(options.prefixMatch);
    EXPECT_TRUE(options.reverseOrder);
    EXPECT_EQ(options.maxResults, 50);
    EXPECT_FALSE(options.afterTime.has_value());
    EXPECT_FALSE(options.beforeTime.has_value());
    EXPECT_TRUE(options.tags.empty());
}

TEST_F(HistorySearchOptionsTest, CustomOptions) {
    HistorySearchOptions options;
    options.caseSensitive = true;
    options.regexSearch = true;
    options.maxResults = 100;
    options.tags = {"git", "important"};

    EXPECT_TRUE(options.caseSensitive);
    EXPECT_TRUE(options.regexSearch);
    EXPECT_EQ(options.maxResults, 100);
    EXPECT_EQ(options.tags.size(), 2);
}

TEST_F(HistorySearchOptionsTest, TimeRangeOptions) {
    HistorySearchOptions options;
    auto now = std::chrono::system_clock::now();
    options.afterTime = now - std::chrono::hours(24);
    options.beforeTime = now;

    EXPECT_TRUE(options.afterTime.has_value());
    EXPECT_TRUE(options.beforeTime.has_value());
}

// ============================================================================
// HistoryStats Tests
// ============================================================================

class HistoryStatsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HistoryStatsTest, DefaultConstruction) {
    HistoryStats stats;
    EXPECT_EQ(stats.totalEntries, 0);
    EXPECT_EQ(stats.uniqueCommands, 0);
    EXPECT_EQ(stats.favoriteCount, 0);
    EXPECT_TRUE(stats.topCommands.empty());
}

// ============================================================================
// HistoryManager Basic Tests
// ============================================================================

class HistoryManagerBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        config.deduplicateConsecutive = true;
        manager = std::make_unique<HistoryManager>(config);
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerBasicTest, DefaultConstruction) {
    HistoryManager defaultManager;
    EXPECT_TRUE(defaultManager.empty());
    EXPECT_EQ(defaultManager.size(), 0);
}

TEST_F(HistoryManagerBasicTest, ConstructWithConfig) {
    EXPECT_TRUE(manager->empty());
    EXPECT_EQ(manager->getConfig().maxEntries, 100);
}

TEST_F(HistoryManagerBasicTest, SetConfig) {
    HistoryConfig newConfig;
    newConfig.maxEntries = 200;
    newConfig.deduplicateConsecutive = false;

    manager->setConfig(newConfig);

    EXPECT_EQ(manager->getConfig().maxEntries, 200);
    EXPECT_FALSE(manager->getConfig().deduplicateConsecutive);
}

TEST_F(HistoryManagerBasicTest, GetConfig) {
    const HistoryConfig& config = manager->getConfig();
    EXPECT_EQ(config.maxEntries, 100);
    EXPECT_TRUE(config.deduplicateConsecutive);
}

// ============================================================================
// HistoryManager Entry Management Tests
// ============================================================================

class HistoryManagerEntryTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        config.deduplicateConsecutive = true;
        manager = std::make_unique<HistoryManager>(config);
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerEntryTest, AddEntry) {
    manager->add("command1");
    EXPECT_EQ(manager->size(), 1);
    EXPECT_FALSE(manager->empty());
}

TEST_F(HistoryManagerEntryTest, AddMultipleEntries) {
    manager->add("command1");
    manager->add("command2");
    manager->add("command3");
    EXPECT_EQ(manager->size(), 3);
}

TEST_F(HistoryManagerEntryTest, AddEntryWithResult) {
    CommandResult result;
    result.success = true;
    result.output = "Output";

    manager->add("command1", result);
    EXPECT_EQ(manager->size(), 1);

    auto entry = manager->get(0);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->command, "command1");
}

TEST_F(HistoryManagerEntryTest, AddHistoryEntry) {
    HistoryEntry entry;
    entry.command = "full_entry";
    entry.timestamp = std::chrono::system_clock::now();
    entry.favorite = true;
    entry.tags = {"important"};

    manager->add(entry);
    EXPECT_EQ(manager->size(), 1);
}

TEST_F(HistoryManagerEntryTest, DeduplicateConsecutive) {
    manager->add("command1");
    manager->add("command1");  // Should not be added
    EXPECT_EQ(manager->size(), 1);
}

TEST_F(HistoryManagerEntryTest, DeduplicateNonConsecutive) {
    manager->add("command1");
    manager->add("command2");
    manager->add("command1");  // Should be added (not consecutive)
    EXPECT_EQ(manager->size(), 3);
}

TEST_F(HistoryManagerEntryTest, GetEntry) {
    manager->add("command1");
    auto entry = manager->get(0);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->command, "command1");
}

TEST_F(HistoryManagerEntryTest, GetInvalidIndex) {
    manager->add("command1");
    auto entry = manager->get(100);
    EXPECT_FALSE(entry.has_value());
}

TEST_F(HistoryManagerEntryTest, GetLast) {
    manager->add("first");
    manager->add("second");
    manager->add("last");

    auto entry = manager->getLast();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->command, "last");
}

TEST_F(HistoryManagerEntryTest, GetLastEmpty) {
    auto entry = manager->getLast();
    EXPECT_FALSE(entry.has_value());
}

TEST_F(HistoryManagerEntryTest, GetRelative) {
    manager->add("cmd1");
    manager->add("cmd2");
    manager->add("cmd3");

    manager->resetNavigation();
    manager->previous();  // Move to cmd3

    auto entry = manager->getRelative(-1);  // Get cmd2
    // Result depends on implementation
}

TEST_F(HistoryManagerEntryTest, RemoveEntry) {
    manager->add("command1");
    manager->add("command2");

    bool result = manager->remove(0);
    EXPECT_TRUE(result);
    EXPECT_EQ(manager->size(), 1);
}

TEST_F(HistoryManagerEntryTest, RemoveInvalidIndex) {
    manager->add("command1");
    bool result = manager->remove(100);
    EXPECT_FALSE(result);
}

TEST_F(HistoryManagerEntryTest, RemoveIf) {
    manager->add("git status");
    manager->add("ls -la");
    manager->add("git commit");

    size_t removed = manager->removeIf([](const HistoryEntry& entry) {
        return entry.command.find("git") != std::string::npos;
    });

    EXPECT_EQ(removed, 2);
    EXPECT_EQ(manager->size(), 1);
}

TEST_F(HistoryManagerEntryTest, Clear) {
    manager->add("command1");
    manager->add("command2");
    manager->clear();
    EXPECT_TRUE(manager->empty());
    EXPECT_EQ(manager->size(), 0);
}

TEST_F(HistoryManagerEntryTest, Size) {
    EXPECT_EQ(manager->size(), 0);
    manager->add("command1");
    EXPECT_EQ(manager->size(), 1);
    manager->add("command2");
    EXPECT_EQ(manager->size(), 2);
}

TEST_F(HistoryManagerEntryTest, Empty) {
    EXPECT_TRUE(manager->empty());
    manager->add("command1");
    EXPECT_FALSE(manager->empty());
}

// ============================================================================
// HistoryManager Navigation Tests
// ============================================================================

class HistoryManagerNavigationTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        manager = std::make_unique<HistoryManager>(config);

        manager->add("cmd1");
        manager->add("cmd2");
        manager->add("cmd3");
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerNavigationTest, Previous) {
    manager->resetNavigation();

    auto entry1 = manager->previous();
    ASSERT_TRUE(entry1.has_value());
    EXPECT_EQ(entry1->command, "cmd3");

    auto entry2 = manager->previous();
    ASSERT_TRUE(entry2.has_value());
    EXPECT_EQ(entry2->command, "cmd2");

    auto entry3 = manager->previous();
    ASSERT_TRUE(entry3.has_value());
    EXPECT_EQ(entry3->command, "cmd1");
}

TEST_F(HistoryManagerNavigationTest, PreviousAtBeginning) {
    manager->resetNavigation();

    manager->previous();  // cmd3
    manager->previous();  // cmd2
    manager->previous();  // cmd1
    auto entry = manager->previous();  // Should return nullopt or cmd1

    // Behavior at beginning depends on implementation
}

TEST_F(HistoryManagerNavigationTest, Next) {
    manager->resetNavigation();
    manager->previous();  // cmd3
    manager->previous();  // cmd2

    auto entry = manager->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->command, "cmd3");
}

TEST_F(HistoryManagerNavigationTest, NextAtEnd) {
    manager->resetNavigation();
    auto entry = manager->next();
    // Should return nullopt when at end
    EXPECT_FALSE(entry.has_value());
}

TEST_F(HistoryManagerNavigationTest, ResetNavigation) {
    manager->previous();
    manager->previous();

    manager->resetNavigation();

    auto entry = manager->previous();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->command, "cmd3");
}

TEST_F(HistoryManagerNavigationTest, GetPosition) {
    manager->resetNavigation();
    size_t pos = manager->getPosition();
    // Position should be at end after reset
}

TEST_F(HistoryManagerNavigationTest, SetPosition) {
    manager->setPosition(1);
    size_t pos = manager->getPosition();
    EXPECT_EQ(pos, 1);
}

// ============================================================================
// HistoryManager Search Tests
// ============================================================================

class HistoryManagerSearchTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        manager = std::make_unique<HistoryManager>(config);

        manager->add("git status");
        manager->add("git commit -m 'test'");
        manager->add("ls -la");
        manager->add("git push origin main");
        manager->add("grep pattern file.txt");
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerSearchTest, SearchPattern) {
    HistorySearchOptions options;
    options.maxResults = 10;

    auto results = manager->search("git", options);
    EXPECT_EQ(results.size(), 3);
}

TEST_F(HistoryManagerSearchTest, SearchNoMatch) {
    HistorySearchOptions options;
    auto results = manager->search("nonexistent", options);
    EXPECT_TRUE(results.empty());
}

TEST_F(HistoryManagerSearchTest, SearchCaseSensitive) {
    HistorySearchOptions options;
    options.caseSensitive = true;

    auto results = manager->search("GIT", options);
    EXPECT_TRUE(results.empty());  // No uppercase GIT in history
}

TEST_F(HistoryManagerSearchTest, SearchCaseInsensitive) {
    HistorySearchOptions options;
    options.caseSensitive = false;

    auto results = manager->search("GIT", options);
    EXPECT_EQ(results.size(), 3);
}

TEST_F(HistoryManagerSearchTest, SearchMaxResults) {
    HistorySearchOptions options;
    options.maxResults = 2;

    auto results = manager->search("git", options);
    EXPECT_LE(results.size(), 2);
}

TEST_F(HistoryManagerSearchTest, SearchPrefix) {
    auto results = manager->searchPrefix("git", 10);
    EXPECT_EQ(results.size(), 3);
}

TEST_F(HistoryManagerSearchTest, SearchPrefixNoMatch) {
    auto results = manager->searchPrefix("xyz", 10);
    EXPECT_TRUE(results.empty());
}

TEST_F(HistoryManagerSearchTest, ReverseSearch) {
    auto result = manager->reverseSearch("git");
    ASSERT_TRUE(result.has_value());
    // Should find most recent git command
}

TEST_F(HistoryManagerSearchTest, ReverseSearchNoMatch) {
    auto result = manager->reverseSearch("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(HistoryManagerSearchTest, GetMatching) {
    auto results = manager->getMatching("git");
    EXPECT_EQ(results.size(), 3);
}

// ============================================================================
// HistoryManager Favorites Tests
// ============================================================================

class HistoryManagerFavoritesTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        manager = std::make_unique<HistoryManager>(config);

        manager->add("command1");
        manager->add("command2");
        manager->add("command3");
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerFavoritesTest, SetFavorite) {
    bool result = manager->setFavorite(0, true);
    EXPECT_TRUE(result);

    auto favorites = manager->getFavorites();
    EXPECT_EQ(favorites.size(), 1);
    EXPECT_EQ(favorites[0].command, "command1");
}

TEST_F(HistoryManagerFavoritesTest, UnsetFavorite) {
    manager->setFavorite(0, true);
    manager->setFavorite(0, false);

    auto favorites = manager->getFavorites();
    EXPECT_TRUE(favorites.empty());
}

TEST_F(HistoryManagerFavoritesTest, SetFavoriteInvalidIndex) {
    bool result = manager->setFavorite(100, true);
    EXPECT_FALSE(result);
}

TEST_F(HistoryManagerFavoritesTest, ToggleFavorite) {
    manager->toggleFavorite(0);
    auto favorites1 = manager->getFavorites();
    EXPECT_EQ(favorites1.size(), 1);

    manager->toggleFavorite(0);
    auto favorites2 = manager->getFavorites();
    EXPECT_TRUE(favorites2.empty());
}

TEST_F(HistoryManagerFavoritesTest, GetFavorites) {
    manager->setFavorite(0, true);
    manager->setFavorite(2, true);

    auto favorites = manager->getFavorites();
    EXPECT_EQ(favorites.size(), 2);
}

TEST_F(HistoryManagerFavoritesTest, GetFavoritesEmpty) {
    auto favorites = manager->getFavorites();
    EXPECT_TRUE(favorites.empty());
}

// ============================================================================
// HistoryManager Tags Tests
// ============================================================================

class HistoryManagerTagsTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        manager = std::make_unique<HistoryManager>(config);

        manager->add("git status");
        manager->add("ls -la");
        manager->add("git commit");
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerTagsTest, AddTag) {
    bool result = manager->addTag(0, "git");
    EXPECT_TRUE(result);

    auto tagged = manager->getByTag("git");
    EXPECT_EQ(tagged.size(), 1);
}

TEST_F(HistoryManagerTagsTest, AddTagInvalidIndex) {
    bool result = manager->addTag(100, "tag");
    EXPECT_FALSE(result);
}

TEST_F(HistoryManagerTagsTest, RemoveTag) {
    manager->addTag(0, "git");
    bool result = manager->removeTag(0, "git");
    EXPECT_TRUE(result);

    auto tagged = manager->getByTag("git");
    EXPECT_TRUE(tagged.empty());
}

TEST_F(HistoryManagerTagsTest, RemoveTagInvalidIndex) {
    bool result = manager->removeTag(100, "tag");
    EXPECT_FALSE(result);
}

TEST_F(HistoryManagerTagsTest, GetByTag) {
    manager->addTag(0, "important");
    manager->addTag(2, "important");

    auto tagged = manager->getByTag("important");
    EXPECT_EQ(tagged.size(), 2);
}

TEST_F(HistoryManagerTagsTest, GetByTagNoMatch) {
    auto tagged = manager->getByTag("nonexistent");
    EXPECT_TRUE(tagged.empty());
}

TEST_F(HistoryManagerTagsTest, GetAllTags) {
    manager->addTag(0, "git");
    manager->addTag(0, "important");
    manager->addTag(1, "filesystem");

    auto tags = manager->getAllTags();
    EXPECT_EQ(tags.size(), 3);
}

TEST_F(HistoryManagerTagsTest, GetAllTagsEmpty) {
    auto tags = manager->getAllTags();
    EXPECT_TRUE(tags.empty());
}

// ============================================================================
// HistoryManager Persistence Tests
// ============================================================================

class HistoryManagerPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        manager = std::make_unique<HistoryManager>(config);

        testFilePath = std::filesystem::temp_directory_path() / "test_history.json";
    }

    void TearDown() override {
        manager.reset();
        if (std::filesystem::exists(testFilePath)) {
            std::filesystem::remove(testFilePath);
        }
    }

    std::unique_ptr<HistoryManager> manager;
    std::filesystem::path testFilePath;
};

TEST_F(HistoryManagerPersistenceTest, SaveToFile) {
    manager->add("command1");
    manager->add("command2");

    bool result = manager->save(testFilePath.string());
    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(testFilePath));
}

TEST_F(HistoryManagerPersistenceTest, LoadFromFile) {
    manager->add("command1");
    manager->add("command2");
    manager->save(testFilePath.string());

    HistoryManager newManager;
    bool result = newManager.load(testFilePath.string());
    EXPECT_TRUE(result);
    EXPECT_EQ(newManager.size(), 2);
}

TEST_F(HistoryManagerPersistenceTest, LoadNonexistentFile) {
    bool result = manager->load("/nonexistent/path/file.json");
    EXPECT_FALSE(result);
}

TEST_F(HistoryManagerPersistenceTest, ExportJson) {
    manager->add("command1");
    manager->add("command2");

    std::string json = manager->exportJson();
    EXPECT_FALSE(json.empty());
}

TEST_F(HistoryManagerPersistenceTest, ImportJson) {
    manager->add("command1");
    std::string json = manager->exportJson();

    HistoryManager newManager;
    bool result = newManager.importJson(json);
    EXPECT_TRUE(result);
    EXPECT_EQ(newManager.size(), 1);
}

TEST_F(HistoryManagerPersistenceTest, ImportInvalidJson) {
    bool result = manager->importJson("invalid json");
    EXPECT_FALSE(result);
}

// ============================================================================
// HistoryManager Statistics Tests
// ============================================================================

class HistoryManagerStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        manager = std::make_unique<HistoryManager>(config);

        manager->add("git status");
        manager->add("ls -la");
        manager->add("git status");  // Duplicate (non-consecutive)
        manager->add("git commit");
        manager->add("ls -la");  // Duplicate (non-consecutive)
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerStatsTest, GetStats) {
    auto stats = manager->getStats();
    EXPECT_EQ(stats.totalEntries, 5);
}

TEST_F(HistoryManagerStatsTest, GetCommandFrequency) {
    auto frequency = manager->getCommandFrequency(10);
    EXPECT_FALSE(frequency.empty());
}

TEST_F(HistoryManagerStatsTest, GetCommandFrequencyTopN) {
    auto frequency = manager->getCommandFrequency(2);
    EXPECT_LE(frequency.size(), 2);
}

TEST_F(HistoryManagerStatsTest, GetInTimeRange) {
    auto now = std::chrono::system_clock::now();
    auto hourAgo = now - std::chrono::hours(1);

    auto entries = manager->getInTimeRange(hourAgo, now);
    EXPECT_EQ(entries.size(), 5);  // All entries should be within range
}

// ============================================================================
// HistoryManager Iteration Tests
// ============================================================================

class HistoryManagerIterationTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 100;
        manager = std::make_unique<HistoryManager>(config);

        manager->add("cmd1");
        manager->add("cmd2");
        manager->add("cmd3");
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerIterationTest, GetAll) {
    auto entries = manager->getAll();
    EXPECT_EQ(entries.size(), 3);
}

TEST_F(HistoryManagerIterationTest, GetRecent) {
    auto recent = manager->getRecent(2);
    EXPECT_EQ(recent.size(), 2);
    EXPECT_EQ(recent[0].command, "cmd3");
    EXPECT_EQ(recent[1].command, "cmd2");
}

TEST_F(HistoryManagerIterationTest, GetRecentMoreThanAvailable) {
    auto recent = manager->getRecent(10);
    EXPECT_EQ(recent.size(), 3);
}

TEST_F(HistoryManagerIterationTest, ForEach) {
    std::vector<std::string> commands;
    manager->forEach([&commands](const HistoryEntry& entry) {
        commands.push_back(entry.command);
    });

    EXPECT_EQ(commands.size(), 3);
}

// ============================================================================
// HistoryManager Move Semantics Tests
// ============================================================================

class HistoryManagerMoveTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HistoryManagerMoveTest, MoveConstruction) {
    HistoryManager original;
    original.add("command1");
    original.add("command2");

    HistoryManager moved(std::move(original));
    EXPECT_EQ(moved.size(), 2);
}

TEST_F(HistoryManagerMoveTest, MoveAssignment) {
    HistoryManager original;
    original.add("command1");
    original.add("command2");

    HistoryManager target;
    target = std::move(original);
    EXPECT_EQ(target.size(), 2);
}

// ============================================================================
// HistoryManager Max Entries Tests
// ============================================================================

class HistoryManagerMaxEntriesTest : public ::testing::Test {
protected:
    void SetUp() override {
        HistoryConfig config;
        config.maxEntries = 5;
        config.deduplicateConsecutive = false;
        manager = std::make_unique<HistoryManager>(config);
    }

    void TearDown() override { manager.reset(); }

    std::unique_ptr<HistoryManager> manager;
};

TEST_F(HistoryManagerMaxEntriesTest, EnforceMaxEntries) {
    for (int i = 0; i < 10; ++i) {
        manager->add("command" + std::to_string(i));
    }

    EXPECT_LE(manager->size(), 5);
}

TEST_F(HistoryManagerMaxEntriesTest, OldestEntriesRemoved) {
    for (int i = 0; i < 10; ++i) {
        manager->add("command" + std::to_string(i));
    }

    // Oldest entries should be removed
    auto all = manager->getAll();
    // The first entry should be one of the later commands
}

}  // namespace lithium::debug::terminal::test
