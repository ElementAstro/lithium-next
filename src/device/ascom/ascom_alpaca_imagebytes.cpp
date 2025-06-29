/*
 * ascom_alpaca_imagebytes.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-19

Description: ASCOM Alpaca ImageBytes Protocol Implementation (API v9)

**************************************************/

#include <spdlog/spdlog.h>
#include "ascom_alpaca_client.hpp"

// ImageBytes implementation for high-speed image transfer

bool ASCOMAlpacaClient::supportsImageBytes() {
    // Check if the device supports ImageBytes by calling ImageArray with Accept
    // header
    auto originalHeaders = custom_headers_;
    custom_headers_["Accept"] = "application/imagebytes";

    auto response = performRequest(HttpMethod::GET, "imagearray");

    // Restore original headers
    custom_headers_ = originalHeaders;

    // Check response content type
    auto contentType = response.headers.find("Content-Type");
    return contentType != response.headers.end() &&
           contentType->second.find("application/imagebytes") !=
               std::string::npos;
}

std::pair<ImageBytesMetadata, std::vector<uint8_t>>
ASCOMAlpacaClient::getImageBytes() {
    // Set Accept header for ImageBytes format
    addCustomHeader("Accept", "application/imagebytes");

    auto response = performRequest(HttpMethod::GET, "imagearray");

    // Remove the Accept header
    removeCustomHeader("Accept");

    ImageBytesMetadata metadata;
    std::vector<uint8_t> imageData;

    if (!response.success || response.status_code != 200) {
        metadata.error_number = response.status_code;
        metadata.error_message =
            "HTTP request failed: " + response.error_message;
        return {metadata, imageData};
    }

    // Check if response is in ImageBytes format
    auto contentType = response.headers.find("Content-Type");
    if (contentType == response.headers.end() ||
        contentType->second.find("application/imagebytes") ==
            std::string::npos) {
        metadata.error_number = 0x500;
        metadata.error_message = "Server does not support ImageBytes format";
        return {metadata, imageData};
    }

    // Parse ImageBytes binary format
    std::vector<uint8_t> responseData(response.body.begin(),
                                      response.body.end());
    metadata = parseImageBytesMetadata(responseData);

    if (metadata.isSuccess()) {
        imageData = extractImageBytesData(responseData, metadata);
    }

    return {metadata, imageData};
}

ImageBytesMetadata ASCOMAlpacaClient::parseImageBytesMetadata(
    const std::vector<uint8_t>& data) {
    ImageBytesMetadata metadata;

    if (data.size() < 32) {  // Minimum size for metadata
        metadata.error_number = 0x500;
        metadata.error_message = "Invalid ImageBytes data: too small";
        return metadata;
    }

    size_t offset = 0;

    // Read header (4 bytes each for transaction IDs and error info)
    std::memcpy(&metadata.client_transaction_id, data.data() + offset, 4);
    offset += 4;

    std::memcpy(&metadata.server_transaction_id, data.data() + offset, 4);
    offset += 4;

    std::memcpy(&metadata.error_number, data.data() + offset, 4);
    offset += 4;

    // Error message length (4 bytes)
    uint32_t errorMessageLength;
    std::memcpy(&errorMessageLength, data.data() + offset, 4);
    offset += 4;

    // Error message (if any)
    if (errorMessageLength > 0) {
        if (offset + errorMessageLength > data.size()) {
            metadata.error_number = 0x500;
            metadata.error_message =
                "Invalid ImageBytes data: error message overflow";
            return metadata;
        }

        metadata.error_message = std::string(
            data.begin() + offset, data.begin() + offset + errorMessageLength);
        offset += errorMessageLength;
    }

    // If there's an error, return here
    if (metadata.error_number != 0) {
        return metadata;
    }

    // Image metadata
    if (offset + 12 > data.size()) {
        metadata.error_number = 0x500;
        metadata.error_message = "Invalid ImageBytes data: metadata incomplete";
        return metadata;
    }

    std::memcpy(&metadata.image_element_type, data.data() + offset, 4);
    offset += 4;

    std::memcpy(&metadata.transmission_element_type, data.data() + offset, 4);
    offset += 4;

    std::memcpy(&metadata.rank, data.data() + offset, 4);
    offset += 4;

    // Dimension sizes
    metadata.dimension.resize(metadata.rank);
    for (int i = 0; i < metadata.rank; ++i) {
        if (offset + 4 > data.size()) {
            metadata.error_number = 0x500;
            metadata.error_message =
                "Invalid ImageBytes data: dimension overflow";
            return metadata;
        }

        std::memcpy(&metadata.dimension[i], data.data() + offset, 4);
        offset += 4;
    }

    return metadata;
}

std::vector<uint8_t> ASCOMAlpacaClient::extractImageBytesData(
    const std::vector<uint8_t>& data, const ImageBytesMetadata& metadata) {
    if (metadata.error_number != 0) {
        return {};
    }

    // Calculate metadata size
    size_t metadataSize = 16;                       // Fixed header
    metadataSize += metadata.error_message.size();  // Error message
    metadataSize += 12;                             // Image type info
    metadataSize += metadata.rank * 4;              // Dimensions

    if (metadataSize >= data.size()) {
        return {};
    }

    // Extract image data
    std::vector<uint8_t> imageData(data.begin() + metadataSize, data.end());

    // Validate expected size
    size_t expectedSize =
        metadata.getTotalElements() * metadata.getElementSize();
    if (imageData.size() != expectedSize) {
        spdlog::warn("ImageBytes data size mismatch: expected {}, got {}",
                     expectedSize, imageData.size());
    }

    return imageData;
}

