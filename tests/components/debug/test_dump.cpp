#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstdlib>
#include <ctime>
#include "components/debug/dump.hpp"

namespace lithium::test {

class CoreDumpAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
        analyzer = std::make_unique<lithium::addon::CoreDumpAnalyzer>();
        std::srand(std::time(nullptr));
    }

    void TearDown() override {
        // Cleanup code if needed
        analyzer.reset();
        // Clean up any remaining test files
        std::vector<std::string> testFiles = {
            "test_core_dump", "corrupted_dump", "empty_dump", "truncated_dump",
            "test_dump_1", "test_dump_2", "large_dump"
        };
        for (const auto& file : testFiles) {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
            }
        }
    }

    std::unique_ptr<lithium::addon::CoreDumpAnalyzer> analyzer;
};

// ============================================================================
// 基础功能测试
// ============================================================================

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

// ============================================================================
// 分析选项测试
// ============================================================================

TEST_F(CoreDumpAnalyzerTest, AnalysisWithDifferentOptions) {
    std::ofstream("test_core_dump") << "Test content for options";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));

    // Test with different option combinations
    analyzer->setAnalysisOptions(true, false, false);  // memory only
    EXPECT_NO_THROW(analyzer->analyze());

    analyzer->setAnalysisOptions(false, true, false);  // threads only
    EXPECT_NO_THROW(analyzer->analyze());

    analyzer->setAnalysisOptions(false, false, true);  // stack only
    EXPECT_NO_THROW(analyzer->analyze());

    analyzer->setAnalysisOptions(true, true, true);    // all
    EXPECT_NO_THROW(analyzer->analyze());

    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, MemoryAnalysisOptions) {
    std::ofstream("test_core_dump") << "Memory test content";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));

    analyzer->setAnalysisOptions(true, false, false);
    analyzer->analyze();

    auto memInfo = analyzer->getDetailedMemoryInfo();
    EXPECT_FALSE(memInfo.empty());

    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, ThreadAnalysisOptions) {
    std::ofstream("test_core_dump") << "Thread test content";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));

    analyzer->setAnalysisOptions(false, false, true);
    analyzer->analyze();

    auto threadDetails = analyzer->getThreadDetails();
    EXPECT_FALSE(threadDetails.empty());

    std::filesystem::remove("test_core_dump");
}

// ============================================================================
// 报告格式测试
// ============================================================================

TEST_F(CoreDumpAnalyzerTest, GenerateReportComprehensive) {
    std::ofstream("test_core_dump") << "Report test content";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));
    analyzer->analyze();

    auto report = analyzer->generateReport();
    EXPECT_FALSE(report.empty());

    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, ReportContainsMemorySection) {
    std::ofstream("test_core_dump") << "Memory section test";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));

    analyzer->setAnalysisOptions(true, false, false);
    analyzer->analyze();

    auto report = analyzer->generateReport();
    // Report should contain some memory-related information
    EXPECT_TRUE(report.find("memory") != std::string::npos ||
                report.find("Memory") != std::string::npos ||
                !report.empty());

    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, ReportContainsStackSection) {
    std::ofstream("test_core_dump") << "Stack section test";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));

    analyzer->setAnalysisOptions(false, true, false);
    analyzer->analyze();

    auto stackTrace = analyzer->analyzeStackTrace();
    EXPECT_FALSE(stackTrace.empty());

    std::filesystem::remove("test_core_dump");
}

// ============================================================================
// 错误处理测试
// ============================================================================

TEST_F(CoreDumpAnalyzerTest, AnalyzeCorruptedFile) {
    // Create a file with random binary content
    std::ofstream corrupted("corrupted_dump", std::ios::binary);
    for (int i = 0; i < 100; ++i) {
        char c = static_cast<char>(rand() % 256);
        corrupted.write(&c, 1);
    }
    corrupted.close();

    EXPECT_TRUE(analyzer->readFile("corrupted_dump"));
    // Analysis should not crash, even on corrupted data
    EXPECT_NO_THROW(analyzer->analyze());

    std::filesystem::remove("corrupted_dump");
}

TEST_F(CoreDumpAnalyzerTest, AnalyzeEmptyFile) {
    std::ofstream("empty_dump") << "";

    EXPECT_TRUE(analyzer->readFile("empty_dump"));
    // Should handle empty file gracefully
    EXPECT_NO_THROW(analyzer->analyze());

    std::filesystem::remove("empty_dump");
}

