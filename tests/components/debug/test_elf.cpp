#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <memory>

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

}  // namespace lithium::test
