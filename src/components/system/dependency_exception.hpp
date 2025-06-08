#ifndef LITHIUM_SYSTEM_DEPENDENCY_EXCEPTION_HPP
#define LITHIUM_SYSTEM_DEPENDENCY_EXCEPTION_HPP

#include <cstdint>
#include <exception>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include "exception/exception.hpp"

namespace lithium::system {

/**
 * @enum DependencyErrorCode
 * @brief Enumerates error codes for dependency management operations.
 */
enum class DependencyErrorCode : uint32_t {
    SUCCESS = 0,                ///< Operation succeeded.
    PACKAGE_MANAGER_NOT_FOUND,  ///< No suitable package manager found.
    INSTALL_FAILED,             ///< Installation of dependency failed.
    UNINSTALL_FAILED,           ///< Uninstallation of dependency failed.
    DEPENDENCY_NOT_FOUND,       ///< Requested dependency not found.
    CONFIG_LOAD_FAILED,         ///< Failed to load configuration.
    INVALID_VERSION,            ///< Invalid version specified or detected.
    NETWORK_ERROR,              ///< Network-related error occurred.
    PERMISSION_DENIED,          ///< Insufficient permissions for operation.
    UNKNOWN_ERROR               ///< An unknown error occurred.
};

/**
 * @class DependencyException
 * @brief Exception type for dependency management errors, supporting rich
 * context and error codes.
 *
 * Inherits from lithium::exception::ComponentException and provides additional
 * information specific to dependency management failures.
 */
class DependencyException : public lithium::exception::ComponentException {
public:
    /**
     * @brief Construct a DependencyException with error code, message, context,
     * tags, and source location.
     * @param code The error code.
     * @param message Description of the error.
     * @param context Optional error context information.
     * @param tags Optional tags for categorization.
     * @param location Source location where the exception was thrown.
     */
    DependencyException(
        DependencyErrorCode code, std::string_view message,
        lithium::exception::ErrorContext context = {},
        std::vector<std::string> tags = {},
        const std::source_location& location = std::source_location::current())
        : ComponentException(static_cast<uint32_t>(code), message,
                             std::move(context), tags, location),
          code_(code) {}

    /**
     * @brief Construct a DependencyException with error code, message, inner
     * exception, context, tags, and source location.
     * @param code The error code.
     * @param message Description of the error.
     * @param inner Inner exception to chain.
     * @param context Optional error context information.
     * @param tags Optional tags for categorization.
     * @param location Source location where the exception was thrown.
     */
    DependencyException(
        DependencyErrorCode code, std::string_view message,
        const std::exception& inner,
        lithium::exception::ErrorContext context = {},
        std::vector<std::string> tags = {},
        const std::source_location& location = std::source_location::current())
        : ComponentException(static_cast<uint32_t>(code), message,
                             std::move(context), tags, location),
          code_(code) {
        setInnerException(std::make_exception_ptr(inner));
    }

    /**
     * @brief Get the dependency error code.
     * @return The DependencyErrorCode associated with this exception.
     */
    [[nodiscard]] DependencyErrorCode errorCode() const noexcept {
        return code_;
    }

private:
    DependencyErrorCode code_;
};

/**
 * @class DependencyError
 * @brief Enhanced error type for dependency operations, supporting context and
 * JSON serialization.
 */
class DependencyError {
public:
    /**
     * @brief Construct a DependencyError.
     * @param code The error code.
     * @param message Description of the error.
     * @param context Optional error context information.
     */
    DependencyError(DependencyErrorCode code, std::string message,
                    lithium::exception::ErrorContext context = {})
        : code_(code),
          message_(std::move(message)),
          context_(std::move(context)) {}

    /**
     * @brief Get the error code.
     * @return The DependencyErrorCode.
     */
    [[nodiscard]] auto code() const -> DependencyErrorCode { return code_; }

    /**
     * @brief Get the error message.
     * @return The error message string.
     */
    [[nodiscard]] auto message() const -> const std::string& {
        return message_;
    }

    /**
     * @brief Get the error context.
     * @return The ErrorContext object.
     */
    [[nodiscard]] auto context() const
        -> const lithium::exception::ErrorContext& {
        return context_;
    }

    /**
     * @brief Serialize the error to JSON.
     * @return nlohmann::json object representing the error.
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json {
        return {{"code", static_cast<uint32_t>(code_)},
                {"message", message_},
                {"context", context_.toJson()}};
    }

private:
    DependencyErrorCode code_;
    std::string message_;
    lithium::exception::ErrorContext context_;
};

/**
 * @struct DependencyResult
 * @brief Result type for asynchronous or fallible dependency operations.
 * @tparam T The type of the successful result value.
 *
 * Contains either a value or an error.
 */
template <typename T>
struct DependencyResult {
    std::optional<T> value;                ///< The result value, if successful.
    std::optional<DependencyError> error;  ///< The error, if operation failed.
};

/**
 * @struct DependencyResult<void>
 * @brief Specialization for operations that do not return a value.
 *
 * Contains a success flag and an optional error.
 */
template <>
struct DependencyResult<void> {
    bool success{false};  ///< Indicates if the operation succeeded.
    std::optional<DependencyError> error;  ///< The error, if operation failed.
};

}  // namespace lithium::system

#endif  // LITHIUM_SYSTEM_DEPENDENCY_EXCEPTION_HPP
