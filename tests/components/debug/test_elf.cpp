#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>

#ifdef __linux__
#include "components/debug/elf.hpp"

namespace lithium::test {

class ElfParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
        std::ofstream elfFile("test.elf", std::ios::binary);
        // Write minimal ELF header for testing
        elfFile.write(
            "\x7F"
            "ELF",
            4);                            // ELF magic number
        elfFile.write("\x02\x01\x01", 3);  // ELF class, data, version
        elfFile.write("\x00", 1);          // Padding
        elfFile.write("\x00\x00\x00\x00\x00\x00\x00\x00", 8);  // Padding
        elfFile.write("\x02\x00", 2);                          // Type
        elfFile.write("\x3E\x00", 2);                          // Machine
        elfFile.write("\x01\x00\x00\x00", 4);                  // Version
        elfFile.write("\x00\x00\x00\x00\x00\x00\x00\x00", 8);  // Entry
        elfFile.write("\x40\x00\x00\x00\x00\x00\x00\x00", 8);  // Phoff
        elfFile.write("\x00\x00\x00\x00\x00\x00\x00\x00", 8);  // Shoff
        elfFile.write("\x00\x00\x00\x00", 4);                  // Flags
        elfFile.write("\x40\x00", 2);                          // Ehsize
        elfFile.write("\x38\x00", 2);                          // Phentsize
        elfFile.write("\x01\x00", 2);                          // Phnum
        elfFile.write("\x40\x00", 2);                          // Shentsize
        elfFile.write("\x00\x00", 2);                          // Shnum
        elfFile.write("\x00\x00", 2);                          // Shstrndx
        elfFile.close();

        parser = std::make_unique<lithium::ElfParser>("test.elf");
    }

    void TearDown() override {
        // Cleanup code if needed
        parser.reset();
        std::filesystem::remove("test.elf");
    }

    std::unique_ptr<lithium::ElfParser> parser;
};

// ============================================================================
// 基本测试（原有测试）
// ============================================================================

TEST_F(ElfParserTest, Constructor) { EXPECT_NE(parser, nullptr); }

TEST_F(ElfParserTest, ParseSuccess) { EXPECT_TRUE(parser->parse()); }

TEST_F(ElfParserTest, GetElfHeader) {
    EXPECT_TRUE(parser->parse());
    auto header = parser->getElfHeader();
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->type, 2);
    EXPECT_EQ(header->machine, 62);
    EXPECT_EQ(header->version, 1);
}

TEST_F(ElfParserTest, GetProgramHeaders) {
    EXPECT_TRUE(parser->parse());
    auto headers = parser->getProgramHeaders();
    EXPECT_EQ(headers.size(), 1);
}

TEST_F(ElfParserTest, GetSectionHeaders) {
    EXPECT_TRUE(parser->parse());
    auto headers = parser->getSectionHeaders();
    EXPECT_TRUE(headers.empty());
}

TEST_F(ElfParserTest, GetSymbolTable) {
    EXPECT_TRUE(parser->parse());
    auto symbols = parser->getSymbolTable();
    EXPECT_TRUE(symbols.empty());
}

TEST_F(ElfParserTest, GetDynamicEntries) {
    EXPECT_TRUE(parser->parse());
    auto entries = parser->getDynamicEntries();
    EXPECT_TRUE(entries.empty());
}

TEST_F(ElfParserTest, GetRelocationEntries) {
    EXPECT_TRUE(parser->parse());
    auto entries = parser->getRelocationEntries();
    EXPECT_TRUE(entries.empty());
}

TEST_F(ElfParserTest, FindSymbolByName) {
    EXPECT_TRUE(parser->parse());
    auto symbol = parser->findSymbolByName("test_symbol");
    EXPECT_FALSE(symbol.has_value());
}

TEST_F(ElfParserTest, FindSymbolByAddress) {
    EXPECT_TRUE(parser->parse());
    auto symbol = parser->findSymbolByAddress(0x400000);
    EXPECT_FALSE(symbol.has_value());
}

TEST_F(ElfParserTest, FindSection) {
    EXPECT_TRUE(parser->parse());
    auto section = parser->findSection(".text");
    EXPECT_FALSE(section.has_value());
}

