/*
 * test_serializer.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_serializer.cpp
 * @brief Comprehensive tests for IPC Serializer
 */

#include <gtest/gtest.h>
#include "script/ipc/serializer.hpp"

#include <string>
#include <vector>

using namespace lithium::ipc;

// =============================================================================
// JSON Serialization Tests
// =============================================================================

class IPCSerializerJsonTest : public ::testing::Test {};

TEST_F(IPCSerializerJsonTest, SerializeEmptyObject) {
    json j = json::object();
    auto result = IPCSerializer::serialize(j);
    EXPECT_GT(result.size(), 0);
}

TEST_F(IPCSerializerJsonTest, SerializeSimpleObject) {
    json j = {{"key", "value"}, {"number", 42}};
    auto result = IPCSerializer::serialize(j);
    EXPECT_GT(result.size(), 0);
}

TEST_F(IPCSerializerJsonTest, SerializeDeserializeRoundTrip) {
    json original = {
        {"string", "hello"},
        {"number", 123},
        {"float", 3.14},
        {"bool", true},
        {"null", nullptr},
        {"array", {1, 2, 3}},
        {"nested", {{"a", 1}, {"b", 2}}}
    };

    auto serialized = IPCSerializer::serialize(original);
    auto result = IPCSerializer::deserialize(serialized);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), original);
}

TEST_F(IPCSerializerJsonTest, DeserializeEmptyData) {
    std::vector<uint8_t> empty;
    auto result = IPCSerializer::deserialize(empty);
    EXPECT_FALSE(result.has_value());
}

TEST_F(IPCSerializerJsonTest, DeserializeInvalidData) {
    std::vector<uint8_t> invalid = {0xFF, 0xFE, 0xFD};
    auto result = IPCSerializer::deserialize(invalid);
    EXPECT_FALSE(result.has_value());
}

TEST_F(IPCSerializerJsonTest, SerializeLargeObject) {
    json large;
    for (int i = 0; i < 1000; ++i) {
        large["key" + std::to_string(i)] = std::string(100, 'x');
    }

    auto serialized = IPCSerializer::serialize(large);
    auto result = IPCSerializer::deserialize(serialized);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1000);
}

TEST_F(IPCSerializerJsonTest, SerializeArray) {
    json arr = json::array({1, 2, 3, 4, 5});
    auto serialized = IPCSerializer::serialize(arr);
    auto result = IPCSerializer::deserialize(serialized);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_array());
    EXPECT_EQ(result->size(), 5);
}

// =============================================================================
// String Serialization Tests
// =============================================================================

class IPCSerializerStringTest : public ::testing::Test {};

TEST_F(IPCSerializerStringTest, SerializeEmptyString) {
    auto result = IPCSerializer::serializeString("");
    EXPECT_GE(result.size(), 4); // At least length prefix
}

TEST_F(IPCSerializerStringTest, SerializeSimpleString) {
    std::string str = "Hello, World!";
    auto result = IPCSerializer::serializeString(str);
    EXPECT_GT(result.size(), str.size());
}

TEST_F(IPCSerializerStringTest, SerializeDeserializeStringRoundTrip) {
    std::string original = "Test string with special chars: \n\t\r";
    auto serialized = IPCSerializer::serializeString(original);

    size_t offset = 0;
    auto result = IPCSerializer::deserializeString(serialized, offset);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), original);
}

TEST_F(IPCSerializerStringTest, DeserializeStringWithOffset) {
    std::string str1 = "First";
    std::string str2 = "Second";

    auto ser1 = IPCSerializer::serializeString(str1);
    auto ser2 = IPCSerializer::serializeString(str2);

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), ser1.begin(), ser1.end());
    combined.insert(combined.end(), ser2.begin(), ser2.end());

    size_t offset = 0;
    auto result1 = IPCSerializer::deserializeString(combined, offset);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), str1);

    auto result2 = IPCSerializer::deserializeString(combined, offset);
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), str2);
}

TEST_F(IPCSerializerStringTest, SerializeLongString) {
    std::string longStr(10000, 'x');
    auto serialized = IPCSerializer::serializeString(longStr);

    size_t offset = 0;
    auto result = IPCSerializer::deserializeString(serialized, offset);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), longStr);
}

TEST_F(IPCSerializerStringTest, SerializeUnicodeString) {
    std::string unicode = u8"Hello 世界 🌍";
    auto serialized = IPCSerializer::serializeString(unicode);

    size_t offset = 0;
    auto result = IPCSerializer::deserializeString(serialized, offset);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), unicode);
}

// =============================================================================
// Bytes Serialization Tests
// =============================================================================

class IPCSerializerBytesTest : public ::testing::Test {};

TEST_F(IPCSerializerBytesTest, SerializeEmptyBytes) {
    std::vector<uint8_t> empty;
    auto result = IPCSerializer::serializeBytes(empty);
    EXPECT_GE(result.size(), 4); // At least header
}