// Enhanced image array methods with ImageBytes support
std::optional<std::vector<uint16_t>>
ASCOMAlpacaClient::getImageArrayAsUInt16() {
    // Try ImageBytes first if supported
    if (supportsImageBytes()) {
        auto [metadata, data] = getImageBytes();
        if (metadata.isSuccess() && metadata.transmission_element_type == 2) {
            return AlpacaUtils::convertFromBytes<uint16_t>(data);
        }
    }

    // Fallback to JSON method
    auto jsonArray = getProperty("imagearray");
    if (!jsonArray.has_value()) {
        return std::nullopt;
    }

    return AlpacaUtils::jsonArrayToUInt16(jsonArray.value());
}

std::optional<std::vector<uint32_t>>
ASCOMAlpacaClient::getImageArrayAsUInt32() {
    // Try ImageBytes first if supported
    if (supportsImageBytes()) {
        auto [metadata, data] = getImageBytes();
        if (metadata.isSuccess() && metadata.transmission_element_type == 3) {
            return AlpacaUtils::convertFromBytes<uint32_t>(data);
        }
    }

    // Fallback to JSON method
    auto jsonArray = getProperty("imagearray");
    if (!jsonArray.has_value()) {
        return std::nullopt;
    }

    return AlpacaUtils::jsonArrayToUInt32(jsonArray.value());
}

std::optional<std::vector<double>> ASCOMAlpacaClient::getImageArrayAsDouble() {
    // Try ImageBytes first if supported
    if (supportsImageBytes()) {
        auto [metadata, data] = getImageBytes();
        if (metadata.isSuccess() && metadata.transmission_element_type == 6) {
            return AlpacaUtils::convertFromBytes<double>(data);
        }
    }

    // Fallback to JSON method
    auto jsonArray = getProperty("imagearray");
    if (!jsonArray.has_value()) {
        return std::nullopt;
    }

    return AlpacaUtils::jsonArrayToDouble(jsonArray.value());
}

std::optional<std::vector<uint8_t>> ASCOMAlpacaClient::getImageArray() {
    // Try ImageBytes first if supported
    if (supportsImageBytes()) {
        auto [metadata, data] = getImageBytes();
        if (metadata.isSuccess()) {
            return data;
        }
    }

    // Fallback to JSON method
    auto jsonArray = getProperty("imagearray");
    if (!jsonArray.has_value()) {
        return std::nullopt;
    }

    return AlpacaUtils::jsonArrayToUInt8(jsonArray.value());
}

// Device-specific client implementations

// AlpacaCameraClient
std::pair<ImageBytesMetadata, std::vector<uint16_t>>
AlpacaCameraClient::getImageArrayUInt16() {
    auto [metadata, data] = getImageBytes();
    std::vector<uint16_t> imageArray;

    if (metadata.isSuccess()) {
        imageArray = AlpacaUtils::convertFromBytes<uint16_t>(data);
    }

    return {metadata, imageArray};
}

std::pair<ImageBytesMetadata, std::vector<uint32_t>>
AlpacaCameraClient::getImageArrayUInt32() {
    auto [metadata, data] = getImageBytes();
    std::vector<uint32_t> imageArray;

    if (metadata.isSuccess()) {
        imageArray = AlpacaUtils::convertFromBytes<uint32_t>(data);
    }

    return {metadata, imageArray};
}

// Camera-specific methods
std::optional<double> AlpacaCameraClient::getCCDTemperature() {
    return getTypedProperty<double>("ccdtemperature");
}

bool AlpacaCameraClient::setCCDTemperature(double temperature) {
    return setTypedProperty("ccdtemperature", temperature);
}

std::optional<bool> AlpacaCameraClient::getCoolerOn() {
    return getTypedProperty<bool>("cooleron");
}

bool AlpacaCameraClient::setCoolerOn(bool on) {
    return setTypedProperty("cooleron", on);
}

std::optional<int> AlpacaCameraClient::getBinX() {
    return getTypedProperty<int>("binx");
}

bool AlpacaCameraClient::setBinX(int binning) {
    return setTypedProperty("binx", binning);
}

std::optional<int> AlpacaCameraClient::getBinY() {
    return getTypedProperty<int>("biny");
}

bool AlpacaCameraClient::setBinY(int binning) {
    return setTypedProperty("biny", binning);
}

std::optional<double> AlpacaCameraClient::getExposureTime() {
    return getTypedProperty<double>("lastexposureduration");
}

bool AlpacaCameraClient::startExposure(double duration, bool light) {
    std::unordered_map<std::string, json> params;
    params["Duration"] = duration;
    params["Light"] = light;

    auto result = invokeMethod("startexposure", params);
    return result.has_value();
}

bool AlpacaCameraClient::abortExposure() {
    auto result = invokeMethod("abortexposure");
    return result.has_value();
}

std::optional<bool> AlpacaCameraClient::getImageReady() {
    return getTypedProperty<bool>("imageready");
}