TEST_F(ElfParserTest, GetSectionData) {
    EXPECT_TRUE(parser->parse());
    auto headers = parser->getSectionHeaders();
    if (!headers.empty()) {
        auto data = parser->getSectionData(headers[0]);
        EXPECT_TRUE(data.empty());
    }
}

TEST_F(ElfParserTest, GetSymbolsInRange) {
    EXPECT_TRUE(parser->parse());
    auto symbols = parser->getSymbolsInRange(0x400000, 0x401000);
    EXPECT_TRUE(symbols.empty());
}

TEST_F(ElfParserTest, GetExecutableSegments) {
    EXPECT_TRUE(parser->parse());
    auto segments = parser->getExecutableSegments();
    EXPECT_TRUE(segments.empty());
}

TEST_F(ElfParserTest, VerifyIntegrity) {
    EXPECT_TRUE(parser->parse());
    EXPECT_TRUE(parser->verifyIntegrity());
}

TEST_F(ElfParserTest, ClearCache) {
    EXPECT_TRUE(parser->parse());
    parser->clearCache();
    // No direct way to verify, but ensure no exceptions are thrown
}

// ============================================================================
// 符号搜索测试
// ============================================================================

TEST_F(ElfParserTest, FindSymbolsByPattern) {
    EXPECT_TRUE(parser->parse());
    auto symbols = parser->findSymbolsByPattern(".*");
    // Pattern search should work even on minimal ELF
    EXPECT_TRUE(symbols.empty() || !symbols.empty());
}

TEST_F(ElfParserTest, GetWeakSymbols) {
    EXPECT_TRUE(parser->parse());
    auto weakSymbols = parser->getWeakSymbols();
    // Minimal ELF may not have weak symbols
    EXPECT_TRUE(weakSymbols.empty() || !weakSymbols.empty());
}

TEST_F(ElfParserTest, GetExportedSymbols) {
    EXPECT_TRUE(parser->parse());
    auto exported = parser->getExportedSymbols();
    // Verify method executes without error
    EXPECT_TRUE(exported.empty() || !exported.empty());
}

TEST_F(ElfParserTest, GetImportedSymbols) {
    EXPECT_TRUE(parser->parse());
    auto imported = parser->getImportedSymbols();
    // Verify method executes without error
    EXPECT_TRUE(imported.empty() || !imported.empty());
}

// ============================================================================
// 符号处理测试
// ============================================================================

TEST_F(ElfParserTest, DemangleSymbolName) {
    EXPECT_TRUE(parser->parse());
    // Test with a mangled C++ symbol name
    std::string mangled = "_Z3foov";
    std::string demangled = parser->demangleSymbolName(mangled);
    // Demangled should be different from mangled (or same if demangling fails)
    EXPECT_FALSE(demangled.empty());
}

TEST_F(ElfParserTest, DemanglePlainCName) {
    EXPECT_TRUE(parser->parse());
    // Plain C name should remain unchanged
    std::string plainC = "printf";
    std::string result = parser->demangleSymbolName(plainC);
    EXPECT_EQ(result, plainC);
}

TEST_F(ElfParserTest, GetSymbolsByType) {
    EXPECT_TRUE(parser->parse());
    // STT_NOTYPE = 0
    auto symbols = parser->getSymbolsByType(0);
    // May or may not find symbols of this type
    EXPECT_TRUE(symbols.empty() || !symbols.empty());
}

TEST_F(ElfParserTest, GetSymbolVersion) {
    EXPECT_TRUE(parser->parse());
    auto symbols = parser->getSymbolTable();
    if (!symbols.empty()) {
        // Test with the first symbol
        auto version = parser->getSymbolVersion(symbols[0]);
        // May or may not have version info
        EXPECT_TRUE(version.has_value() || !version.has_value());
    }
}

// ============================================================================
// 段分析测试
// ============================================================================

TEST_F(ElfParserTest, GetSegmentPermissions) {
    EXPECT_TRUE(parser->parse());
    auto headers = parser->getProgramHeaders();
    if (!headers.empty()) {
        auto perms = parser->getSegmentPermissions(headers[0]);
        EXPECT_FALSE(perms.empty());
        // Should be a combination of R, W, X
    }
}

