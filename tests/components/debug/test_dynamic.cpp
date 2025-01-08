#include <gtest/gtest.h>
#include <memory>
#include <thread>

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

}  // namespace lithium::test
