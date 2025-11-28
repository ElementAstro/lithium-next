/*
 * json5.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: JSON5 Parser Utility

**************************************************/

#ifndef LITHIUM_CONFIG_UTILS_JSON5_HPP
#define LITHIUM_CONFIG_UTILS_JSON5_HPP

#include <concepts>
#include <string>
#include <string_view>

namespace lithium::internal {

// Using custom expected implementation for C++20 compatibility
template <typename T, typename E>
class expected {
private:
    union {
        T value_;
        E error_;
    };
    bool has_value_;

public:
    explicit expected(const T& value) : value_(value), has_value_(true) {}
    explicit expected(T&& value) : value_(std::move(value)), has_value_(true) {}
    explicit expected(const E& error) : error_(error), has_value_(false) {}
    explicit expected(E&& error)
        : error_(std::move(error)), has_value_(false) {}
    ~expected() {
        if (has_value_)
            value_.~T();
        else
            error_.~E();
    }

    [[nodiscard]] bool has_value() const noexcept { return has_value_; }
    [[nodiscard]] const T& value() const& { return value_; }
    [[nodiscard]] T& value() & { return value_; }
    [[nodiscard]] const E& error() const& { return error_; }
    [[nodiscard]] E& error() & { return error_; }

    // Conversion operators
    explicit operator bool() const noexcept { return has_value_; }
};

/**
 * @brief JSON5 parse error information
 */
struct JSON5ParseError {
    std::string message;
    size_t position;

    JSON5ParseError(std::string_view msg, size_t pos = 0)
        : message(msg), position(pos) {}

    [[nodiscard]] std::string what() const {
        return message +
               (position > 0 ? " at position " + std::to_string(position) : "");
    }
};

/**
 * @brief Concept for string-like types that can be used for JSON processing
 */
template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

/**
 * @brief Remove comments from JSON5 string
 * @tparam T String-like type
 * @param json5 Input JSON5 string
 * @return Expected result with cleaned string or error
 */
template <StringLike T>
[[nodiscard]] auto removeComments(const T& json5)
    -> expected<std::string, JSON5ParseError> {
    const std::string_view input{json5};
    if (input.empty()) {
        return expected<std::string, JSON5ParseError>{std::string{}};
    }

    std::string result;
    result.reserve(input.size());
    bool inSingleLineComment = false;
    bool inMultiLineComment = false;
    bool inString = false;
    bool escaped = false;

    try {
        for (size_t i = 0; i < input.size(); ++i) {
            // String handling with proper escape character support
            if (!inSingleLineComment && !inMultiLineComment) {
                if (input[i] == '\\' && inString) {
                    escaped = !escaped;
                    result.push_back(input[i]);
                    continue;
                }

                if (input[i] == '"' && !escaped) {
                    inString = !inString;
                    result.push_back(input[i]);
                    continue;
                }
            }

            if (escaped) {
                escaped = false;
                result.push_back(input[i]);
                continue;
            }

            if (inString) {
                result.push_back(input[i]);
                continue;
            }

            // Comment handling
            if (!inMultiLineComment && !inSingleLineComment &&
                i + 1 < input.size()) {
                if (input[i] == '/' && input[i + 1] == '/') {
                    inSingleLineComment = true;
                    ++i;
                    continue;
                }
                if (input[i] == '/' && input[i + 1] == '*') {
                    inMultiLineComment = true;
                    ++i;
                    continue;
                }
            }

            if (inSingleLineComment) {
                if (input[i] == '\n') {
                    inSingleLineComment = false;
                    result.push_back('\n');
                }
                continue;
            }

            if (inMultiLineComment) {
                if (i + 1 < input.size() && input[i] == '*' &&
                    input[i + 1] == '/') {
                    inMultiLineComment = false;
                    ++i;
                }
                continue;
            }

            result.push_back(input[i]);
        }

        if (inString) {
            return expected<std::string, JSON5ParseError>{
                JSON5ParseError{"Unterminated string", input.size()}};
        }
        if (inMultiLineComment) {
            return expected<std::string, JSON5ParseError>{JSON5ParseError{
                "Unterminated multi-line comment", input.size()}};
        }

        return expected<std::string, JSON5ParseError>{std::move(result)};
    } catch (const std::exception& e) {
        return expected<std::string, JSON5ParseError>{
            JSON5ParseError{std::string{"JSON5 parse error: "} + e.what()}};
    }
}

/**
 * @brief Convert JSON5 to standard JSON
 * @tparam T String-like type
 * @param json5 Input JSON5 string
 * @return Expected result with JSON string or error
 */
template <StringLike T>
[[nodiscard]] auto convertJSON5toJSON(const T& json5)
    -> expected<std::string, JSON5ParseError> {
    try {
        auto jsonResult = removeComments(json5);
        if (!jsonResult.has_value()) {
            return expected<std::string, JSON5ParseError>{jsonResult.error()};
        }

        const std::string& json = jsonResult.value();
        if (json.empty()) {
            return expected<std::string, JSON5ParseError>{std::string{}};
        }

        std::string result;
        result.reserve(json.size() * 1.2);  // Pre-allocate space
        bool inString = false;
        bool escaped = false;

        for (size_t i = 0; i < json.size(); ++i) {
            if (escaped) {
                result.push_back(json[i]);
                escaped = false;
                continue;
            }

            if (json[i] == '\\' && inString) {
                result.push_back(json[i]);
                escaped = true;
                continue;
            }

            if (json[i] == '"') {
                inString = !inString;
                result.push_back(json[i]);
                continue;
            }

            if (inString) {
                result.push_back(json[i]);
                continue;
            }

            // Handle unquoted property keys (improved algorithm)
            if (std::isalpha(json[i]) || json[i] == '_') {
                size_t start = i;
                while (i < json.size() && (std::isalnum(json[i]) ||
                                           json[i] == '_' || json[i] == '-')) {
                    ++i;
                }

                // Check if this is really a key (should be followed by colon)
                bool isKey = false;
                for (size_t j = i; j < json.size(); ++j) {
                    if (json[j] == ':') {
                        isKey = true;
                        break;
                    } else if (!std::isspace(json[j])) {
                        break;
                    }
                }

                if (isKey) {
                    result.push_back('"');
                    result.append(json.substr(start, i - start));
                    result.push_back('"');
                } else {
                    result.append(json.substr(start, i - start));
                }
                --i;
                continue;
            }

            result.push_back(json[i]);
        }

        if (inString) {
            return expected<std::string, JSON5ParseError>{
                JSON5ParseError{"Unterminated string in JSON5", json.size()}};
        }

        return expected<std::string, JSON5ParseError>{std::move(result)};

    } catch (const std::exception& e) {
        return expected<std::string, JSON5ParseError>{JSON5ParseError{
            std::string{"JSON5 to JSON conversion error: "} + e.what()}};
    }
}

}  // namespace lithium::internal

#endif  // LITHIUM_CONFIG_UTILS_JSON5_HPP