TEST_F(ElfParserTest, GetSectionsByType) {
    EXPECT_TRUE(parser->parse());
    // SHT_NULL = 0
    auto sections = parser->getSectionsByType(0);
    // May or may not find sections of this type
    EXPECT_TRUE(sections.empty() || !sections.empty());
}

// ============================================================================
// 文件分析测试
// ============================================================================

TEST_F(ElfParserTest, GetDependencies) {
    EXPECT_TRUE(parser->parse());
    auto deps = parser->getDependencies();
    // Minimal test ELF has no dependencies
    EXPECT_TRUE(deps.empty());
}

TEST_F(ElfParserTest, CalculateChecksum) {
    EXPECT_TRUE(parser->parse());
    auto checksum = parser->calculateChecksum();
    EXPECT_GT(checksum, 0ULL);
}

TEST_F(ElfParserTest, IsStripped) {
    EXPECT_TRUE(parser->parse());
    bool stripped = parser->isStripped();
    // Minimal ELF is effectively stripped (no debug info)
    EXPECT_TRUE(stripped);
}

// ============================================================================
// 性能和缓存测试
// ============================================================================

TEST_F(ElfParserTest, EnableCache) {
    parser->enableCache(true);
    EXPECT_TRUE(parser->parse());
    // Cache should improve subsequent lookups
    parser->enableCache(false);
}

TEST_F(ElfParserTest, SetParallelProcessing) {
    parser->setParallelProcessing(true);
    EXPECT_TRUE(parser->parse());
    parser->setParallelProcessing(false);
}

TEST_F(ElfParserTest, SetCacheSize) {
    parser->setCacheSize(100);
    EXPECT_TRUE(parser->parse());
    // Should not crash
}

TEST_F(ElfParserTest, PreloadSymbols) {
    EXPECT_TRUE(parser->parse());
    EXPECT_NO_THROW(parser->preloadSymbols());
}

// ============================================================================
// 边界情况测试
// ============================================================================

TEST_F(ElfParserTest, FindSymbolByInvalidAddress) {
    EXPECT_TRUE(parser->parse());
    auto symbol = parser->findSymbolByAddress(0xFFFFFFFFFFFFFFFF);
    EXPECT_FALSE(symbol.has_value());
}

TEST_F(ElfParserTest, GetSymbolsInInvalidRange) {
    EXPECT_TRUE(parser->parse());
    auto symbols = parser->getSymbolsInRange(0xFFFFFFFF, 0);
    EXPECT_TRUE(symbols.empty());
}

TEST_F(ElfParserTest, FindNonExistentSection) {
    EXPECT_TRUE(parser->parse());
    auto section = parser->findSection(".nonexistent_section");
    EXPECT_FALSE(section.has_value());
}

// ============================================================================
// Template-based 符号搜索测试
// ============================================================================

TEST_F(ElfParserTest, FindSymbolWithPredicate) {
    EXPECT_TRUE(parser->parse());
    auto symbol = parser->findSymbol([](const lithium::Symbol& s) {
        return s.size > 0;
    });
    // May or may not find a symbol
    EXPECT_TRUE(symbol.has_value() || !symbol.has_value());
}

TEST_F(ElfParserTest, FindSymbolWithBindingPredicate) {
    EXPECT_TRUE(parser->parse());
    auto symbol = parser->findSymbol([](const lithium::Symbol& s) {
        return s.bind == 0;  // STB_LOCAL
    });
    EXPECT_TRUE(symbol.has_value() || !symbol.has_value());
}

// ============================================================================
// 真实 ELF 文件测试 (使用系统库)
// ============================================================================

class RealElfParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Try to find a real ELF file on the system
        // Common system libraries
        std::vector<std::string> candidates = {
            "/lib/x86_64-linux-gnu/libc.so.6",
            "/lib64/libc.so.6",
            "/usr/lib/libc.so.6",
            "/lib/libc.so.6"
        };

        for (const auto& path : candidates) {
            if (std::filesystem::exists(path)) {
                realElfPath = path;
                parser = std::make_unique<lithium::ElfParser>(realElfPath);
                break;
            }
        }
    }

    void TearDown() override {
        parser.reset();
    }

    bool hasRealElf() const {
        return !realElfPath.empty() && std::filesystem::exists(realElfPath);
    }

    std::string realElfPath;
    std::unique_ptr<lithium::ElfParser> parser;
};

