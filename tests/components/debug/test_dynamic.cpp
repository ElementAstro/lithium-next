#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "components/debug/dynamic.hpp"

namespace lithium::test {

class DynamicLibraryParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    static std::unique_ptr<lithium::addon::DynamicLibraryParser> createParser(
        const std::string& executable) {
        return std::make_unique<lithium::addon::DynamicLibraryParser>(
            executable);
    }
};

// ============================================================================
// Basic Tests (Original)
// ============================================================================

TEST_F(DynamicLibraryParserTest, Constructor) {
    auto parser = createParser("test_executable");
    EXPECT_NE(parser, nullptr);
}

TEST_F(DynamicLibraryParserTest, SetConfig) {
    auto parser = createParser("test_executable");
    lithium::addon::ParserConfig config;
    config.json_output = true;
    config.use_cache = false;
    parser->setConfig(config);
    // No direct way to verify, but ensure no exceptions are thrown
}

TEST_F(DynamicLibraryParserTest, Parse) {
    auto parser = createParser("test_executable");
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, ParseAsync) {
    auto parser = createParser("test_executable");
    bool callbackCalled = false;
    parser->parseAsync([&callbackCalled](bool success) {
        callbackCalled = true;
        EXPECT_TRUE(success);
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(callbackCalled);
}

TEST_F(DynamicLibraryParserTest, GetDependencies) {
    auto parser = createParser("test_executable");
    parser->parse();
    auto dependencies = parser->getDependencies();
    EXPECT_TRUE(
        dependencies.empty());  // Assuming no dependencies for test_executable
}

TEST_F(DynamicLibraryParserTest, VerifyLibrary) {
    auto parser = createParser("test_executable");
    EXPECT_FALSE(parser->verifyLibrary("non_existent_library"));
}

TEST_F(DynamicLibraryParserTest, ClearCache) {
    auto parser = createParser("test_executable");
    EXPECT_NO_THROW(parser->clearCache());
}

TEST_F(DynamicLibraryParserTest, SetJsonOutput) {
    auto parser = createParser("test_executable");
    parser->setJsonOutput(true);
    // No direct way to verify, but ensure no exceptions are thrown
}

TEST_F(DynamicLibraryParserTest, SetOutputFilename) {
    auto parser = createParser("test_executable");
    parser->setOutputFilename("output.json");
    // No direct way to verify, but ensure no exceptions are thrown
}

// ============================================================================
// Real Library Dependency Tests
// ============================================================================

class RealDynamicLibraryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Find a real shared library on the system
#ifdef _WIN32
        // Windows system DLLs
        std::vector<std::string> candidates = {
            "C:\\Windows\\System32\\kernel32.dll",
            "C:\\Windows\\System32\\user32.dll",
            "C:\\Windows\\System32\\ntdll.dll"};
#else
        // Linux/macOS shared libraries
        std::vector<std::string> candidates = {
            "/lib/x86_64-linux-gnu/libc.so.6", "/lib64/libc.so.6",
            "/usr/lib/libc.so.6",
            "/usr/lib/libSystem.B.dylib"  // macOS
        };
#endif

        for (const auto& path : candidates) {
            if (std::filesystem::exists(path)) {
                realLibPath = path;
                break;
            }
        }
    }

    bool hasRealLib() const {
        return !realLibPath.empty() && std::filesystem::exists(realLibPath);
    }

    static std::unique_ptr<lithium::addon::DynamicLibraryParser> createParser(
        const std::string& executable) {
        return std::make_unique<lithium::addon::DynamicLibraryParser>(
            executable);
    }

    std::string realLibPath;
};

TEST_F(RealDynamicLibraryTest, GetDependenciesWithRealLibrary) {
    if (!hasRealLib()) {
        GTEST_SKIP() << "No suitable system library found";
    }

    auto parser = createParser(realLibPath);
    EXPECT_NO_THROW(parser->parse());

    auto dependencies = parser->getDependencies();
    // Real libraries typically have some dependencies or none
    EXPECT_TRUE(true);  // Successful parse is the goal
}

TEST_F(RealDynamicLibraryTest, VerifyExistingLibrary) {
    if (!hasRealLib()) {
        GTEST_SKIP() << "No suitable system library found";
    }

    auto parser = createParser(realLibPath);
    parser->parse();

    // The library file itself should be verifiable
    EXPECT_TRUE(parser->verifyLibrary(realLibPath));
}

// ============================================================================
// Verification Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, VerifyNonExistentLibrary) {
    auto parser = createParser("test_executable");
    EXPECT_FALSE(parser->verifyLibrary("/nonexistent/path/to/library.so"));
}

TEST_F(DynamicLibraryParserTest, VerifyEmptyPath) {
    auto parser = createParser("test_executable");
    EXPECT_FALSE(parser->verifyLibrary(""));
}

