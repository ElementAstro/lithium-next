// phd2_exceptions.h
#pragma once

#include <stdexcept>
#include <string>

namespace phd2 {

/**
 * @brief Base exception class for PHD2 client errors
 */
class PHD2Exception : public std::runtime_error {
public:
    explicit PHD2Exception(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when connection to PHD2 fails
 */
class ConnectionException : public PHD2Exception {
public:
    explicit ConnectionException(const std::string& message)
        : PHD2Exception("PHD2 Connection error: " + message) {}
};

/**
 * @brief Exception thrown for RPC errors
 */
class RpcException : public PHD2Exception {
public:
    int code;

    RpcException(const std::string& message, int errorCode)
        : PHD2Exception("RPC error: " + message), code(errorCode) {}
};

/**
 * @brief Exception thrown for timeout errors
 */
class TimeoutException : public PHD2Exception {
public:
    explicit TimeoutException(const std::string& message)
        : PHD2Exception("Timeout error: " + message) {}
};

/**
 * @brief Exception thrown when an operation is attempted in an invalid state
 */
class InvalidStateException : public PHD2Exception {
public:
    explicit InvalidStateException(const std::string& message)
        : PHD2Exception("Invalid state: " + message) {}
};

/**
 * @brief Exception thrown for parse errors
 */
class ParseException : public PHD2Exception {
public:
    explicit ParseException(const std::string& message)
        : PHD2Exception("Parse error: " + message) {}
};

}  // namespace phd2