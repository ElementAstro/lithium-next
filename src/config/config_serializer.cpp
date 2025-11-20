/*
 * config_serializer.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: Configuration Serializer Implementation

**************************************************/

#include "config_serializer.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <unordered_map>
#include "json5.hpp"


namespace lithium {

struct ConfigSerializer::Impl {
    Config config_;
    std::shared_ptr<spdlog::logger> logger_;

    explicit Impl(const Config& config) : config_(config) {
        logger_ = spdlog::get("config_serializer");
        if (!logger_) {
            logger_ = spdlog::default_logger();
        }

        logger_->info("ConfigSerializer initialized with buffer size: {} KB",
                      config_.bufferSize / 1024);
    }

    SerializationResult serializeToString(
        const json& data, const SerializationOptions& options) const {
        auto start = std::chrono::steady_clock::now();
        SerializationResult result;

        try {
            std::string originalDump;
            if (config_.enableMetrics) {
                originalDump = data.dump(-1);
                result.originalSize = originalDump.size();
            }

            switch (options.format) {
                case SerializationFormat::JSON:
                case SerializationFormat::PRETTY_JSON:
                    if (options.sortKeys) {
                        json sortedData = sortJsonKeys(data);
                        result.data = sortedData.dump(options.indentSize);
                    } else {
                        result.data = data.dump(options.indentSize);
                    }
                    break;

                case SerializationFormat::COMPACT_JSON:
                    if (options.sortKeys) {
                        json sortedData = sortJsonKeys(data);
                        result.data = sortedData.dump(-1);
                    } else {
                        result.data = data.dump(-1);
                    }
                    break;

                case SerializationFormat::JSON5:
                    result.data = serializeToJson5(data, options);
                    break;

                case SerializationFormat::BINARY_JSON:
                    result.data = serializeToBinary(data);
                    break;

                default:
                    result.errorMessage = "Unsupported serialization format";
                    return result;
            }

            result.serializedSize = result.data.size();
            result.success = true;

            if (config_.enableValidation && options.validateOutput) {
                if (!validateSerialization(result.data, options.format)) {
                    result.success = false;
                    result.errorMessage = "Output validation failed";
                }
            }

        } catch (const std::exception& e) {
            result.errorMessage = e.what();
            logger_->error("Serialization error: {}", e.what());
        }

        auto end = std::chrono::steady_clock::now();
        result.duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (config_.enableMetrics) {
            logger_->debug(
                "Serialization completed: {} bytes in {}ms, ratio: {:.2f}",
                result.serializedSize, result.duration.count(),
                result.getCompressionRatio());
        }

        return result;
    }

    DeserializationResult deserializeFromString(
        std::string_view input, SerializationFormat format) const {
        auto start = std::chrono::steady_clock::now();
        DeserializationResult result;
        result.bytesProcessed = input.size();

        try {
            switch (format) {
                case SerializationFormat::JSON:
                case SerializationFormat::PRETTY_JSON:
                case SerializationFormat::COMPACT_JSON:
                    result.data = json::parse(input);
                    break;

                case SerializationFormat::JSON5: {
                    auto json5Result =
                        internal::convertJSON5toJSON(std::string(input));
                    if (!json5Result) {
                        result.errorMessage = json5Result.error().what();
                        return result;
                    }
                    result.data = json::parse(json5Result.value());
                    break;
                }

                case SerializationFormat::BINARY_JSON:
                    result.data = deserializeFromBinary(input);
                    break;

                default:
                    result.errorMessage = "Unsupported deserialization format";
                    return result;
            }

            result.success = true;

        } catch (const std::exception& e) {
            result.errorMessage = e.what();
            logger_->error("Deserialization error: {}", e.what());
        }

        auto end = std::chrono::steady_clock::now();
        result.duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (config_.enableMetrics) {
            logger_->debug("Deserialization completed: {} bytes in {}ms",
                           result.bytesProcessed, result.duration.count());
        }

        return result;
    }

    std::string serializeToJson5(const json& data,
                                 const SerializationOptions& options) const {
        std::string jsonStr = data.dump(options.indentSize);

        if (options.preserveComments) {
            return "// Generated JSON5 configuration\n" + jsonStr;
        }

        return jsonStr;
    }

    std::string serializeToBinary(const json& data) const {
        return data.dump();
    }

    json deserializeFromBinary(std::string_view input) const {
        return json::parse(input);
    }