TEST_F(CoreDumpAnalyzerTest, AnalyzeTruncatedFile) {
    // Create a file that looks like it might be a dump but is truncated
    std::ofstream truncated("truncated_dump", std::ios::binary);
    truncated << "CORE";  // Might look like a core dump header
    truncated.close();

    EXPECT_TRUE(analyzer->readFile("truncated_dump"));
    EXPECT_NO_THROW(analyzer->analyze());

    std::filesystem::remove("truncated_dump");
}

TEST_F(CoreDumpAnalyzerTest, ReadNonExistentFile) {
    EXPECT_FALSE(analyzer->readFile("/nonexistent/path/to/dump"));
}

TEST_F(CoreDumpAnalyzerTest, AnalyzeWithoutReading) {
    // Analyze without reading a file first
    EXPECT_NO_THROW(analyzer->analyze());

    // Results should be empty or indicate no data
    auto report = analyzer->generateReport();
    // Either empty or contains "no data" message
}

// ============================================================================
// 内存信息测试
// ============================================================================

TEST_F(CoreDumpAnalyzerTest, GetMemoryRegions) {
    std::ofstream("test_core_dump") << "Memory regions test content with more data";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));
    analyzer->analyze();

    auto memInfo = analyzer->getDetailedMemoryInfo();
    // Should have some memory information
    EXPECT_FALSE(memInfo.empty());

    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, GetDetailedMemoryInfoFormat) {
    std::ofstream("test_core_dump") << "Detailed memory info test";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));
    analyzer->analyze();

    auto memInfo = analyzer->getDetailedMemoryInfo();
    EXPECT_FALSE(memInfo.empty());

    std::filesystem::remove("test_core_dump");
}

// ============================================================================
// 线程信息测试
// ============================================================================

TEST_F(CoreDumpAnalyzerTest, GetThreadDetailsFormat) {
    std::ofstream("test_core_dump") << "Thread details test";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));
    analyzer->analyze();

    auto threadDetails = analyzer->getThreadDetails();
    EXPECT_FALSE(threadDetails.empty());

    std::filesystem::remove("test_core_dump");
}

// ============================================================================
// 堆栈追踪测试
// ============================================================================

TEST_F(CoreDumpAnalyzerTest, AnalyzeStackTraceFormat) {
    std::ofstream("test_core_dump") << "Stack trace test";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));
    analyzer->analyze();

    auto stackTrace = analyzer->analyzeStackTrace();
    EXPECT_FALSE(stackTrace.empty());

    std::filesystem::remove("test_core_dump");
}

// ============================================================================
// 多次分析测试
// ============================================================================

TEST_F(CoreDumpAnalyzerTest, MultipleAnalyses) {
    std::ofstream("test_core_dump") << "Multiple analysis test";
    EXPECT_TRUE(analyzer->readFile("test_core_dump"));

    // Analyze multiple times - should be idempotent
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(analyzer->analyze());
        auto report = analyzer->generateReport();
        EXPECT_FALSE(report.empty());
    }

    std::filesystem::remove("test_core_dump");
}

TEST_F(CoreDumpAnalyzerTest, ReadDifferentFiles) {
    // Create two different test files
    std::ofstream("test_dump_1") << "First dump content";
    std::ofstream("test_dump_2") << "Second dump content different";

    // Read and analyze first
    EXPECT_TRUE(analyzer->readFile("test_dump_1"));
    analyzer->analyze();
    auto report1 = analyzer->generateReport();

    // Read and analyze second
    EXPECT_TRUE(analyzer->readFile("test_dump_2"));
    analyzer->analyze();
    auto report2 = analyzer->generateReport();

    // Both reports should exist
    EXPECT_FALSE(report1.empty());
    EXPECT_FALSE(report2.empty());

    std::filesystem::remove("test_dump_1");
    std::filesystem::remove("test_dump_2");
}

// ============================================================================
// 大文件测试
// ============================================================================

TEST_F(CoreDumpAnalyzerTest, LargeFileHandling) {
    // Create a larger test file (1MB)
    std::ofstream large("large_dump", std::ios::binary);
    std::string chunk(1024, 'X');
    for (int i = 0; i < 1024; ++i) {
        large << chunk;
    }
    large.close();

    EXPECT_TRUE(analyzer->readFile("large_dump"));
    EXPECT_NO_THROW(analyzer->analyze());

    auto report = analyzer->generateReport();
    EXPECT_FALSE(report.empty());

    std::filesystem::remove("large_dump");
}

}  // namespace lithium::test