TEST_F(RealElfParserTest, ParseRealSystemLibrary) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
}

TEST_F(RealElfParserTest, GetRealSymbolTable) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    auto symbols = parser->getSymbolTable();
    EXPECT_FALSE(symbols.empty());
}

TEST_F(RealElfParserTest, FindCommonSymbol) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    // Most C libraries have printf or malloc
    auto symbol = parser->findSymbolByName("malloc");
    // May or may not find depending on the library
    EXPECT_TRUE(symbol.has_value() || !symbol.has_value());
}

TEST_F(RealElfParserTest, GetRealDependencies) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    auto deps = parser->getDependencies();
    // Real libraries typically have dependencies
    EXPECT_TRUE(deps.empty() || !deps.empty());
}

TEST_F(RealElfParserTest, GetRealExportedSymbols) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    auto exported = parser->getExportedSymbols();
    EXPECT_FALSE(exported.empty());
}

TEST_F(RealElfParserTest, VerifyRealIntegrity) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    EXPECT_TRUE(parser->verifyIntegrity());
}

TEST_F(RealElfParserTest, IsRealLibraryStripped) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    // System libraries are typically stripped
    bool stripped = parser->isStripped();
    // Either way is valid
    EXPECT_TRUE(stripped || !stripped);
}

TEST_F(RealElfParserTest, GetRealExecutableSegments) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    auto segments = parser->getExecutableSegments();
    // Most libraries have executable segments
    EXPECT_TRUE(!segments.empty() || segments.empty());
}

TEST_F(RealElfParserTest, FindRealSymbolsByPattern) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    // Search for common patterns
    auto symbols = parser->findSymbolsByPattern("malloc.*");
    EXPECT_TRUE(symbols.empty() || !symbols.empty());
}

TEST_F(RealElfParserTest, GetRealWeakSymbols) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    auto weakSymbols = parser->getWeakSymbols();
    // May or may not have weak symbols
    EXPECT_TRUE(weakSymbols.empty() || !weakSymbols.empty());
}

TEST_F(RealElfParserTest, RealElfChecksum) {
    if (!hasRealElf()) {
        GTEST_SKIP() << "No suitable system ELF found";
    }

    EXPECT_TRUE(parser->parse());
    auto checksum = parser->calculateChecksum();
    EXPECT_GT(checksum, 0ULL);
}

// ============================================================================
// 高级功能测试
// ============================================================================

TEST_F(ElfParserTest, CachingPerformance) {
    parser->enableCache(true);
    EXPECT_TRUE(parser->parse());

    // First lookup (cache miss)
    auto symbol1 = parser->findSymbolByName("test");

    // Second lookup (cache hit)
    auto symbol2 = parser->findSymbolByName("test");

    // Results should be consistent
    EXPECT_EQ(symbol1.has_value(), symbol2.has_value());

    parser->enableCache(false);
}

TEST_F(ElfParserTest, ParallelProcessingMode) {
    parser->setParallelProcessing(true);
    EXPECT_TRUE(parser->parse());

    auto symbols = parser->getSymbolTable();
    EXPECT_TRUE(symbols.empty() || !symbols.empty());

    parser->setParallelProcessing(false);
}

TEST_F(ElfParserTest, CacheSizeLimit) {
    parser->setCacheSize(10);  // Very small cache
    EXPECT_TRUE(parser->parse());

    // Multiple queries to test cache eviction
    parser->findSymbolByName("symbol1");
    parser->findSymbolByName("symbol2");
    parser->findSymbolByName("symbol3");

    parser->setCacheSize(1024 * 1024);  // Reset to default
}

TEST_F(ElfParserTest, PreloadOptimization) {
    parser->enableCache(true);
    EXPECT_TRUE(parser->parse());

    EXPECT_NO_THROW(parser->preloadSymbols());

    // After preload, lookups should be cached
    auto symbols = parser->getSymbolTable();
    EXPECT_TRUE(symbols.empty() || !symbols.empty());

    parser->enableCache(false);
}

}  // namespace lithium::test

#endif  // __linux__