    json sortJsonKeys(const json& data) const {
        if (data.is_object()) {
            json sorted = json::object();
            std::vector<std::string> keys;
            keys.reserve(data.size());

            for (auto it = data.begin(); it != data.end(); ++it) {
                keys.push_back(it.key());
            }

            std::sort(keys.begin(), keys.end());

            for (const auto& key : keys) {
                sorted[key] = sortJsonKeys(data[key]);
            }

            return sorted;
        } else if (data.is_array()) {
            json sorted = json::array();
            for (const auto& item : data) {
                sorted.push_back(sortJsonKeys(item));
            }
            return sorted;
        }

        return data;
    }

    bool validateSerialization(const std::string& output,
                               SerializationFormat format) const {
        try {
            switch (format) {
                case SerializationFormat::JSON:
                case SerializationFormat::PRETTY_JSON:
                case SerializationFormat::COMPACT_JSON: {
                    auto parsed = json::parse(output);
                    (void)parsed;  // Suppress unused variable warning
                    return true;
                }

                case SerializationFormat::JSON5: {
                    auto result = internal::convertJSON5toJSON(output);
                    if (!result)
                        return false;
                    auto parsed = json::parse(result.value());
                    (void)parsed;  // Suppress unused variable warning
                    return true;
                }

                case SerializationFormat::BINARY_JSON:
                    return !output.empty();

                default:
                    return false;
            }
        } catch (const std::exception&) {
            return false;
        }
    }
};

ConfigSerializer::ConfigSerializer(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {}

ConfigSerializer::~ConfigSerializer() = default;

ConfigSerializer::ConfigSerializer(ConfigSerializer&&) noexcept = default;
ConfigSerializer& ConfigSerializer::operator=(ConfigSerializer&&) noexcept =
    default;

SerializationResult ConfigSerializer::serialize(
    const json& data, const SerializationOptions& options) const {
    return impl_->serializeToString(data, options);
}

DeserializationResult ConfigSerializer::deserialize(
    std::string_view input, SerializationFormat format) const {
    return impl_->deserializeFromString(input, format);
}

bool ConfigSerializer::serializeToFile(
    const json& data, const fs::path& filePath,
    const SerializationOptions& options) const {
    try {
        auto result = serialize(data, options);
        if (!result.isValid()) {
            impl_->logger_->error("Failed to serialize data for file: {}",
                                  filePath.string());
            return false;
        }

        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            impl_->logger_->error("Failed to open file for writing: {}",
                                  filePath.string());
            return false;
        }

        file.write(result.data.c_str(), result.data.size());

        if (!file.good()) {
            impl_->logger_->error("Failed to write data to file: {}",
                                  filePath.string());
            return false;
        }

        impl_->logger_->info("Successfully serialized {} bytes to file: {}",
                             result.data.size(), filePath.string());
        return true;

    } catch (const std::exception& e) {
        impl_->logger_->error("Error writing to file {}: {}", filePath.string(),
                              e.what());
        return false;
    }
}

DeserializationResult ConfigSerializer::deserializeFromFile(
    const fs::path& filePath, std::optional<SerializationFormat> format) const {
    DeserializationResult result;

    try {
        if (!fs::exists(filePath)) {
            result.errorMessage = "File does not exist: " + filePath.string();
            return result;
        }

        auto fileSize = fs::file_size(filePath);
        if (fileSize > impl_->config_.maxFileSize) {
            result.errorMessage = "File too large: " + filePath.string();
            return result;
        }

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            result.errorMessage = "Failed to open file: " + filePath.string();
            return result;
        }

        std::string content;
        content.reserve(fileSize);
        content.assign((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

        SerializationFormat detectedFormat = format.value_or(
            detectFormat(filePath).value_or(SerializationFormat::JSON));

        result = deserialize(content, detectedFormat);

        if (result.isValid()) {
            impl_->logger_->info(
                "Successfully deserialized {} bytes from file: {}",
                content.size(), filePath.string());
        }

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
        impl_->logger_->error("Error reading from file {}: {}",
                              filePath.string(), e.what());
    }

    return result;
}

std::vector<SerializationResult> ConfigSerializer::serializeBatch(
    const std::vector<json>& dataList,
    const SerializationOptions& options) const {
    std::vector<SerializationResult> results;
    results.reserve(dataList.size());

    constexpr size_t PARALLEL_THRESHOLD = 4;

    if (dataList.size() > PARALLEL_THRESHOLD) {
        std::vector<std::future<SerializationResult>> futures;
        futures.reserve(dataList.size());

        for (const auto& data : dataList) {
            futures.push_back(
                std::async(std::launch::async, [this, &data, &options]() {
                    return serialize(data, options);
                }));
        }

        for (auto& future : futures) {
            results.push_back(future.get());
        }
    } else {
        for (const auto& data : dataList) {
            results.push_back(serialize(data, options));
        }
    }

    return results;
}

std::vector<DeserializationResult> ConfigSerializer::deserializeBatch(
    const std::vector<std::string>& inputList,
    SerializationFormat format) const {
    std::vector<DeserializationResult> results;
    results.reserve(inputList.size());

    constexpr size_t PARALLEL_THRESHOLD = 4;

    if (inputList.size() > PARALLEL_THRESHOLD) {
        std::vector<std::future<DeserializationResult>> futures;
        futures.reserve(inputList.size());

        for (const auto& input : inputList) {
            futures.push_back(
                std::async(std::launch::async, [this, &input, format]() {
                    return deserialize(input, format);
                }));
        }

        for (auto& future : futures) {
            results.push_back(future.get());
        }
    } else {
        for (const auto& input : inputList) {
            results.push_back(deserialize(input, format));
        }
    }

    return results;
}

bool ConfigSerializer::streamSerialize(
    const json& data, std::ostream& output,
    const SerializationOptions& options) const {
    try {
        auto result = serialize(data, options);
        if (!result.isValid()) {
            return false;
        }

        output.write(result.data.c_str(), result.data.size());
        return output.good();

    } catch (const std::exception& e) {
        impl_->logger_->error("Stream serialization error: {}", e.what());
        return false;
    }
}

DeserializationResult ConfigSerializer::streamDeserialize(
    std::istream& input, SerializationFormat format) const {
    try {
        std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
        return deserialize(content, format);

    } catch (const std::exception& e) {
        DeserializationResult result;
        result.errorMessage = e.what();
        impl_->logger_->error("Stream deserialization error: {}", e.what());
        return result;
    }
}

std::optional<SerializationFormat> ConfigSerializer::detectFormat(
    const fs::path& filePath) {
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::unordered_map<std::string, SerializationFormat>
        formatMap = {{".json", SerializationFormat::JSON},
                     {".json5", SerializationFormat::JSON5},
                     {".lithium", SerializationFormat::JSON},
                     {".config", SerializationFormat::JSON},
                     {".lithium5", SerializationFormat::JSON5},
                     {".cbor", SerializationFormat::BINARY_JSON},
                     {".msgpack", SerializationFormat::BINARY_JSON}};

    auto it = formatMap.find(ext);
    return (it != formatMap.end())
               ? std::optional<SerializationFormat>(it->second)
               : std::nullopt;
}

std::optional<SerializationFormat> ConfigSerializer::detectFormat(
    std::string_view content) {
    if (content.empty()) {
        return std::nullopt;
    }

    size_t start = 0;
    while (start < content.size() && std::isspace(content[start])) {
        ++start;
    }

    if (start >= content.size()) {
        return std::nullopt;
    }

    if (content.substr(start, 2) == "//") {
        return SerializationFormat::JSON5;
    }

    char first = content[start];
    if (first == '{' || first == '[') {
        return SerializationFormat::JSON;
    }

    constexpr size_t SAMPLE_SIZE = 100;
    for (size_t i = start; i < std::min(start + SAMPLE_SIZE, content.size());
         ++i) {
        if (!std::isprint(content[i]) && !std::isspace(content[i])) {
            return SerializationFormat::BINARY_JSON;
        }
    }

    return SerializationFormat::JSON;
}

bool ConfigSerializer::validateJson(const json& data) {
    try {
        std::string dumped = data.dump();
        json parsed = json::parse(dumped);
        return parsed == data;
    } catch (const std::exception&) {
        return false;
    }
}

SerializationResult ConfigSerializer::convertFormat(
    std::string_view input, SerializationFormat fromFormat,
    SerializationFormat toFormat, const SerializationOptions& options) const {
    auto deserResult = deserialize(input, fromFormat);
    if (!deserResult.isValid()) {
        SerializationResult result;
        result.errorMessage =
            "Failed to deserialize input: " + deserResult.errorMessage;
        return result;
    }

    return serialize(deserResult.data, options);
}

std::vector<std::string> ConfigSerializer::getSupportedExtensions(
    SerializationFormat format) {
    switch (format) {
        case SerializationFormat::JSON:
        case SerializationFormat::PRETTY_JSON:
        case SerializationFormat::COMPACT_JSON:
            return {".json", ".lithium"};

        case SerializationFormat::JSON5:
            return {".json5", ".lithium5"};

        case SerializationFormat::BINARY_JSON:
            return {".cbor", ".msgpack", ".bin"};

        default:
            return {};
    }
}

const ConfigSerializer::Config& ConfigSerializer::getConfig() const {
    return impl_->config_;
}

void ConfigSerializer::setConfig(const Config& newConfig) {
    impl_->config_ = newConfig;
    impl_->logger_->info("Serializer configuration updated");
}

void ConfigSerializer::reset() {
    impl_->logger_->debug("Serializer state reset");
}

}  // namespace lithium
