/**
 * @file enhanced_exception.hpp
 * @brief Enhanced exception handling system using C++23 features
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_EXCEPTION_ENHANCED_HPP
#define LITHIUM_EXCEPTION_ENHANCED_HPP

#include <chrono>
#include <concepts>
#include <exception>
#include <format>
#include <source_location>
#include <stacktrace>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>


#include "atom/error/exception.hpp"
#include "atom/type/expected.hpp"
#include "atom/type/json.hpp"

#undef ERROR


using json = nlohmann::json;

namespace lithium::exception {

// Enhanced error severity levels
enum class ErrorSeverity : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5,
    FATAL = 6
};

// Error categories for better classification
enum class ErrorCategory : uint16_t {
    UNKNOWN = 0,
    SYSTEM = 100,
    NETWORK = 200,
    DATABASE = 300,
    FILESYSTEM = 400,
    MEMORY = 500,
    COMPONENT = 600,
    SERVER = 700,
    DEBUG = 800,
    SECURITY = 900,
    CONFIGURATION = 1000,
    USER_INPUT = 1100,
    EXTERNAL_SERVICE = 1200
};

// Error context information
struct ErrorContext {
    std::string operation;
    std::string module;
    std::string function;
    json metadata;
    std::chrono::steady_clock::time_point timestamp;
    std::thread::id threadId;

    ErrorContext(std::string_view op = "", std::string_view mod = "",
                 std::string_view func = "", json meta = {})
        : operation(op),
          module(mod),
          function(func),
          metadata(std::move(meta)),
          timestamp(std::chrono::steady_clock::now()),
          threadId(std::this_thread::get_id()) {}

    [[nodiscard]] auto toJson() const -> json {
        return json{
            {"operation", operation},
            {"module", module},
            {"function", function},
            {"metadata", metadata},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                              timestamp.time_since_epoch())
                              .count()},
            {"threadId", std::to_string(std::hash<std::thread::id>{}(threadId))}};
    }
};

// Enhanced exception base class with modern C++ features
class EnhancedException : public atom::error::Exception {
private:
    ErrorSeverity severity_;
    ErrorCategory category_;
    uint32_t errorCode_;
    ErrorContext context_;
    std::stacktrace stackTrace_;
    std::vector<std::string> tags_;
    std::optional<std::exception_ptr> innerException_;

public:
    template <typename... Args>
    explicit EnhancedException(
        ErrorSeverity severity, ErrorCategory category, uint32_t errorCode,
        std::string_view message, ErrorContext context = {},
        std::vector<std::string> tags = {},
        const std::source_location& location = std::source_location::current(),
        Args&&... args)
        : Exception(location.file_name(), location.line(),
                    location.function_name(),
                    format_message(message, std::forward<Args>(args)...)),
          severity_(severity),
          category_(category),
          errorCode_(errorCode),
          context_(std::move(context)),
          stackTrace_(std::stacktrace::current()),
          tags_(std::move(tags)) {
        // Update context with source location info if not provided
        if (context_.function.empty()) {
            context_.function = location.function_name();
        }
    }

    // 禁止拷贝和移动
    EnhancedException(const EnhancedException&) = delete;
    EnhancedException(EnhancedException&&) = delete;

private:
    template <typename... FmtArgs>
    static std::string format_message(std::string_view msg,
                                      [[maybe_unused]] FmtArgs&&... fmt_args) {
        if constexpr (sizeof...(FmtArgs) == 0) {
            return std::string(msg);
        } else {
#if defined(__cpp_lib_format) && __cpp_lib_format >= 202207L
            try {
                return std::vformat(std::string(msg),
                                    std::make_format_args(fmt_args...));
            } catch (...) {
                return std::string(msg);  // fallback on formatting error
            }
#else
            return std::string(msg);  // fallback: no formatting if std::vformat
                                      // is not available
#endif
        }
    }

public:
    // Assignment operators
    EnhancedException& operator=(const EnhancedException&) = delete;
    EnhancedException& operator=(EnhancedException&&) = delete;

    // Accessors
    [[nodiscard]] auto getSeverity() const noexcept -> ErrorSeverity {
        return severity_;
    }
    [[nodiscard]] auto getCategory() const noexcept -> ErrorCategory {
        return category_;
    }
    [[nodiscard]] auto getErrorCode() const noexcept -> uint32_t {
        return errorCode_;
    }
    [[nodiscard]] auto getContext() const noexcept -> const ErrorContext& {
        return context_;
    }
    [[nodiscard]] auto getStackTrace() const noexcept
        -> const std::stacktrace& {
        return stackTrace_;
    }
    [[nodiscard]] auto getTags() const noexcept
        -> const std::vector<std::string>& {
        return tags_;
    }

    // Inner exception handling
    void setInnerException(std::exception_ptr ex) { innerException_ = ex; }

    [[nodiscard]] auto hasInnerException() const noexcept -> bool {
        return innerException_.has_value();
    }

    [[nodiscard]] auto getInnerException() const -> std::exception_ptr {
        return innerException_.value_or(nullptr);
    }

    // Enhanced what() method with detailed information
    [[nodiscard]] const char* what() const noexcept override {
        try {
            static thread_local std::string detailedMessage;
            detailedMessage = std::format(
                "[{}] [{}:{}] {}: {} (Code: {}, Category: {}, Severity: {})",
                context_.module.empty() ? "Unknown" : context_.module,
                static_cast<uint16_t>(category_), errorCode_,
                context_.operation.empty() ? "Operation" : context_.operation,
                Exception::what(), errorCode_, static_cast<uint8_t>(category_),
                static_cast<uint8_t>(severity_));
            return detailedMessage.c_str();
        } catch (...) {
            return Exception::what();
        }
    }

    // JSON serialization - made public
    [[nodiscard]] auto toJson() const -> json {
        json result{{"type", "EnhancedException"},
                    {"message", Exception::what()},
                    {"severity", static_cast<uint8_t>(severity_)},
                    {"category", static_cast<uint16_t>(category_)},
                    {"errorCode", errorCode_},
                    {"context", context_.toJson()},
                    {"tags", tags_}};

        // Add stack trace
        json stackTraceJson = json::array();
        for (const auto& frame : stackTrace_) {
            stackTraceJson.push_back(frame.description()); // Use description() instead of format
        }
        result["stackTrace"] = stackTraceJson;

        // Add inner exception if present
        if (hasInnerException()) {
            try {
                std::rethrow_exception(getInnerException());
            } catch (const EnhancedException& inner) {
                result["innerException"] = inner.toJson();
            } catch (const std::exception& inner) {
                result["innerException"] =
                    json{{"type", "std::exception"}, {"message", inner.what()}};
            } catch (...) {
                result["innerException"] =
                    json{{"type", "unknown"}, {"message", "Unknown exception"}};
            }
        }

        return result;
    }

    // String representation with full details
    [[nodiscard]] auto toString() const -> std::string {
        return toJson().dump(2);
    }
};

// Specific exception types for different categories
class SystemException : public EnhancedException {
public:
    template <typename... Args>
    explicit SystemException(uint32_t code, std::string_view message,
                             ErrorContext context = {}, Args&&... args)
        : EnhancedException(ErrorSeverity::ERROR, ErrorCategory::SYSTEM, code,
                            message, std::move(context), {"system"},
                            std::source_location::current(),
                            std::forward<Args>(args)...) {}
};

class NetworkException : public EnhancedException {
public:
    template <typename... Args>
    explicit NetworkException(uint32_t code, std::string_view message,
                              ErrorContext context = {}, Args&&... args)
        : EnhancedException(ErrorSeverity::ERROR, ErrorCategory::NETWORK, code,
                            message, std::move(context), {"network"},
                            std::source_location::current(),
                            std::forward<Args>(args)...) {}
};

class ComponentException : public EnhancedException {
public:
    template <typename... Args>
    explicit ComponentException(uint32_t code, std::string_view message,
                                ErrorContext context = {}, Args&&... args)
        : EnhancedException(ErrorSeverity::ERROR, ErrorCategory::COMPONENT,
                            code, message, std::move(context), {"component"},
                            std::source_location::current(),
                            std::forward<Args>(args)...) {}
};

class ServerException : public EnhancedException {
public:
    template <typename... Args>
    explicit ServerException(uint32_t code, std::string_view message,
                             ErrorContext context = {}, Args&&... args)
        : EnhancedException(ErrorSeverity::ERROR, ErrorCategory::SERVER, code,
                            message, std::move(context), {"server"},
                            std::source_location::current(),
                            std::forward<Args>(args)...) {}
};

class DebugException : public EnhancedException {
public:
    template <typename... Args>
    explicit DebugException(uint32_t code, std::string_view message,
                            ErrorContext context = {}, Args&&... args)
        : EnhancedException(ErrorSeverity::ERROR, ErrorCategory::DEBUG, code,
                            message, std::move(context), {"debug"},
                            std::source_location::current(),
                            std::forward<Args>(args)...) {}
};

// Result types using atom::type::expected
template <typename T>
using Result = atom::type::expected<T, EnhancedException>;

using VoidResult = Result<void>;

// Helper concepts for error handling
template <typename T>
concept Exception = std::derived_from<T, std::exception>;

template <typename T>
concept EnhancedExceptionType = std::derived_from<T, EnhancedException>;

// Error handling utilities
class ErrorHandler {
public:
    // Exception chaining
    template <EnhancedExceptionType ExceptionT, typename... Args>
    [[noreturn]] static void throwWithInner(std::exception_ptr inner,
                                            Args&&... args) {
        auto ex = ExceptionT(std::forward<Args>(args)...);
        ex.setInnerException(inner);
        throw ex;
    }

    // Safe function execution with error handling
    template <typename Func, typename... Args>
    static auto safeExecute(Func&& func, Args&&... args)
        -> Result<std::invoke_result_t<Func, Args...>> {
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
                std::invoke(std::forward<Func>(func),
                            std::forward<Args>(args)...);
                return {};
            } else {
                return std::invoke(std::forward<Func>(func),
                                   std::forward<Args>(args)...);
            }
        } catch (const EnhancedException& ex) {
            return atom::type::make_unexpected(ex);
        } catch (const std::exception& ex) {
            auto enhancedEx = SystemException(
                0, "Unhandled standard exception: {}",
                ErrorContext("safeExecute", "ErrorHandler", __FUNCTION__),
                ex.what());
            return atom::type::make_unexpected(enhancedEx);
        } catch (...) {
            auto enhancedEx = SystemException(
                0, "Unknown exception caught",
                ErrorContext("safeExecute", "ErrorHandler", __FUNCTION__));
            return atom::type::make_unexpected(enhancedEx);
        }
    }

    // Async error handling for coroutines
    template <typename T>
    static auto handleAsync(Result<T> result) -> std::optional<T> {
        if (result.has_value()) {
            return result.value();
        } else {
            // Log error or handle as needed
            return std::nullopt;
        }
    }

    // Error aggregation for multiple operations
    class ErrorCollector {
    private:
        std::vector<EnhancedException> errors_;

    public:
        template <typename T>
        auto collect(Result<T> result) -> std::optional<T> {
            if (result.has_value()) {
                return result.value();
            } else {
                errors_.push_back(result.error().error());
                return std::nullopt;
            }
        }

        [[nodiscard]] auto hasErrors() const -> bool {
            return !errors_.empty();
        }

        [[nodiscard]] auto getErrors() const
            -> const std::vector<EnhancedException>& {
            return errors_;
        }

        [[nodiscard]] auto createAggregateException() const -> SystemException {
            if (errors_.empty()) {
                return SystemException(0, "No errors collected");
            }

            json errorList = json::array();
            for (const auto& error : errors_) {
                errorList.push_back(error.toJson());
            }

            return SystemException(
                static_cast<uint32_t>(errors_.size()),
                "Multiple errors occurred ({})",
                ErrorContext("aggregate", "ErrorCollector", __FUNCTION__),
                errors_.size());
        }
    };
};

// RAII error context helper
class ErrorContextScope {
private:
    thread_local static std::vector<ErrorContext> contextStack_;

public:
    explicit ErrorContextScope(ErrorContext context) {
        contextStack_.push_back(std::move(context));
    }

    ~ErrorContextScope() {
        if (!contextStack_.empty()) {
            contextStack_.pop_back();
        }
    }

    [[nodiscard]] static auto getCurrentContext() -> ErrorContext {
        return contextStack_.empty() ? ErrorContext{} : contextStack_.back();
    }
};

// Convenience macros for exception handling
#define ENHANCED_TRY_CATCH(operation) \
    ErrorHandler::safeExecute([&]() { return (operation); })

#define ENHANCED_THROW(ExceptionType, code, message, ...)                      \
    throw ExceptionType(code, message, ErrorContextScope::getCurrentContext(), \
                        ##__VA_ARGS__)

#define ENHANCED_CONTEXT(operation, module, function) \
    ErrorContextScope _ctx_scope(ErrorContext(operation, module, function))

#define ENHANCED_CONTEXT_WITH_META(operation, module, function, metadata) \
    ErrorContextScope _ctx_scope(                                         \
        ErrorContext(operation, module, function, metadata))

}  // namespace lithium::exception

#endif  // LITHIUM_EXCEPTION_ENHANCED_HPP
