/*
 * config_serializer.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: Configuration Serializer Component for JSON/JSON5 Handling

**************************************************/

#ifndef LITHIUM_CONFIG_SERIALIZER_HPP
#define LITHIUM_CONFIG_SERIALIZER_HPP

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "atom/type/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace lithium {

/**
 * @brief Serialization format options
 */
enum class SerializationFormat {
    JSON,          ///< Standard JSON format
    JSON5,         ///< JSON5 format (with comments and relaxed syntax)
    PRETTY_JSON,   ///< Pretty-printed JSON
    COMPACT_JSON,  ///< Compact JSON (minimal whitespace)
    BINARY_JSON    ///< Binary JSON format (CBOR/MessagePack)
};

/**
 * @brief Serialization options and configuration
 */
struct SerializationOptions {
    SerializationFormat format = SerializationFormat::PRETTY_JSON;
    int indentSize = 4;              ///< Indentation size for pretty printing
    bool sortKeys = false;           ///< Sort object keys alphabetically
    bool preserveComments = true;    ///< Preserve comments when possible
    bool validateOutput = true;      ///< Validate serialized output
    bool compressOutput = false;     ///< Apply compression to output
    std::string encoding = "utf-8";  ///< Text encoding

    /**
     * @brief Create options for compact JSON
     * @return Serialization options configured for compact output
     */
    static SerializationOptions compact() {
        SerializationOptions opts;
        opts.format = SerializationFormat::COMPACT_JSON;
        opts.indentSize = 0;
        return opts;
    }

    /**
     * @brief Create options for pretty JSON
     * @return Serialization options configured for pretty output
     */
    static SerializationOptions pretty(int indent = 4) {
        SerializationOptions opts;
        opts.format = SerializationFormat::PRETTY_JSON;
        opts.indentSize = indent;
        return opts;
    }

    /**
     * @brief Create options for JSON5 format
     * @return Serialization options configured for JSON5
     */
    static SerializationOptions json5() {
        SerializationOptions opts;
        opts.format = SerializationFormat::JSON5;
        opts.preserveComments = true;
        return opts;
    }
};

/**
 * @brief Serialization result with metadata
 */
struct SerializationResult {
    bool success = false;                   ///< Whether serialization succeeded
    std::string data;                       ///< Serialized data
    std::string errorMessage;               ///< Error message if failed
    size_t originalSize = 0;                ///< Original data size in bytes
    size_t serializedSize = 0;              ///< Serialized data size in bytes
    std::chrono::milliseconds duration{0};  ///< Serialization duration

    /**
     * @brief Check if serialization was successful
     * @return True if successful
     */
    [[nodiscard]] bool isValid() const noexcept {
        return success && !data.empty();
    }

    /**
     * @brief Get compression ratio
     * @return Compression ratio (0.0 - 1.0), or 1.0 if no compression
     */
    [[nodiscard]] double getCompressionRatio() const noexcept {
        return originalSize > 0
                   ? static_cast<double>(serializedSize) / originalSize
                   : 1.0;
    }
};

/**
 * @brief Deserialization result with metadata
 */
struct DeserializationResult {
    bool success = false;       ///< Whether deserialization succeeded
    json data;                  ///< Deserialized JSON data
    std::string errorMessage;   ///< Error message if failed
    size_t bytesProcessed = 0;  ///< Number of bytes processed
    std::chrono::milliseconds duration{0};  ///< Deserialization duration

    /**
     * @brief Check if deserialization was successful
     * @return True if successful
     */
    [[nodiscard]] bool isValid() const noexcept {
        return success && !data.is_null();
    }
};

/**
 * @brief High-performance configuration serializer with multiple format support
 *
 * This class provides:
 * - JSON and JSON5 serialization/deserialization
 * - Binary format support (CBOR, MessagePack)
 * - Streaming operations for large files
 * - Compression support
 * - Performance metrics and validation
 * - Batch processing capabilities
 */
class ConfigSerializer {
public:
    /**
     * @brief Serializer configuration
     */
    struct Config {
        bool enableMetrics = true;      ///< Enable performance metrics
        bool enableValidation = true;   ///< Enable output validation
        size_t bufferSize = 64 * 1024;  ///< I/O buffer size
        bool useMemoryMapping = true;   ///< Use memory mapping for large files
        size_t maxFileSize =
            100 * 1024 * 1024;  ///< Maximum file size for processing

        Config()
            : enableMetrics(true),
              enableValidation(true),
              bufferSize(64 * 1024),
              useMemoryMapping(true),
              maxFileSize(100 * 1024 * 1024) {}

        Config(bool metrics, bool validation, size_t buffer, bool memoryMap,
               size_t maxSize)
            : enableMetrics(metrics),
              enableValidation(validation),
              bufferSize(buffer),
              useMemoryMapping(memoryMap),
              maxFileSize(maxSize) {}
    };

