/**
 * @file test_serializer.cpp
 * @brief Comprehensive unit tests for ConfigSerializer component
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "config/components/serializer.hpp"
#include "atom/type/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace lithium::config;

namespace lithium::config::test {

class ConfigSerializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_serializer_test";
        fs::create_directories(testDir_);
        serializer_ = std::make_unique<ConfigSerializer>();
    }

    void TearDown() override {
        serializer_.reset();
        fs::remove_all(testDir_);
    }

    fs::path testDir_;
    std::unique_ptr<ConfigSerializer> serializer_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ConfigSerializerTest, DefaultConstruction) {
    ConfigSerializer serializer;
    auto config = serializer.getConfig();
    EXPECT_TRUE(config.enableMetrics);
    EXPECT_TRUE(config.enableValidation);
}

TEST_F(ConfigSerializerTest, ConstructionWithConfig) {
    ConfigSerializer::Config config;
    config.enableMetrics = false;
    config.bufferSize = 32 * 1024;

    ConfigSerializer serializer(config);
    EXPECT_EQ(serializer.getConfig().enableMetrics, false);
    EXPECT_EQ(serializer.getConfig().bufferSize, 32 * 1024);
}

TEST_F(ConfigSerializerTest, MoveConstruction) {
    ConfigSerializer moved(std::move(*serializer_));
    auto config = moved.getConfig();
    EXPECT_TRUE(config.enableMetrics);
}

TEST_F(ConfigSerializerTest, MoveAssignment) {
    ConfigSerializer other;
    other = std::move(*serializer_);
    auto config = other.getConfig();
    EXPECT_TRUE(config.enableMetrics);
}

// ============================================================================
// Serialize Tests
// ============================================================================

TEST_F(ConfigSerializerTest, SerializeSimpleObject) {
    json data = {{"key", "value"}, {"number", 42}};
    auto result = serializer_->serialize(data);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.isValid());
    EXPECT_FALSE(result.data.empty());
}

TEST_F(ConfigSerializerTest, SerializeWithPrettyFormat) {
    json data = {{"key", "value"}};
    auto options = SerializationOptions::pretty(4);
    auto result = serializer_->serialize(data, options);

    EXPECT_TRUE(result.success);
    EXPECT_NE(result.data.find('\n'), std::string::npos);
}

TEST_F(ConfigSerializerTest, SerializeWithCompactFormat) {
    json data = {{"key", "value"}};
    auto options = SerializationOptions::compact();
    auto result = serializer_->serialize(data, options);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data.find('\n'), std::string::npos);
}

TEST_F(ConfigSerializerTest, SerializeArray) {
    json data = {1, 2, 3, 4, 5};
    auto result = serializer_->serialize(data);

    EXPECT_TRUE(result.success);
    EXPECT_NE(result.data.find('['), std::string::npos);
}

TEST_F(ConfigSerializerTest, SerializeNestedObject) {
    json data = {
        {"level1", {
            {"level2", {
                {"level3", "deep_value"}
            }}
        }}
    };
    auto result = serializer_->serialize(data);

    EXPECT_TRUE(result.success);
    EXPECT_NE(result.data.find("deep_value"), std::string::npos);
}

TEST_F(ConfigSerializerTest, SerializeEmptyObject) {
    json data = json::object();
    auto result = serializer_->serialize(data);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data.find("{}"), 0);
}

TEST_F(ConfigSerializerTest, SerializeSortedKeys) {
    json data = {{"zebra", 1}, {"apple", 2}, {"mango", 3}};
    SerializationOptions options;
    options.sortKeys = true;
    auto result = serializer_->serialize(data, options);

    EXPECT_TRUE(result.success);
    size_t applePos = result.data.find("apple");
    size_t mangoPos = result.data.find("mango");
    size_t zebraPos = result.data.find("zebra");
    EXPECT_LT(applePos, mangoPos);
    EXPECT_LT(mangoPos, zebraPos);
}

// ============================================================================
// Deserialize Tests
// ============================================================================

TEST_F(ConfigSerializerTest, DeserializeSimpleObject) {
    std::string input = R"({"key": "value", "number": 42})";
    auto result = serializer_->deserialize(input);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.data["key"].get<std::string>(), "value");
    EXPECT_EQ(result.data["number"].get<int>(), 42);
}

TEST_F(ConfigSerializerTest, DeserializeArray) {
    std::string input = "[1, 2, 3, 4, 5]";
    auto result = serializer_->deserialize(input);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data.size(), 5);
}

TEST_F(ConfigSerializerTest, DeserializeInvalidJson) {
    std::string input = "not valid json {{{";
    auto result = serializer_->deserialize(input);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.isValid());
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ConfigSerializerTest, DeserializeEmptyString) {
    std::string input = "";
    auto result = serializer_->deserialize(input);

    EXPECT_FALSE(result.success);
}

TEST_F(ConfigSerializerTest, DeserializeNestedObject) {
    std::string input = R"({"a": {"b": {"c": "value"}}})";
    auto result = serializer_->deserialize(input);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data["a"]["b"]["c"].get<std::string>(), "value");
}

// ============================================================================
// File Operations Tests
// ============================================================================

TEST_F(ConfigSerializerTest, SerializeToFile) {
    json data = {{"key", "value"}};
    fs::path filePath = testDir_ / "output.json";

    EXPECT_TRUE(serializer_->serializeToFile(data, filePath));
    EXPECT_TRUE(fs::exists(filePath));

    std::ifstream file(filePath);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("key"), std::string::npos);
}

TEST_F(ConfigSerializerTest, DeserializeFromFile) {
    fs::path filePath = testDir_ / "input.json";
    std::ofstream file(filePath);
    file << R"({"key": "value"})";
    file.close();

    auto result = serializer_->deserializeFromFile(filePath);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data["key"].get<std::string>(), "value");
}

TEST_F(ConfigSerializerTest, DeserializeFromNonExistentFile) {
    auto result = serializer_->deserializeFromFile(testDir_ / "nonexistent.json");
    EXPECT_FALSE(result.success);
}

TEST_F(ConfigSerializerTest, SerializeToFileWithOptions) {
    json data = {{"key", "value"}};
    fs::path filePath = testDir_ / "pretty.json";
    auto options = SerializationOptions::pretty(2);

    EXPECT_TRUE(serializer_->serializeToFile(data, filePath, options));

    std::ifstream file(filePath);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_NE(content.find('\n'), std::string::npos);
}

// ============================================================================
// Batch Operations Tests
// ============================================================================

TEST_F(ConfigSerializerTest, SerializeBatch) {
    std::vector<json> dataList = {
        {{"id", 1}},
        {{"id", 2}},
        {{"id", 3}}
    };

    auto results = serializer_->serializeBatch(dataList);
    EXPECT_EQ(results.size(), 3);
    for (const auto& result : results) {
        EXPECT_TRUE(result.success);
    }
}

TEST_F(ConfigSerializerTest, DeserializeBatch) {
    std::vector<std::string> inputList = {
        R"({"id": 1})",
        R"({"id": 2})",
        R"({"id": 3})"
    };

    auto results = serializer_->deserializeBatch(inputList);
    EXPECT_EQ(results.size(), 3);
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_TRUE(results[i].success);
        EXPECT_EQ(results[i].data["id"].get<int>(), static_cast<int>(i + 1));
    }
}

TEST_F(ConfigSerializerTest, DeserializeBatchWithInvalid) {
    std::vector<std::string> inputList = {
        R"({"id": 1})",
        "invalid json",
        R"({"id": 3})"
    };

    auto results = serializer_->deserializeBatch(inputList);
    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].success);
    EXPECT_FALSE(results[1].success);
    EXPECT_TRUE(results[2].success);
}

// ============================================================================
// Stream Operations Tests
// ============================================================================

TEST_F(ConfigSerializerTest, StreamSerialize) {
    json data = {{"key", "value"}};
    std::ostringstream output;

    EXPECT_TRUE(serializer_->streamSerialize(data, output));
    EXPECT_NE(output.str().find("key"), std::string::npos);
}

TEST_F(ConfigSerializerTest, StreamDeserialize) {
    std::istringstream input(R"({"key": "value"})");
    auto result = serializer_->streamDeserialize(input);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data["key"].get<std::string>(), "value");
}

// ============================================================================
// Format Detection Tests
// ============================================================================

TEST_F(ConfigSerializerTest, DetectFormatFromJsonExtension) {
    auto format = ConfigSerializer::detectFormat(fs::path("config.json"));
    ASSERT_TRUE(format.has_value());
    EXPECT_EQ(*format, SerializationFormat::JSON);
}

TEST_F(ConfigSerializerTest, DetectFormatFromJson5Extension) {
    auto format = ConfigSerializer::detectFormat(fs::path("config.json5"));
    ASSERT_TRUE(format.has_value());
    EXPECT_EQ(*format, SerializationFormat::JSON5);
}

TEST_F(ConfigSerializerTest, DetectFormatFromUnknownExtension) {
    auto format = ConfigSerializer::detectFormat(fs::path("config.xyz"));
    EXPECT_FALSE(format.has_value());
}

TEST_F(ConfigSerializerTest, DetectFormatFromContent) {
    std::string jsonContent = R"({"key": "value"})";
    auto format = ConfigSerializer::detectFormat(jsonContent);
    ASSERT_TRUE(format.has_value());
}

// ============================================================================
// Format Conversion Tests
// ============================================================================

TEST_F(ConfigSerializerTest, ConvertPrettyToCompact) {
    std::string prettyJson = R"({
    "key": "value"
})";
    auto result = serializer_->convertFormat(
        prettyJson,
        SerializationFormat::PRETTY_JSON,
        SerializationFormat::COMPACT_JSON
    );

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data.find('\n'), std::string::npos);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_F(ConfigSerializerTest, ValidateValidJson) {
    json data = {{"key", "value"}};
    EXPECT_TRUE(ConfigSerializer::validateJson(data));
}

TEST_F(ConfigSerializerTest, ValidateNullJson) {
    json data = nullptr;
    EXPECT_TRUE(ConfigSerializer::validateJson(data));
}

// ============================================================================
// Supported Extensions Tests
// ============================================================================

TEST_F(ConfigSerializerTest, GetSupportedExtensionsJson) {
    auto extensions = ConfigSerializer::getSupportedExtensions(SerializationFormat::JSON);
    EXPECT_FALSE(extensions.empty());
    EXPECT_TRUE(std::find(extensions.begin(), extensions.end(), ".json") != extensions.end());
}

TEST_F(ConfigSerializerTest, GetSupportedExtensionsJson5) {
    auto extensions = ConfigSerializer::getSupportedExtensions(SerializationFormat::JSON5);
    EXPECT_FALSE(extensions.empty());
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(ConfigSerializerTest, GetConfig) {
    auto config = serializer_->getConfig();
    EXPECT_TRUE(config.enableMetrics);
    EXPECT_TRUE(config.enableValidation);
    EXPECT_EQ(config.bufferSize, 64 * 1024);
}

TEST_F(ConfigSerializerTest, SetConfig) {
    ConfigSerializer::Config newConfig;
    newConfig.enableMetrics = false;
    newConfig.bufferSize = 128 * 1024;

    serializer_->setConfig(newConfig);
    auto config = serializer_->getConfig();
    EXPECT_FALSE(config.enableMetrics);
    EXPECT_EQ(config.bufferSize, 128 * 1024);
}

TEST_F(ConfigSerializerTest, Reset) {
    json data = {{"key", "value"}};
    serializer_->serialize(data);
    serializer_->reset();
    // After reset, serializer should still work
    auto result = serializer_->serialize(data);
    EXPECT_TRUE(result.success);
}

// ============================================================================
// Metrics Tests
// ============================================================================

TEST_F(ConfigSerializerTest, GetMetrics) {
    json data = {{"key", "value"}};
    serializer_->serialize(data);
    serializer_->deserialize(R"({"key": "value"})");

    auto metrics = serializer_->getMetrics();
    EXPECT_GE(metrics.totalSerializations, 1);
    EXPECT_GE(metrics.totalDeserializations, 1);
}

TEST_F(ConfigSerializerTest, ResetMetrics) {
    json data = {{"key", "value"}};
    serializer_->serialize(data);
    serializer_->resetMetrics();

    auto metrics = serializer_->getMetrics();
    EXPECT_EQ(metrics.totalSerializations, 0);
    EXPECT_EQ(metrics.totalDeserializations, 0);
}

TEST_F(ConfigSerializerTest, MetricsErrorCount) {
    serializer_->deserialize("invalid json");
    auto metrics = serializer_->getMetrics();
    EXPECT_GE(metrics.deserializationErrors, 1);
}

// ============================================================================
// Hook Tests
// ============================================================================

TEST_F(ConfigSerializerTest, AddHook) {
    bool hookCalled = false;
    ConfigSerializer::SerializerEvent receivedEvent;

    size_t hookId = serializer_->addHook(
        [&](ConfigSerializer::SerializerEvent event, SerializationFormat format,
            size_t dataSize, bool success) {
            hookCalled = true;
            receivedEvent = event;
        });

    json data = {{"key", "value"}};
    serializer_->serialize(data);

    EXPECT_TRUE(hookCalled);
    EXPECT_TRUE(serializer_->removeHook(hookId));
}

TEST_F(ConfigSerializerTest, RemoveHook) {
    size_t hookId = serializer_->addHook(
        [](ConfigSerializer::SerializerEvent, SerializationFormat, size_t, bool) {});
    EXPECT_TRUE(serializer_->removeHook(hookId));
    EXPECT_FALSE(serializer_->removeHook(hookId));
}

TEST_F(ConfigSerializerTest, ClearHooks) {
    serializer_->addHook(
        [](ConfigSerializer::SerializerEvent, SerializationFormat, size_t, bool) {});
    serializer_->addHook(
        [](ConfigSerializer::SerializerEvent, SerializationFormat, size_t, bool) {});

    serializer_->clearHooks();

    bool hookCalled = false;
    serializer_->addHook(
        [&](ConfigSerializer::SerializerEvent, SerializationFormat, size_t, bool) {
            hookCalled = true;
        });
    serializer_->clearHooks();
    serializer_->serialize(json{{"key", "value"}});
    EXPECT_FALSE(hookCalled);
}

// ============================================================================
// Custom Format Tests
// ============================================================================

TEST_F(ConfigSerializerTest, RegisterCustomFormat) {
    serializer_->registerCustomFormat(
        "custom",
        [](const json& data) { return "CUSTOM:" + data.dump(); },
        [](std::string_view input) {
            std::string str(input);
            if (str.substr(0, 7) == "CUSTOM:") {
                return json::parse(str.substr(7));
            }
            return json{};
        }
    );

    EXPECT_TRUE(serializer_->hasCustomFormat("custom"));
}

TEST_F(ConfigSerializerTest, HasCustomFormat) {
    EXPECT_FALSE(serializer_->hasCustomFormat("nonexistent"));
    serializer_->registerCustomFormat(
        "test",
        [](const json&) { return ""; },
        [](std::string_view) { return json{}; }
    );
    EXPECT_TRUE(serializer_->hasCustomFormat("test"));
}

TEST_F(ConfigSerializerTest, GetCustomFormats) {
    serializer_->registerCustomFormat(
        "format1",
        [](const json&) { return ""; },
        [](std::string_view) { return json{}; }
    );
    serializer_->registerCustomFormat(
        "format2",
        [](const json&) { return ""; },
        [](std::string_view) { return json{}; }
    );

    auto formats = serializer_->getCustomFormats();
    EXPECT_EQ(formats.size(), 2);
}

// ============================================================================
// SerializationOptions Tests
// ============================================================================

TEST_F(ConfigSerializerTest, SerializationOptionsCompact) {
    auto options = SerializationOptions::compact();
    EXPECT_EQ(options.format, SerializationFormat::COMPACT_JSON);
    EXPECT_EQ(options.indentSize, 0);
}

TEST_F(ConfigSerializerTest, SerializationOptionsPretty) {
    auto options = SerializationOptions::pretty(2);
    EXPECT_EQ(options.format, SerializationFormat::PRETTY_JSON);
    EXPECT_EQ(options.indentSize, 2);
}

TEST_F(ConfigSerializerTest, SerializationOptionsJson5) {
    auto options = SerializationOptions::json5();
    EXPECT_EQ(options.format, SerializationFormat::JSON5);
    EXPECT_TRUE(options.preserveComments);
}

// ============================================================================
// SerializationResult Tests
// ============================================================================

TEST_F(ConfigSerializerTest, SerializationResultIsValid) {
    SerializationResult result;
    result.success = true;
    result.data = "test";
    EXPECT_TRUE(result.isValid());

    result.data = "";
    EXPECT_FALSE(result.isValid());
}

TEST_F(ConfigSerializerTest, SerializationResultCompressionRatio) {
    SerializationResult result;
    result.originalSize = 100;
    result.serializedSize = 50;
    EXPECT_DOUBLE_EQ(result.getCompressionRatio(), 0.5);
}

// ============================================================================
// DeserializationResult Tests
// ============================================================================

TEST_F(ConfigSerializerTest, DeserializationResultIsValid) {
    DeserializationResult result;
    result.success = true;
    result.data = {{"key", "value"}};
    EXPECT_TRUE(result.isValid());

    result.data = nullptr;
    EXPECT_FALSE(result.isValid());
}

// ============================================================================
// Backward Compatibility Tests
// ============================================================================

TEST_F(ConfigSerializerTest, BackwardCompatibilityAliases) {
    lithium::ConfigSerializer serializer;
    lithium::SerializationOptions options;
    lithium::SerializationResult result;
    lithium::DeserializationResult deserResult;

    EXPECT_EQ(options.format, lithium::SerializationFormat::PRETTY_JSON);
}

}  // namespace lithium::config::test
