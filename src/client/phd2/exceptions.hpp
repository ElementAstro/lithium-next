/*
 * exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: PHD2 exception types with modern C++ features

*************************************************/

#pragma once

#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <format>

namespace phd2 {

/**
 * @brief Base exception class for PHD2 client errors
 */
class PHD2Exception : public std::runtime_error {
public:
    explicit PHD2Exception(
        std::string_view message,
        std::source_location location = std::source_location::current())
        : std::runtime_error(std::format("[{}:{}] PHD2 Error: {}",
                                         location.file_name(),
                                         location.line(),
                                         message)),
          location_(location) {}

    [[nodiscard]] auto location() const noexcept -> const std::source_location& {
        return location_;
    }

private:
    std::source_location location_;
};

/**
 * @brief Exception thrown when connection to PHD2 fails
 */
class ConnectionException : public PHD2Exception {
public:
    explicit ConnectionException(
        std::string_view message,
        std::source_location location = std::source_location::current())
        : PHD2Exception(std::format("Connection error: {}", message), location) {}
};

/**
 * @brief Exception thrown for RPC errors
 */
class RpcException : public PHD2Exception {
public:
    RpcException(
        std::string_view message,
        int errorCode,
        std::source_location location = std::source_location::current())
        : PHD2Exception(std::format("RPC error (code {}): {}", errorCode, message),
                        location),
          code_(errorCode) {}

    [[nodiscard]] auto code() const noexcept -> int { return code_; }

private:
    int code_;
};

/**
 * @brief Exception thrown for timeout errors
 */
class TimeoutException : public PHD2Exception {
public:
    explicit TimeoutException(
        std::string_view message,
        int timeoutMs = 0,
        std::source_location location = std::source_location::current())
        : PHD2Exception(std::format("Timeout ({}ms): {}", timeoutMs, message),
                        location),
          timeoutMs_(timeoutMs) {}

    [[nodiscard]] auto timeoutMs() const noexcept -> int { return timeoutMs_; }

private:
    int timeoutMs_;
};

/**
 * @brief Exception thrown when an operation is attempted in an invalid state
 */
class InvalidStateException : public PHD2Exception {
public:
    explicit InvalidStateException(
        std::string_view message,
        std::string_view currentState = "",
        std::source_location location = std::source_location::current())
        : PHD2Exception(
              currentState.empty()
                  ? std::format("Invalid state: {}", message)
                  : std::format("Invalid state ({}): {}", currentState, message),
              location),
          currentState_(currentState) {}

    [[nodiscard]] auto currentState() const noexcept -> std::string_view {
        return currentState_;
    }

private:
    std::string currentState_;
};

/**
 * @brief Exception thrown for parse errors
 */
class ParseException : public PHD2Exception {
public:
    explicit ParseException(
        std::string_view message,
        std::string_view input = "",
        std::source_location location = std::source_location::current())
        : PHD2Exception(std::format("Parse error: {}", message), location),
          input_(input) {}

    [[nodiscard]] auto input() const noexcept -> std::string_view {
        return input_;
    }

private:
    std::string input_;
};

/**
 * @brief Exception thrown when equipment is not connected
 */
class EquipmentNotConnectedException : public PHD2Exception {
public:
    explicit EquipmentNotConnectedException(
        std::string_view equipment = "equipment",
        std::source_location location = std::source_location::current())
        : PHD2Exception(std::format("{} not connected", equipment), location),
          equipment_(equipment) {}

    [[nodiscard]] auto equipment() const noexcept -> std::string_view {
        return equipment_;
    }

private:
    std::string equipment_;
};

/**
 * @brief Exception thrown when calibration fails
 */
class CalibrationException : public PHD2Exception {
public:
    explicit CalibrationException(
        std::string_view message,
        std::source_location location = std::source_location::current())
        : PHD2Exception(std::format("Calibration error: {}", message), location) {}
};

}  // namespace phd2