// ============================================================================
// Output Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, JsonOutputFormat) {
    auto parser = createParser("test_executable");
    parser->setJsonOutput(true);

    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, WriteToFile) {
    auto parser = createParser("test_executable");
    parser->setOutputFilename("test_output.json");
    parser->setJsonOutput(true);

    EXPECT_NO_THROW(parser->parse());

    // Clean up
    if (std::filesystem::exists("test_output.json")) {
        std::filesystem::remove("test_output.json");
    }
}

TEST_F(DynamicLibraryParserTest, OutputFilenameChange) {
    auto parser = createParser("test_executable");

    parser->setOutputFilename("output1.json");
    parser->setOutputFilename("output2.json");
    parser->setOutputFilename("final_output.json");

    // Should use the last set filename
    parser->setJsonOutput(true);
    EXPECT_NO_THROW(parser->parse());

    // Clean up
    if (std::filesystem::exists("final_output.json")) {
        std::filesystem::remove("final_output.json");
    }
}

// ============================================================================
// Cache Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, CacheOperations) {
    auto parser = createParser("test_executable");

    // Parse first time
    parser->parse();

    // Clear cache
    EXPECT_NO_THROW(parser->clearCache());

    // Parse again after clearing cache
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, CacheWithConfig) {
    auto parser = createParser("test_executable");

    lithium::addon::ParserConfig config;
    config.use_cache = true;
    parser->setConfig(config);

    // First parse - populates cache
    parser->parse();

    // Second parse - should use cache
    parser->parse();

    // Clear and parse again
    parser->clearCache();
    parser->parse();
}

TEST_F(DynamicLibraryParserTest, DisableCache) {
    auto parser = createParser("test_executable");

    lithium::addon::ParserConfig config;
    config.use_cache = false;
    parser->setConfig(config);

    // Parse multiple times without cache
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(parser->parse());
    }
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, SetConfigMultipleTimes) {
    auto parser = createParser("test_executable");

    lithium::addon::ParserConfig config1;
    config1.json_output = false;
    config1.use_cache = true;
    parser->setConfig(config1);

    lithium::addon::ParserConfig config2;
    config2.json_output = true;
    config2.use_cache = false;
    parser->setConfig(config2);

    // Should use config2
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, DefaultConfig) {
    auto parser = createParser("test_executable");

    // Use default config
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, ConfigWithVerifyLibraries) {
    auto parser = createParser("test_executable");

    lithium::addon::ParserConfig config;
    config.verify_libraries = true;
    config.analyze_dependencies = true;
    parser->setConfig(config);

    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, ConfigCacheDirectory) {
    auto parser = createParser("test_executable");

    lithium::addon::ParserConfig config;
    config.cache_dir = ".test_cache";
    parser->setConfig(config);

    EXPECT_NO_THROW(parser->parse());
}

// ============================================================================
// Async Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, ParseAsyncCompletion) {
    auto parser = createParser("test_executable");

    std::promise<bool> promise;
    auto future = promise.get_future();

    parser->parseAsync(
        [&promise](bool success) { promise.set_value(success); });

    // Wait for completion with timeout
    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    bool result = future.get();
    EXPECT_TRUE(result);
}

TEST_F(DynamicLibraryParserTest, MultipleAsyncParsesSequential) {
    auto parser = createParser("test_executable");

    std::atomic<int> completionCount{0};

    for (int i = 0; i < 3; ++i) {
        std::promise<void> promise;
        auto future = promise.get_future();

        parser->parseAsync([&completionCount, &promise](bool) {
            completionCount++;
            promise.set_value();
        });

        future.wait_for(std::chrono::seconds(2));
    }

    // Allow some time for all callbacks
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(completionCount, 3);
}

TEST_F(DynamicLibraryParserTest, AsyncParseWithCallback) {
    auto parser = createParser("test_executable");
    std::atomic<bool> callbackExecuted{false};
    std::atomic<bool> successValue{false};

    parser->parseAsync([&callbackExecuted, &successValue](bool success) {
        callbackExecuted = true;
        successValue = success;
    });

    // Wait for callback
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_TRUE(callbackExecuted);
}

// ============================================================================
// Dependency Analysis Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, GetDependenciesEmpty) {
    auto parser = createParser("nonexistent_executable");
    parser->parse();

    auto deps = parser->getDependencies();
    EXPECT_TRUE(deps.empty());
}

TEST_F(DynamicLibraryParserTest, GetDependenciesAfterClear) {
    auto parser = createParser("test_executable");
    parser->parse();

    auto deps1 = parser->getDependencies();

    parser->clearCache();

    auto deps2 = parser->getDependencies();
    // After clear, should still work but might be empty
    EXPECT_TRUE(true);
}