    /**
     * @brief Constructor
     * @param config Serializer configuration
     */
    explicit ConfigSerializer(const Config& config = Config{});

    /**
     * @brief Destructor
     */
    ~ConfigSerializer();

    ConfigSerializer(const ConfigSerializer&) = delete;
    ConfigSerializer& operator=(const ConfigSerializer&) = delete;
    ConfigSerializer(ConfigSerializer&&) noexcept;
    ConfigSerializer& operator=(ConfigSerializer&&) noexcept;

    /**
     * @brief Serialize JSON data to string
     * @param data JSON data to serialize
     * @param options Serialization options
     * @return Serialization result with data and metadata
     */
    [[nodiscard]] SerializationResult serialize(
        const json& data, const SerializationOptions& options = {}) const;

    /**
     * @brief Deserialize string to JSON data
     * @param input Input string to deserialize
     * @param format Expected input format
     * @return Deserialization result with data and metadata
     */
    [[nodiscard]] DeserializationResult deserialize(
        std::string_view input,
        SerializationFormat format = SerializationFormat::JSON) const;

    /**
     * @brief Serialize JSON data to file
     * @param data JSON data to serialize
     * @param filePath Output file path
     * @param options Serialization options
     * @return True if successful
     */
    bool serializeToFile(const json& data, const fs::path& filePath,
                         const SerializationOptions& options = {}) const;

    /**
     * @brief Deserialize JSON data from file
     * @param filePath Input file path
     * @param format Expected file format (auto-detected if not specified)
     * @return Deserialization result
     */
    [[nodiscard]] DeserializationResult deserializeFromFile(
        const fs::path& filePath,
        std::optional<SerializationFormat> format = std::nullopt) const;

    /**
     * @brief Batch serialize multiple JSON objects
     * @param dataList Vector of JSON objects to serialize
     * @param options Serialization options
     * @return Vector of serialization results
     */
    [[nodiscard]] std::vector<SerializationResult> serializeBatch(
        const std::vector<json>& dataList,
        const SerializationOptions& options = {}) const;

    /**
     * @brief Batch deserialize multiple strings
     * @param inputList Vector of input strings
     * @param format Expected input format
     * @return Vector of deserialization results
     */
    [[nodiscard]] std::vector<DeserializationResult> deserializeBatch(
        const std::vector<std::string>& inputList,
        SerializationFormat format = SerializationFormat::JSON) const;

    /**
     * @brief Stream serialize large JSON data
     * @param data JSON data to serialize
     * @param output Output stream
     * @param options Serialization options
     * @return True if successful
     */
    bool streamSerialize(const json& data, std::ostream& output,
                         const SerializationOptions& options = {}) const;

    /**
     * @brief Stream deserialize large JSON data
     * @param input Input stream
     * @param format Expected input format
     * @return Deserialization result
     */
    [[nodiscard]] DeserializationResult streamDeserialize(
        std::istream& input,
        SerializationFormat format = SerializationFormat::JSON) const;

    /**
     * @brief Auto-detect serialization format from file extension
     * @param filePath File path to analyze
     * @return Detected format or nullopt if unknown
     */
    [[nodiscard]] static std::optional<SerializationFormat> detectFormat(
        const fs::path& filePath);

    /**
     * @brief Auto-detect serialization format from content
     * @param content Content to analyze
     * @return Detected format or nullopt if unknown
     */
    [[nodiscard]] static std::optional<SerializationFormat> detectFormat(
        std::string_view content);

    /**
     * @brief Validate JSON data
     * @param data JSON data to validate
     * @return True if valid
     */
    [[nodiscard]] static bool validateJson(const json& data);

    /**
     * @brief Convert between serialization formats
     * @param input Input data
     * @param fromFormat Source format
     * @param toFormat Target format
     * @param options Serialization options for output
     * @return Conversion result
     */
    [[nodiscard]] SerializationResult convertFormat(
        std::string_view input, SerializationFormat fromFormat,
        SerializationFormat toFormat,
        const SerializationOptions& options = {}) const;

    /**
     * @brief Get supported file extensions for each format
     * @param format Serialization format
     * @return Vector of supported extensions
     */
    [[nodiscard]] static std::vector<std::string> getSupportedExtensions(
        SerializationFormat format);

    /**
     * @brief Get current configuration
     * @return Current serializer configuration
     */
    [[nodiscard]] const Config& getConfig() const;

    /**
     * @brief Update serializer configuration
     * @param newConfig New configuration
     */
    void setConfig(const Config& newConfig);

    /**
     * @brief Clear internal caches and reset state
     */
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium

#endif  // LITHIUM_CONFIG_SERIALIZER_HPP