TEST_F(IPCSerializerBytesTest, SerializeBinaryData) {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0xFF, 0xFE};
    auto result = IPCSerializer::serializeBytes(data);
    EXPECT_GT(result.size(), data.size());
}

TEST_F(IPCSerializerBytesTest, SerializeLargeBinaryData) {
    std::vector<uint8_t> large(100000);
    for (size_t i = 0; i < large.size(); ++i) {
        large[i] = static_cast<uint8_t>(i % 256);
    }

    auto result = IPCSerializer::serializeBytes(large);
    EXPECT_GT(result.size(), 0);
}

// =============================================================================
// Checksum Tests
// =============================================================================

class IPCSerializerChecksumTest : public ::testing::Test {};

TEST_F(IPCSerializerChecksumTest, CalculateChecksumEmpty) {
    std::vector<uint8_t> empty;
    auto checksum = IPCSerializer::calculateChecksum(empty);
    // CRC32 of empty data should be a specific value
    EXPECT_NE(checksum, 0); // Or check specific value
}

TEST_F(IPCSerializerChecksumTest, CalculateChecksumDeterministic) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto checksum1 = IPCSerializer::calculateChecksum(data);
    auto checksum2 = IPCSerializer::calculateChecksum(data);
    EXPECT_EQ(checksum1, checksum2);
}

TEST_F(IPCSerializerChecksumTest, DifferentDataDifferentChecksum) {
    std::vector<uint8_t> data1 = {1, 2, 3};
    std::vector<uint8_t> data2 = {1, 2, 4};

    auto checksum1 = IPCSerializer::calculateChecksum(data1);
    auto checksum2 = IPCSerializer::calculateChecksum(data2);

    EXPECT_NE(checksum1, checksum2);
}

TEST_F(IPCSerializerChecksumTest, LargeDataChecksum) {
    std::vector<uint8_t> large(1000000, 0xAB);
    auto checksum = IPCSerializer::calculateChecksum(large);
    EXPECT_NE(checksum, 0);
}

// =============================================================================
// Compression Tests
// =============================================================================

class IPCSerializerCompressionTest : public ::testing::Test {};

TEST_F(IPCSerializerCompressionTest, CompressEmptyData) {
    std::vector<uint8_t> empty;
    auto result = IPCSerializer::compress(empty);
    // Empty compression may succeed or fail depending on implementation
    if (result.has_value()) {
        EXPECT_GE(result->size(), 0);
    }
}

TEST_F(IPCSerializerCompressionTest, CompressDecompressRoundTrip) {
    std::vector<uint8_t> original(1000, 'A'); // Highly compressible

    auto compressed = IPCSerializer::compress(original);
    ASSERT_TRUE(compressed.has_value());

    auto decompressed = IPCSerializer::decompress(compressed.value());
    ASSERT_TRUE(decompressed.has_value());

    EXPECT_EQ(decompressed.value(), original);
}

TEST_F(IPCSerializerCompressionTest, CompressReducesSize) {
    // Create highly compressible data
    std::vector<uint8_t> repetitive(10000, 'X');

    auto compressed = IPCSerializer::compress(repetitive);
    ASSERT_TRUE(compressed.has_value());

    // Compressed should be smaller than original
    EXPECT_LT(compressed->size(), repetitive.size());
}

TEST_F(IPCSerializerCompressionTest, DecompressInvalidData) {
    std::vector<uint8_t> invalid = {0xFF, 0xFE, 0xFD, 0xFC};
    auto result = IPCSerializer::decompress(invalid);
    EXPECT_FALSE(result.has_value());
}

TEST_F(IPCSerializerCompressionTest, CompressRandomData) {
    // Random data is less compressible
    std::vector<uint8_t> random(1000);
    for (size_t i = 0; i < random.size(); ++i) {
        random[i] = static_cast<uint8_t>(rand() % 256);
    }

    auto compressed = IPCSerializer::compress(random);
    ASSERT_TRUE(compressed.has_value());

    auto decompressed = IPCSerializer::decompress(compressed.value());
    ASSERT_TRUE(decompressed.has_value());

    EXPECT_EQ(decompressed.value(), random);
}

TEST_F(IPCSerializerCompressionTest, CompressLargeData) {
    std::vector<uint8_t> large(1000000);
    for (size_t i = 0; i < large.size(); ++i) {
        large[i] = static_cast<uint8_t>(i % 256);
    }

    auto compressed = IPCSerializer::compress(large);
    ASSERT_TRUE(compressed.has_value());

    auto decompressed = IPCSerializer::decompress(compressed.value());
    ASSERT_TRUE(decompressed.has_value());

    EXPECT_EQ(decompressed.value(), large);
}
