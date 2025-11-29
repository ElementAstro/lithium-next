/*
 * serializer.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "serializer.hpp"

#include <spdlog/spdlog.h>

namespace lithium::ipc {

std::vector<uint8_t> IPCSerializer::serialize(const json& data) {
    // Use JSON as the serialization format
    std::string jsonStr = data.dump();
    return std::vector<uint8_t>(jsonStr.begin(), jsonStr.end());
}

IPCResult<json> IPCSerializer::deserialize(std::span<const uint8_t> data) {
    try {
        std::string jsonStr(data.begin(), data.end());
        return json::parse(jsonStr);
    } catch (const std::exception& e) {
        spdlog::error("JSON parse error: {}", e.what());
        return std::unexpected(IPCError::DeserializationFailed);
    }
}

std::vector<uint8_t> IPCSerializer::serializeString(std::string_view str) {
    std::vector<uint8_t> result;
    uint32_t len = static_cast<uint32_t>(str.size());

    // Length prefix (4 bytes, big-endian)
    result.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(len & 0xFF));

    // String data
    result.insert(result.end(), str.begin(), str.end());

    return result;
}

IPCResult<std::string> IPCSerializer::deserializeString(
    std::span<const uint8_t> data, size_t& offset) {

    if (offset + 4 > data.size()) {
        return std::unexpected(IPCError::InvalidMessage);
    }

    uint32_t len = (static_cast<uint32_t>(data[offset]) << 24) |
                   (static_cast<uint32_t>(data[offset + 1]) << 16) |
                   (static_cast<uint32_t>(data[offset + 2]) << 8) |
                   static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    if (offset + len > data.size()) {
        return std::unexpected(IPCError::InvalidMessage);
    }

    std::string result(data.begin() + offset, data.begin() + offset + len);
    offset += len;

    return result;
}

std::vector<uint8_t> IPCSerializer::serializeBytes(std::span<const uint8_t> data) {
    std::vector<uint8_t> result;
    uint32_t len = static_cast<uint32_t>(data.size());

    // Length prefix
    result.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(len & 0xFF));

    // Data
    result.insert(result.end(), data.begin(), data.end());

    return result;
}

uint32_t IPCSerializer::calculateChecksum(std::span<const uint8_t> data) {
    // CRC32 implementation with full table
    static const uint32_t crcTable[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
        0xF3B6E20C, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83F3D0C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
        0xFA44E5D6, 0x8D079F40, 0x15F5A3F4, 0x62E8EB62, 0xFBEFF47D, 0x8C8315EB,
        0x12C0A48C, 0x6162341A, 0xF88D5D60, 0x8FD40FF6, 0x21DA880B, 0x5605E89D,
        0xCF95D367, 0xB8843371, 0x236B22D2, 0x54D0B244, 0xCDC7075E, 0xBA0B88C8,
        0x2A3AF9D7, 0x5D1FB041, 0xC41B6ADB, 0xB3B6944D, 0x2B03B6EE, 0x5CB1D678,
        0xC5D0C662, 0xB2D04CF4, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
        0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
        0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
        0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856534D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
        0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
        0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
        0xBE0B1850, 0xC90C2086, 0x5A6AFF25, 0x2D6E22B3, 0xB56FE8A9, 0xC266A03F,
        0x5A05DF1B, 0x2D02EF8D, 0xB3B6E20C, 0xC4B5A09A, 0x5B04B331, 0x2C03E6A7,
        0xB40BB85D, 0xC30C8DCB, 0x64D10002, 0x130B0094, 0x8A7A006E, 0xFD30F0F8,
        0x6A8D705B, 0x1D305CCD, 0x84F50137, 0xF3671FA1, 0x6309C830, 0x140FA8A6,
        0x8D0AB85C, 0xFA0A88CA, 0x6E8E1B69, 0x196C3CFF, 0x8059C305, 0xF70F0393,
        0x0EEF63C3, 0x79505955, 0xE0D5A2AF, 0x97D2D72D, 0x09B64C8E, 0x7EB17C18,
        0xE7B82D02, 0x90BF1D94, 0x1DB71064, 0x6AB020F2, 0xF3B6E20C, 0x84BE41DE,
        0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83F3D0C7, 0x136C9856, 0x646BA8C0,
        0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA44E5D6, 0x8D079F40,
        0x15F5A3F4, 0x62E8EB62, 0xFBEFF47D, 0x8C8315EB, 0x12C0A48C, 0x6162341A,
        0xF88D5D60, 0x8FD40FF6, 0x21DA880B, 0x5605E89D, 0xCF95D367, 0xB8843371,
        0x236B22D2, 0x54D0B244, 0xCDC7075E, 0xBA0B88C8, 0x2A3AF9D7, 0x5D1FB041,
        0xC41B6ADB, 0xB3B6944D, 0x2B03B6EE, 0x5CB1D678, 0xC5D0C662, 0xB2D04CF4,
        0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447,
        0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
        0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A,
        0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11,
        0xC1611DAB, 0xB6662D3D
    };

    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc = (crc >> 8) ^ crcTable[(crc ^ byte) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

IPCResult<std::vector<uint8_t>> IPCSerializer::compress(
    std::span<const uint8_t> data) {
    // TODO: Implement zlib compression when zlib is integrated
    // For now, return stub error
    spdlog::warn("Compression not yet implemented");
    return std::unexpected(IPCError::UnknownError);
}

IPCResult<std::vector<uint8_t>> IPCSerializer::decompress(
    std::span<const uint8_t> data) {
    // TODO: Implement zlib decompression when zlib is integrated
    // For now, return stub error
    spdlog::warn("Decompression not yet implemented");
    return std::unexpected(IPCError::UnknownError);
}

}  // namespace lithium::ipc
