#include <gtest/gtest.h>
#include <sstream>
#include "target/reader.hpp"

namespace lithium::target::test {

class DictReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default setup with standard CSV dialect
        fieldnames = {"field1", "field2", "field3"};
        dialect = Dialect(',', '"', true, false, "\n", Quoting::MINIMAL);
    }

    // Helper method to create DictReader instance
    auto createReader(const std::string& data) -> DictReader {
        input = std::make_unique<std::stringstream>(data);
        return DictReader(*input, fieldnames, dialect);
    }

    std::vector<std::string> fieldnames;
    Dialect dialect;
    std::unique_ptr<std::stringstream> input;
};

TEST_F(DictReaderTest, ParseEmptyLine) {
    auto reader = createReader("");
    std::unordered_map<std::string, std::string> row;
    EXPECT_FALSE(reader.next(row));
}

TEST_F(DictReaderTest, ParseSingleField) {
    auto reader = createReader("value1");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "value1");
}

TEST_F(DictReaderTest, ParseMultipleFields) {
    auto reader = createReader("value1,value2,value3");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "value1");
    EXPECT_EQ(row["field2"], "value2");
    EXPECT_EQ(row["field3"], "value3");
}

TEST_F(DictReaderTest, ParseQuotedFields) {
    auto reader =
        createReader("\"quoted value\",normal value,\"another quoted\"");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "quoted value");
    EXPECT_EQ(row["field2"], "normal value");
    EXPECT_EQ(row["field3"], "another quoted");
}

TEST_F(DictReaderTest, ParseFieldsWithDelimiters) {
    auto reader =
        createReader("\"value,with,commas\",normal value,\"last,field\"");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "value,with,commas");
    EXPECT_EQ(row["field2"], "normal value");
    EXPECT_EQ(row["field3"], "last,field");
}

TEST_F(DictReaderTest, ParseDoubleQuotes) {
    auto reader =
        createReader("\"quoted \"\"value\"\"\",normal,\"\"\"quoted\"\"\"");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "quoted \"value\"");
    EXPECT_EQ(row["field2"], "normal");
    EXPECT_EQ(row["field3"], "\"quoted\"");
}

TEST_F(DictReaderTest, ParseEmptyFields) {
    auto reader = createReader("value1,,value3");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "value1");
    EXPECT_EQ(row["field2"], "");
    EXPECT_EQ(row["field3"], "value3");
}

TEST_F(DictReaderTest, ParseWhitespaceFields) {
    auto reader = createReader(" value1 , value2 , value3 ");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "value1");
    EXPECT_EQ(row["field2"], "value2");
    EXPECT_EQ(row["field3"], "value3");
}

TEST_F(DictReaderTest, ParseMixedQuotedAndUnquoted) {
    auto reader = createReader("normal,\"quoted value\",normal again");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "normal");
    EXPECT_EQ(row["field2"], "quoted value");
    EXPECT_EQ(row["field3"], "normal again");
}

TEST_F(DictReaderTest, HandleMalformedInput) {
    auto reader = createReader("\"unclosed quote,value2,value3");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(
        reader.next(row));  // Should handle error based on dialect settings
    EXPECT_EQ(reader.getLastError(), CSVError::INVALID_FORMAT);
}

TEST_F(DictReaderTest, HandleSpecialCharacters) {
    auto reader =
        createReader("\"line\nbreak\",\"tab\tchar\",\"return\rchar\"");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "line\nbreak");
    EXPECT_EQ(row["field2"], "tab\tchar");
    EXPECT_EQ(row["field3"], "return\rchar");
}

TEST_F(DictReaderTest, HandleDifferentDialects) {
    dialect.delimiter = ';';
    dialect.quotechar = '\'';
    auto reader = createReader("value1;'quoted;value';value3");
    std::unordered_map<std::string, std::string> row;
    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "value1");
    EXPECT_EQ(row["field2"], "quoted;value");
    EXPECT_EQ(row["field3"], "value3");
}

TEST_F(DictReaderTest, HandleMultipleRows) {
    auto reader = createReader("a1,b1,c1\na2,b2,c2\na3,b3,c3");
    std::unordered_map<std::string, std::string> row;

    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "a1");
    EXPECT_EQ(row["field2"], "b1");
    EXPECT_EQ(row["field3"], "c1");

    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "a2");
    EXPECT_EQ(row["field2"], "b2");
    EXPECT_EQ(row["field3"], "c2");

    EXPECT_TRUE(reader.next(row));
    EXPECT_EQ(row["field1"], "a3");
    EXPECT_EQ(row["field2"], "b3");
    EXPECT_EQ(row["field3"], "c3");

    EXPECT_FALSE(reader.next(row));
}

}  // namespace lithium::target::test