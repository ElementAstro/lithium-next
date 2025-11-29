#include <gtest/gtest.h>
#include "base64.hpp"

class Base64Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Standard test vectors from RFC 4648
        testVectors = {{"", ""},
                       {"f", "Zg=="},
                       {"fo", "Zm8="},
                       {"foo", "Zm9v"},
                       {"foob", "Zm9vYg=="},
                       {"fooba", "Zm9vYmE="},
                       {"foobar", "Zm9vYmFy"}};

        // Binary data for testing
        binaryData = {0x00, 0xFF, 0x80, 0x01};
        binaryEncoded = "AP+AAQ==";
    }

    std::vector<std::pair<std::string, std::string>> testVectors;
    std::vector<unsigned char> binaryData;
    std::string binaryEncoded;
};

// Basic Encoding Tests
TEST_F(Base64Test, EncodeEmptyString) {
    EXPECT_EQ(base64Encode(nullptr, 0), "");
}

TEST_F(Base64Test, EncodeSingleChar) {
    const unsigned char input[] = "f";
    EXPECT_EQ(base64Encode(input, 1), "Zg==");
}

TEST_F(Base64Test, EncodeStandardString) {
    const unsigned char input[] = "Hello, World!";
    EXPECT_EQ(base64Encode(input, 13), "SGVsbG8sIFdvcmxkIQ==");
}

// RFC 4648 Test Vectors
TEST_F(Base64Test, EncodeRFCTestVectors) {
    for (const auto& [input, expected] : testVectors) {
        EXPECT_EQ(
            base64Encode(reinterpret_cast<const unsigned char*>(input.c_str()),
                         input.length()),
            expected);
    }
}

// Binary Data Tests
TEST_F(Base64Test, EncodeBinaryData) {
    EXPECT_EQ(base64Encode(binaryData.data(), binaryData.size()),
              binaryEncoded);
}

// Basic Decoding Tests
TEST_F(Base64Test, DecodeEmptyString) { EXPECT_EQ(base64Decode(""), ""); }

TEST_F(Base64Test, DecodeSingleChar) { EXPECT_EQ(base64Decode("Zg=="), "f"); }

TEST_F(Base64Test, DecodeStandardString) {
    EXPECT_EQ(base64Decode("SGVsbG8sIFdvcmxkIQ=="), "Hello, World!");
}

// RFC 4648 Test Vectors
TEST_F(Base64Test, DecodeRFCTestVectors) {
    for (const auto& [expected, input] : testVectors) {
        EXPECT_EQ(base64Decode(input), expected);
    }
}

// Padding Tests
TEST_F(Base64Test, HandlePadding) {
    EXPECT_EQ(base64Decode("YQ=="), "a");    // 1 byte, 2 padding chars
    EXPECT_EQ(base64Decode("YWI="), "ab");   // 2 bytes, 1 padding char
    EXPECT_EQ(base64Decode("YWJj"), "abc");  // 3 bytes, no padding
}

// Special Characters Tests
TEST_F(Base64Test, HandleSpecialCharacters) {
    const unsigned char special[] = "\0\n\r\t";
    std::string encoded = base64Encode(special, 4);
    EXPECT_EQ(base64Decode(encoded), std::string("\0\n\r\t", 4));
}

// Roundtrip Tests
TEST_F(Base64Test, RoundtripStrings) {
    for (const auto& [original, _] : testVectors) {
        std::string encoded = base64Encode(
            reinterpret_cast<const unsigned char*>(original.c_str()),
            original.length());
        std::string decoded = base64Decode(encoded);
        EXPECT_EQ(decoded, original);
    }
}

TEST_F(Base64Test, RoundtripBinaryData) {
    std::string encoded = base64Encode(binaryData.data(), binaryData.size());
    std::string decoded = base64Decode(encoded);
    EXPECT_EQ(std::vector<unsigned char>(decoded.begin(), decoded.end()),
              binaryData);
}

// Error Cases
TEST_F(Base64Test, HandleInvalidCharacters) {
    EXPECT_EQ(base64Decode("!@#$"), "");
    EXPECT_EQ(base64Decode("SGVsbG8_"), "Hello?");
}

TEST_F(Base64Test, HandleMalformedInput) {
    EXPECT_EQ(base64Decode("Zg="), "f");    // Missing padding
    EXPECT_EQ(base64Decode("Zg==="), "f");  // Extra padding
    EXPECT_EQ(base64Decode("Z"), "");       // Incomplete input
}

// Edge Cases
TEST_F(Base64Test, HandleLongStrings) {
    std::string longInput(1000, 'X');
    std::string encoded =
        base64Encode(reinterpret_cast<const unsigned char*>(longInput.c_str()),
                     longInput.length());
    std::string decoded = base64Decode(encoded);
    EXPECT_EQ(decoded, longInput);
}

TEST_F(Base64Test, HandleWhitespace) {
    EXPECT_EQ(base64Decode("SG Vs\nbG8="), "Hello");
}
