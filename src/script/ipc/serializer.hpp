/*
 * Copyright (c) 2023-2024, Max Qian <lightapt.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LITHIUM_SCRIPT_IPC_SERIALIZER_HPP
#define LITHIUM_SCRIPT_IPC_SERIALIZER_HPP

#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "message_types.hpp"

namespace lithium::ipc {

using json = nlohmann::json;

/**
 * @brief Serializer for IPC messages
 *
 * Uses a msgpack-compatible binary format for efficiency
 */
class IPCSerializer {
public:
    /**
     * @brief Serialize JSON to binary format
     */
    [[nodiscard]] static std::vector<uint8_t> serialize(const json& data);

    /**
     * @brief Deserialize binary to JSON
     */
    [[nodiscard]] static IPCResult<json> deserialize(
        std::span<const uint8_t> data);

    /**
     * @brief Serialize a string with length prefix
     */
    [[nodiscard]] static std::vector<uint8_t> serializeString(
        std::string_view str);

    /**
     * @brief Deserialize a length-prefixed string
     */
    [[nodiscard]] static IPCResult<std::string> deserializeString(
        std::span<const uint8_t> data, size_t& offset);

    /**
     * @brief Serialize raw bytes with header
     */
    [[nodiscard]] static std::vector<uint8_t> serializeBytes(
        std::span<const uint8_t> data);

    /**
     * @brief Calculate CRC32 checksum
     */
    [[nodiscard]] static uint32_t calculateChecksum(
        std::span<const uint8_t> data);

    /**
     * @brief Compress data using zlib
     */
    [[nodiscard]] static IPCResult<std::vector<uint8_t>> compress(
        std::span<const uint8_t> data);

    /**
     * @brief Decompress zlib-compressed data
     */
    [[nodiscard]] static IPCResult<std::vector<uint8_t>> decompress(
        std::span<const uint8_t> data);
};

}  // namespace lithium::ipc

#endif  // LITHIUM_SCRIPT_IPC_SERIALIZER_HPP
