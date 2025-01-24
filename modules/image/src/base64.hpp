#ifndef LITHIUM_IMAGE_BASE64_HPP
#define LITHIUM_IMAGE_BASE64_HPP

#include <string>

/**
 * @brief Encode a byte array to a Base64 string.
 *
 * @param bytes_to_encode Pointer to the byte array to encode.
 * @param input_length The length of the byte array.
 * @return std::string The encoded Base64 string.
 */
auto base64Encode(unsigned char const* bytes_to_encode,
                  unsigned int input_length) -> std::string;

/**
 * @brief Decode a Base64 string to its original byte array.
 *
 * @param encoded_string The Base64 encoded string.
 * @return std::string The decoded byte array as a string.
 */
auto base64Decode(std::string const& encoded_string) -> std::string;

#endif  // LITHIUM_IMAGE_BASE64_HPP