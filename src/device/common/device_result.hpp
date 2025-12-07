/*
 * device_result.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device operation result types using std::expected

**************************************************/

#ifndef LITHIUM_DEVICE_COMMON_DEVICE_RESULT_HPP
#define LITHIUM_DEVICE_COMMON_DEVICE_RESULT_HPP

#include <expected>
#include <functional>
#include <type_traits>

#include "device_error.hpp"
#include "device_exceptions.hpp"

namespace lithium::device {

/**
 * @brief Result type for device operations
 *
 * Uses std::expected to represent either a successful value or an error.
 * This provides a functional approach to error handling without exceptions.
 */
template <typename T>
using DeviceResult = std::expected<T, DeviceError>;

/**
 * @brief Result type for operations with no return value
 */
using DeviceVoidResult = DeviceResult<void>;

/**
 * @brief Create a successful result
 */
template <typename T>
[[nodiscard]] inline auto success(T&& value) -> DeviceResult<std::decay_t<T>> {
    return DeviceResult<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Create a successful void result
 */
[[nodiscard]] inline auto success() -> DeviceVoidResult {
    return DeviceVoidResult();
}

/**
 * @brief Create a failure result
 */
template <typename T>
[[nodiscard]] inline auto failure(const DeviceError& error) -> DeviceResult<T> {
    return std::unexpected(error);
}

/**
 * @brief Create a failure result with error code and message
 */
template <typename T>
[[nodiscard]] inline auto failure(DeviceErrorCode code,
                                  const std::string& message)
    -> DeviceResult<T> {
    return std::unexpected(DeviceError(code, message));
}

/**
 * @brief Create a failure void result
 */
[[nodiscard]] inline auto failure(const DeviceError& error) -> DeviceVoidResult {
    return std::unexpected(error);
}

/**
 * @brief Create a failure void result with error code and message
 */
[[nodiscard]] inline auto failure(DeviceErrorCode code,
                                  const std::string& message)
    -> DeviceVoidResult {
    return std::unexpected(DeviceError(code, message));
}

/**
 * @brief Map a result to another type
 */
template <typename T, typename F>
[[nodiscard]] auto map(DeviceResult<T>&& result, F&& func)
    -> DeviceResult<std::invoke_result_t<F, T>> {
    if (result) {
        return success(std::invoke(std::forward<F>(func), std::move(*result)));
    }
    return std::unexpected(result.error());
}

/**
 * @brief Flat map a result (for chaining operations)
 */
template <typename T, typename F>
[[nodiscard]] auto flatMap(DeviceResult<T>&& result, F&& func)
    -> std::invoke_result_t<F, T> {
    if (result) {
        return std::invoke(std::forward<F>(func), std::move(*result));
    }
    using ResultType = std::invoke_result_t<F, T>;
    return std::unexpected(result.error());
}

/**
 * @brief Execute callback if result is successful
 */
template <typename T, typename F>
auto onSuccess(DeviceResult<T>&& result, F&& func) -> DeviceResult<T> {
    if (result) {
        std::invoke(std::forward<F>(func), *result);
    }
    return std::move(result);
}

/**
 * @brief Execute callback if result is an error
 */
template <typename T, typename F>
auto onError(DeviceResult<T>&& result, F&& func) -> DeviceResult<T> {
    if (!result) {
        std::invoke(std::forward<F>(func), result.error());
    }
    return std::move(result);
}

/**
 * @brief Get value or default if error
 */
template <typename T>
[[nodiscard]] auto valueOr(DeviceResult<T>&& result, T&& defaultValue) -> T {
    if (result) {
        return std::move(*result);
    }
    return std::forward<T>(defaultValue);
}

/**
 * @brief Convert result to optional (discarding error info)
 */
template <typename T>
[[nodiscard]] auto toOptional(DeviceResult<T>&& result) -> std::optional<T> {
    if (result) {
        return std::move(*result);
    }
    return std::nullopt;
}

/**
 * @brief Throw exception if result is error
 */
template <typename T>
auto throwIfError(DeviceResult<T>&& result) -> T {
    if (!result) {
        throw DeviceException(result.error());
    }
    return std::move(*result);
}

/**
 * @brief Throw exception if void result is error
 */
inline void throwIfError(DeviceVoidResult&& result) {
    if (!result) {
        throw DeviceException(result.error());
    }
}

/**
 * @brief Combine multiple results (all must succeed)
 */
template <typename... Ts>
[[nodiscard]] auto combine(DeviceResult<Ts>&&... results)
    -> DeviceResult<std::tuple<Ts...>> {
    // Check if all results are successful
    bool allSuccess = (results.has_value() && ...);
    if (!allSuccess) {
        // Find first error
        DeviceError firstError;
        bool found = false;
        (
            [&](auto&& result) {
                if (!found && !result.has_value()) {
                    firstError = result.error();
                    found = true;
                }
            }(std::move(results)),
            ...);
        return std::unexpected(firstError);
    }
    return std::make_tuple(std::move(*results)...);
}

/**
 * @brief Try to execute a function and convert exceptions to DeviceResult
 */
template <typename F>
[[nodiscard]] auto tryExecute(F&& func)
    -> DeviceResult<std::invoke_result_t<F>> {
    using ReturnType = std::invoke_result_t<F>;
    try {
        if constexpr (std::is_void_v<ReturnType>) {
            std::invoke(std::forward<F>(func));
            return success();
        } else {
            return success(std::invoke(std::forward<F>(func)));
        }
    } catch (const DeviceException& e) {
        return std::unexpected(e.error());
    } catch (const std::exception& e) {
        return std::unexpected(
            DeviceError(DeviceErrorCode::InternalError, e.what()));
    } catch (...) {
        return std::unexpected(
            DeviceError(DeviceErrorCode::Unknown, "Unknown exception"));
    }
}

/**
 * @brief Helper class for building device results with context
 */
class ResultBuilder {
public:
    ResultBuilder() = default;

    auto withDevice(const std::string& name) -> ResultBuilder& {
        deviceName_ = name;
        return *this;
    }

    auto withOperation(const std::string& name) -> ResultBuilder& {
        operationName_ = name;
        return *this;
    }

    template <typename T>
    [[nodiscard]] auto success(T&& value) const
        -> DeviceResult<std::decay_t<T>> {
        return DeviceResult<std::decay_t<T>>(std::forward<T>(value));
    }

    [[nodiscard]] auto success() const -> DeviceVoidResult {
        return DeviceVoidResult();
    }

    template <typename T>
    [[nodiscard]] auto error(DeviceErrorCode code,
                             const std::string& message) const
        -> DeviceResult<T> {
        auto err = DeviceError::create(code, message, deviceName_.value_or(""),
                                       operationName_.value_or(""));
        return std::unexpected(err);
    }

    [[nodiscard]] auto error(DeviceErrorCode code,
                             const std::string& message) const
        -> DeviceVoidResult {
        auto err = DeviceError::create(code, message, deviceName_.value_or(""),
                                       operationName_.value_or(""));
        return std::unexpected(err);
    }

private:
    std::optional<std::string> deviceName_;
    std::optional<std::string> operationName_;
};

/**
 * @brief Create a result builder for fluent API
 */
[[nodiscard]] inline auto result() -> ResultBuilder { return ResultBuilder(); }

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_COMMON_DEVICE_RESULT_HPP
