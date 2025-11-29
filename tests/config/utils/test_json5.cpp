/**
 * @file test_json5.cpp
 * @brief Comprehensive unit tests for JSON5 parser utility
 */

#include <gtest/gtest.h>
#include "config/utils/json5.hpp"

using namespace lithium::internal;

namespace lithium::config::test {

class JSON5Test : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// RemoveComments Tests
// ============================================================================

TEST_F(JSON5Test, RemoveCommentsEmpty) {
    auto result = removeComments("");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(JSON5Test, RemoveCommentsSingleLineComment) {
    std::string input = R"({
        "key": "value" // this is a comment
    })";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().find("//"), std::string::npos);
    EXPECT_NE(result.value().find("key"), std::string::npos);
}

TEST_F(JSON5Test, RemoveCommentsMultiLineComment) {
    std::string input = R"({
        "key": "value" /* this is
        a multi-line comment */
    })";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().find("/*"), std::string::npos);
    EXPECT_EQ(result.value().find("*/"), std::string::npos);
    EXPECT_NE(result.value().find("key"), std::string::npos);
}

TEST_F(JSON5Test, RemoveCommentsMixed) {
    std::string input = R"({
        // single line comment
        "key1": "value1", /* inline comment */
        "key2": "value2"
        /* multi
           line
           comment */
    })";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().find("//"), std::string::npos);
    EXPECT_EQ(result.value().find("/*"), std::string::npos);
}

TEST_F(JSON5Test, RemoveCommentsPreserveStrings) {
    std::string input = R"({"key": "value with // comment-like text"})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("// comment-like"), std::string::npos);
}

TEST_F(JSON5Test, RemoveCommentsPreserveStringsMultiLine) {
    std::string input = R"({"key": "value with /* comment */ inside"})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("/* comment */"), std::string::npos);
}

TEST_F(JSON5Test, RemoveCommentsEscapedQuotes) {
    std::string input = R"({"key": "value with \"escaped\" quotes"})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("escaped"), std::string::npos);
}

TEST_F(JSON5Test, RemoveCommentsUnterminatedString) {
    std::string input = R"({"key": "unterminated string)";
    auto result = removeComments(input);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("Unterminated string"), std::string::npos);
}

TEST_F(JSON5Test, RemoveCommentsUnterminatedMultiLineComment) {
    std::string input = R"({"key": "value" /* unterminated comment)";
    auto result = removeComments(input);
    EXPECT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("Unterminated multi-line comment"), std::string::npos);
}

TEST_F(JSON5Test, RemoveCommentsNoComments) {
    std::string input = R"({"key": "value", "number": 42})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), input);
}

// ============================================================================
// ConvertJSON5toJSON Tests
// ============================================================================

TEST_F(JSON5Test, ConvertJSON5toJSONEmpty) {
    auto result = convertJSON5toJSON("");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(JSON5Test, ConvertJSON5toJSONUnquotedKeys) {
    std::string input = R"({key: "value"})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("\"key\""), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONMultipleUnquotedKeys) {
    std::string input = R"({key1: "value1", key2: "value2"})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("\"key1\""), std::string::npos);
    EXPECT_NE(result.value().find("\"key2\""), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONNestedUnquotedKeys) {
    std::string input = R"({outer: {inner: "value"}})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("\"outer\""), std::string::npos);
    EXPECT_NE(result.value().find("\"inner\""), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONUnderscoreInKey) {
    std::string input = R"({my_key: "value"})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("\"my_key\""), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONHyphenInKey) {
    std::string input = R"({my-key: "value"})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("\"my-key\""), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONNumberInKey) {
    std::string input = R"({key123: "value"})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("\"key123\""), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONPreserveQuotedKeys) {
    std::string input = R"({"already_quoted": "value"})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("\"already_quoted\""), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONWithComments) {
    std::string input = R"({
        // comment
        key: "value"
    })";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().find("//"), std::string::npos);
    EXPECT_NE(result.value().find("\"key\""), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONPreserveStringValues) {
    std::string input = R"({key: "value with spaces"})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("value with spaces"), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONPreserveNumbers) {
    std::string input = R"({key: 42})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("42"), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONPreserveBooleans) {
    std::string input = R"({key: true})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("true"), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONPreserveNull) {
    std::string input = R"({key: null})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("null"), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONArray) {
    std::string input = R"({key: [1, 2, 3]})";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("[1, 2, 3]"), std::string::npos);
}