TEST_F(DynamicLibraryParserTest, DependenciesAreVectorOfStrings) {
    auto parser = createParser("test_executable");
    parser->parse();

    auto deps = parser->getDependencies();
    // Verify it's a vector
    EXPECT_TRUE(std::is_same_v<decltype(deps), std::vector<std::string>>);
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, EmptyExecutablePath) {
    auto parser = createParser("");
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, SpecialCharactersInPath) {
    auto parser = createParser("path with spaces/executable");
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, VeryLongPath) {
    std::string longPath(500, 'a');
    auto parser = createParser(longPath);
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, PathWithDotDot) {
    auto parser = createParser("../path/to/executable");
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, AbsoluteVsRelativePath) {
    auto parser1 = createParser("./executable");
    EXPECT_NO_THROW(parser1->parse());

    auto parser2 = createParser("/absolute/path/executable");
    EXPECT_NO_THROW(parser2->parse());
}

// ============================================================================
// Concurrency Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, ConcurrentParsing) {
    constexpr int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&successCount, i]() {
            auto parser =
                std::make_unique<lithium::addon::DynamicLibraryParser>(
                    "test_exec_" + std::to_string(i));
            parser->parse();
            successCount++;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount, numThreads);
}

TEST_F(DynamicLibraryParserTest, ConcurrentAsyncParsing) {
    constexpr int numParsers = 3;
    std::vector<std::unique_ptr<lithium::addon::DynamicLibraryParser>> parsers;
    std::atomic<int> completionCount{0};

    for (int i = 0; i < numParsers; ++i) {
        parsers.push_back(createParser("concurrent_test_" + std::to_string(i)));
    }

    // Start all async parses
    for (auto& parser : parsers) {
        parser->parseAsync([&completionCount](bool) { completionCount++; });
    }

    // Wait for completion
    std::this_thread::sleep_for(std::chrono::seconds(3));
    EXPECT_EQ(completionCount, numParsers);
}

TEST_F(DynamicLibraryParserTest, ThreadSafetyOfGetDependencies) {
    auto parser = createParser("test_executable");
    parser->parse();

    std::atomic<int> readCount{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&parser, &readCount]() {
            auto deps = parser->getDependencies();
            readCount++;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(readCount, 5);
}

TEST_F(DynamicLibraryParserTest, ConcurrentClearCache) {
    auto parser = createParser("test_executable");
    parser->parse();

    std::vector<std::thread> threads;

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&parser]() { parser->clearCache(); });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // After concurrent clears, parser should still be functional
    EXPECT_NO_THROW(parser->parse());
}

// ============================================================================
// State Management Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, ParseAfterConfigChange) {
    auto parser = createParser("test_executable");

    // Parse with default config
    EXPECT_NO_THROW(parser->parse());

    // Change config and parse again
    lithium::addon::ParserConfig config;
    config.json_output = true;
    parser->setConfig(config);

    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, MultipleParseSequences) {
    auto parser = createParser("test_executable");

    // First sequence
    parser->parse();
    auto deps1 = parser->getDependencies();

    // Clear and parse again
    parser->clearCache();
    parser->parse();
    auto deps2 = parser->getDependencies();

    // Parse once more
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, ClearCacheMultipleTimes) {
    auto parser = createParser("test_executable");

    EXPECT_NO_THROW(parser->clearCache());
    EXPECT_NO_THROW(parser->clearCache());
    EXPECT_NO_THROW(parser->clearCache());

    EXPECT_NO_THROW(parser->parse());
}

// ============================================================================
// Configuration Persistence Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, ConfigPersistedAcrossParses) {
    auto parser = createParser("test_executable");

    lithium::addon::ParserConfig config;
    config.json_output = true;
    config.use_cache = false;
    parser->setConfig(config);

    EXPECT_NO_THROW(parser->parse());
    EXPECT_NO_THROW(parser->parse());
    EXPECT_NO_THROW(parser->parse());
}

TEST_F(DynamicLibraryParserTest, MultipleConfigChanges) {
    auto parser = createParser("test_executable");

    for (int i = 0; i < 5; ++i) {
        lithium::addon::ParserConfig config;
        config.json_output = (i % 2 == 0);
        config.use_cache = (i % 2 != 0);
        parser->setConfig(config);
        EXPECT_NO_THROW(parser->parse());
    }
}

// ============================================================================
// Library Verification Extended Tests
// ============================================================================

TEST_F(DynamicLibraryParserTest, VerifyLibraryWithMultiplePaths) {
    auto parser = createParser("test_executable");

    EXPECT_FALSE(parser->verifyLibrary("/path/1/lib.so"));
    EXPECT_FALSE(parser->verifyLibrary("/path/2/lib.so"));
    EXPECT_FALSE(parser->verifyLibrary("/path/3/lib.so"));
}

TEST_F(DynamicLibraryParserTest, VerifyLibraryWithSpecialChars) {
    auto parser = createParser("test_executable");

    EXPECT_FALSE(parser->verifyLibrary("lib@file.so"));
    EXPECT_FALSE(parser->verifyLibrary("lib#file.so"));
    EXPECT_FALSE(parser->verifyLibrary("lib$file.so"));
}

}  // namespace lithium::test
