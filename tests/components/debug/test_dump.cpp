#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include "components/debug/dump.hpp"

namespace lithium::test {

class CoreDumpAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
        analyzer = std::make_unique<lithium::addon::CoreDumpAnalyzer>();
    }

    void TearDown() override {
        // Cleanup code if needed
        analyzer.reset();
    }

    std::unique_ptr<lithium::addon::CoreDumpAnalyzer> analyzer;
};

TEST_F(CoreDumpAnalyzerTest, Constructor) { EXPECT_NE(analyzer, nullptr); }

TEST_F(CoreDumpAnalyzerTest, ReadFileSuccess) {
    std::ofstream("test_core_dump") << "Test content";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));
    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, ReadFileFailure) {
    EXPECT_FALSE(analyzer->readFile("non_existent_file"));
}

TEST_F(CoreDumpAnalyzerTest, Analyze) {
    std::ofstream("test_core_dump") << "Test content";
    analyzer->readFile("test_core_dump");
    EXPECT_NO_THROW(analyzer->analyze());
    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, GetDetailedMemoryInfo) {
    std::ofstream("test_core_dump") << "Test content";
    analyzer->readFile("test_core_dump");
    analyzer->analyze();
    auto memoryInfo = analyzer->getDetailedMemoryInfo();
    EXPECT_FALSE(memoryInfo.empty());
    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, AnalyzeStackTrace) {
    std::ofstream("test_core_dump") << "Test content";
    analyzer->readFile("test_core_dump");
    analyzer->analyze();
    auto stackTrace = analyzer->analyzeStackTrace();
    EXPECT_FALSE(stackTrace.empty());
    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, GetThreadDetails) {
    std::ofstream("test_core_dump") << "Test content";
    analyzer->readFile("test_core_dump");
    analyzer->analyze();
    auto threadDetails = analyzer->getThreadDetails();
    EXPECT_FALSE(threadDetails.empty());
    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, GenerateReport) {
    std::ofstream("test_core_dump") << "Test content";
    analyzer->readFile("test_core_dump");
    analyzer->analyze();
    auto report = analyzer->generateReport();
    EXPECT_FALSE(report.empty());
    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, SetAnalysisOptions) {
    analyzer->setAnalysisOptions(true, true, true);
    // No direct way to verify, but ensure no exceptions are thrown
}

}  // namespace lithium::test