TEST_F(JSON5Test, ConvertJSON5toJSONUnterminatedString) {
    std::string input = R"({key: "unterminated)";
    auto result = convertJSON5toJSON(input);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// JSON5ParseError Tests
// ============================================================================

TEST_F(JSON5Test, JSON5ParseErrorConstruction) {
    JSON5ParseError error("Test error", 42);
    EXPECT_EQ(error.message, "Test error");
    EXPECT_EQ(error.position, 42);
}

TEST_F(JSON5Test, JSON5ParseErrorWhat) {
    JSON5ParseError error("Test error", 42);
    std::string what = error.what();
    EXPECT_NE(what.find("Test error"), std::string::npos);
    EXPECT_NE(what.find("42"), std::string::npos);
}

TEST_F(JSON5Test, JSON5ParseErrorWhatNoPosition) {
    JSON5ParseError error("Test error", 0);
    std::string what = error.what();
    EXPECT_EQ(what, "Test error");
}

// ============================================================================
// Expected Type Tests
// ============================================================================

TEST_F(JSON5Test, ExpectedHasValue) {
    expected<std::string, JSON5ParseError> result(std::string("success"));
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "success");
}

TEST_F(JSON5Test, ExpectedHasError) {
    expected<std::string, JSON5ParseError> result(JSON5ParseError("error", 0));
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message, "error");
}

TEST_F(JSON5Test, ExpectedBoolConversion) {
    expected<std::string, JSON5ParseError> success(std::string("success"));
    expected<std::string, JSON5ParseError> failure(JSON5ParseError("error", 0));

    EXPECT_TRUE(static_cast<bool>(success));
    EXPECT_FALSE(static_cast<bool>(failure));
}

// ============================================================================
// StringLike Concept Tests
// ============================================================================

TEST_F(JSON5Test, StringLikeWithStdString) {
    std::string input = R"({"key": "value"})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
}

TEST_F(JSON5Test, StringLikeWithStringView) {
    std::string_view input = R"({"key": "value"})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
}

TEST_F(JSON5Test, StringLikeWithCharPtr) {
    const char* input = R"({"key": "value"})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(JSON5Test, EdgeCaseOnlyComments) {
    std::string input = "// just a comment";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().find("//") == std::string::npos);
}

TEST_F(JSON5Test, EdgeCaseNestedComments) {
    std::string input = R"({
        /* outer /* nested */ comment */
        "key": "value"
    })";
    // Note: JSON5 doesn't support nested comments, so this tests behavior
    auto result = removeComments(input);
    // Result depends on implementation
}

TEST_F(JSON5Test, EdgeCaseCommentAtEnd) {
    std::string input = R"({"key": "value"} // trailing comment)";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().find("//"), std::string::npos);
}

TEST_F(JSON5Test, EdgeCaseMultipleSlashes) {
    std::string input = R"({"url": "http://example.com"})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("http://"), std::string::npos);
}

TEST_F(JSON5Test, EdgeCaseEscapedBackslash) {
    std::string input = R"({"path": "C:\\Users\\test"})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("C:\\\\Users"), std::string::npos);
}

TEST_F(JSON5Test, EdgeCaseUnicodeInString) {
    std::string input = R"({"emoji": "😀"})";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
}

TEST_F(JSON5Test, EdgeCaseWhitespaceOnly) {
    std::string input = "   \n\t  ";
    auto result = removeComments(input);
    ASSERT_TRUE(result.has_value());
}

TEST_F(JSON5Test, EdgeCaseComplexJSON5) {
    std::string input = R"({
        // Configuration file
        server: {
            host: "localhost", // Server host
            port: 8080,        /* Server port */
            ssl: true
        },
        /* Database settings */
        database: {
            connection_string: "mongodb://localhost:27017"
        }
    })";
    auto result = convertJSON5toJSON(input);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().find("\"server\""), std::string::npos);
    EXPECT_NE(result.value().find("\"database\""), std::string::npos);
    EXPECT_EQ(result.value().find("//"), std::string::npos);
    EXPECT_EQ(result.value().find("/*"), std::string::npos);
}

}  // namespace lithium::config::test
