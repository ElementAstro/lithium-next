# Exception Handling Patterns and Usage Guide

## Overview

This document details practical exception handling patterns found in the lithium-next codebase, categorized by use case with code examples and rationales.

---

## Evidence Section

<CodeSection>

## Macro-Based Exception Throwing Pattern

**File:** `D:\Project\lithium-next\libs\atom\atom\error\exception.hpp`
**Lines:** 130-448
**Purpose:** Standardized exception throwing with automatic source location injection

**Common macro patterns:**

```cpp
// Generic exceptions
#define THROW_EXCEPTION(...) \
    throw atom::error::Exception(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_RUNTIME_ERROR(...) \
    throw atom::error::RuntimeError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                    ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_LOGIC_ERROR(...) \
    throw atom::error::LogicError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__)

// Argument validation
#define THROW_INVALID_ARGUMENT(...) \
    throw atom::error::InvalidArgument(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_MISSING_ARGUMENT(...) \
    throw atom::error::MissingArgument(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

// File operations
#define THROW_FILE_NOT_FOUND(...) \
    throw atom::error::FileNotFound(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                    ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_FAIL_TO_OPEN_FILE(...) \
    throw atom::error::FailToOpenFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_FAIL_TO_READ_FILE(...) \
    throw atom::error::FailToReadFile(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// JSON parsing
#define THROW_JSON_PARSE_ERROR(...) \
    throw atom::error::JsonParseError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_JSON_VALUE_ERROR(...) \
    throw atom::error::JsonValueError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)
```

**Usage Examples:**

```cpp
// File: src/client/indi/collection.cpp
THROW_FILE_NOT_FOUND("No XML files found in directory: ", path);

// File: src/client/phd2/profile.cpp
THROW_FAIL_TO_OPEN_FILE("Failed to open file for reading.");

// File: src/components/dependency.cpp
THROW_INVALID_ARGUMENT("Node name cannot be empty");
THROW_JSON_PARSE_ERROR("Error parsing JSON in " + std::string(path) +
                       ": " + error);
```

**Key Details:**

- Variadic arguments streamed via fold expression: `((oss << arg), ...)`
- No need to manually pass __FILE__, __LINE__, __func__ to functions
- Works with any printable type (string, int, float, custom types with operator<<)
- Macro names indicate intent and domain (FILE_*, JSON_*, ARGUMENT_*, etc.)

</CodeSection>

<CodeSection>

## Exception Throwing in Device Operations

**File:** `D:\Project\lithium-next\src\device\manager.hpp`, `manager.cpp`
**Lines:** 14-31 (definitions), 120-125 (usage)
**Purpose:** Domain-specific exception throwing for device management

```cpp
// Exception definition with throwing macro
class DeviceNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_NOT_FOUND(...) \
    throw DeviceNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__)

class DeviceTypeNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_TYPE_NOT_FOUND(...) \
    throw DeviceTypeNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

// Usage in device manager
void DeviceManager::setPrimaryDevice(const std::string& type,
                                     std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        if (std::find(it->second.begin(), it->second.end(), device) !=
            it->second.end()) {
            pimpl->primaryDevices[type] = device;
            LOG_F(INFO, "Primary device for {} set to {}", type,
                  device->getName());
        } else {
            THROW_DEVICE_NOT_FOUND("Device not found");
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }
}
```

**Key Details:**

- Domain-specific exception class defined once, reused everywhere
- Paired throwing macro for convenience and consistency
- Used at validation boundaries where preconditions fail
- Message indicates which condition failed (device vs. type)

</CodeSection>

<CodeSection>

## Try-Catch Event Callback Protection

**File:** `D:\Project\lithium-next\src\device\manager.cpp`
**Lines:** 45-55
**Purpose:** Protect asynchronous callback invocations from propagating exceptions

```cpp
void emitEvent(DeviceEventType event, const std::string& deviceType,
               const std::string& deviceName, const json& data = {}) {
    if (eventCallback) {
        try {
            // User-provided callback may throw
            eventCallback(event, deviceType, deviceName, data);
        } catch (const std::exception& e) {
            // Log but do not propagate - event system must remain stable
            LOG_F(ERROR, "DeviceManager: Event callback error: %s",
                  e.what());
        }
    }
}
```

**Key Details:**

- Callback wrapped in try-catch to prevent propagation to caller
- Catches std::exception (covers all atom::error exceptions)
- Error logged with context ("DeviceManager:")
- Empty catch body intentional - error logged, callback continues
- Pattern used for any user-provided callbacks or hooks

</CodeSection>

<CodeSection>

## Try-Catch Device Connection Loop

**File:** `D:\Project\lithium-next\src\device\manager.cpp`
**Lines:** 138-154
**Purpose:** Continue processing devices even if individual connections fail

```cpp
void DeviceManager::connectAllDevices() {
    std::shared_lock lock(pimpl->mtx);
    for (auto& [type, vec] : pimpl->devices) {
        for (auto& device : vec) {
            try {
                device->connect("7624");  // INDI default port
                LOG_F(INFO, "Connected device {} of type {}",
                      device->getName(), type);
            } catch (const DeviceNotFoundException& e) {
                // Handle device-specific error
                LOG_F(ERROR, "Connection error: %s", e.what());
                updateDeviceState(device->getName(), false);
                emitEvent(DeviceEventType::DEVICE_ERROR, type,
                          device->getName(), {});
            }
            // Continue with next device even if this one failed
        }
    }
}
```

**Key Details:**

- Try-catch within loop allows partial success
- Specific exception type caught (DeviceNotFoundException)
- Error handling includes state update and event notification
- No re-throw - loop continues processing remaining devices
- Pattern for batch operations with per-item error handling

</CodeSection>

<CodeSection>

## Try-Catch Network Operations

**File:** `D:\Project\lithium-next\src\client\indi\indi_client.cpp`
**Lines:** 281-303, 422-426
**Purpose:** Handle unpredictable network failures in I/O operations

```cpp
// Pattern 1: Catch-all for network operations
try {
    // Network socket operations may fail unexpectedly
    socket_->connect();
    socket_->sendMessage(message);
    // ... more network I/O
} catch (...) {
    // Unknown exception from socket layer
    LOG_F(ERROR, "Unexpected error in INDI client");
    handleDisconnection();  // Cleanup
    // Exception not re-thrown - operation fails silently with logging
}

// Pattern 2: Standard exception catching with cleanup
try {
    // Setup connection with INDI server
    setupConnection();
} catch (const std::exception& e) {
    // Expected exceptions from application code
    LOG_F(ERROR, "INDI setup error: {}", e.what());
    // Caller can decide whether to retry or propagate
    throw;  // Re-throw for caller handling
}
```

**Key Details:**

- Generic catch (...) for unexpected/unknown exceptions
- std::exception catch for expected application exceptions
- handleDisconnection() called as cleanup operation
- First pattern swallows exception; second re-throws
- Network operations vulnerable to OS-level errors

</CodeSection>

<CodeSection>

## Exception Chaining with rethrowNested

**File:** `D:\Project\lithium-next\libs\atom\atom\error\exception.hpp`
**Lines:** 48-55
**Purpose:** Preserve exception chain for root cause analysis

```cpp
template <typename... Args>
static void rethrowNested(Args &&...args) {
    try {
        throw;  // Re-throw current exception
    } catch (...) {
        // Wrap in new exception with additional context
        std::throw_with_nested(
            Exception(std::forward<Args>(args)...));
    }
}

#define THROW_NESTED_EXCEPTION(...) \
    atom::error::Exception::rethrowNested(ATOM_FILE_NAME, \
                                          ATOM_FILE_LINE, \
                                          ATOM_FUNC_NAME, __VA_ARGS__)
```

**Usage Pattern:**

```cpp
// In a function catching another exception
try {
    // Some operation that throws exception E1
    parseJsonFile(path);
} catch (...) {
    // Add context and re-throw with chain
    THROW_NESTED_EXCEPTION("Additional context about parsing failure");
    // Original exception preserved in exception_ptr chain
}
```

**Key Details:**

- std::throw_with_nested creates exception_ptr chain
- Original exception accessible via std::nested_exception::rethrow_nested()
- Enables debugging of exception propagation path
- Multiple levels of context preserved

</CodeSection>

<CodeSection>

## Result Type Pattern for Non-Throwing Error Handling

**File:** `D:\Project\lithium-next\src\exception\exception.hpp`
**Lines:** 322-374
**Purpose:** Functional error handling without exceptions

```cpp
// Result type using atom::type::expected
template <typename T>
using Result = atom::type::expected<T, EnhancedException>;

using VoidResult = Result<void>;

// Safe execution with automatic exception wrapping
class ErrorHandler {
    template <typename Func, typename... Args>
    static auto safeExecute(Func&& func, Args&&... args)
        -> Result<std::invoke_result_t<Func, Args...>> {
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
                std::invoke(std::forward<Func>(func),
                            std::forward<Args>(args)...);
                return {};  // Success with void result
            } else {
                return std::invoke(std::forward<Func>(func),
                                   std::forward<Args>(args)...);
            }
        } catch (const EnhancedException& ex) {
            return atom::type::make_unexpected(ex);
        } catch (const std::exception& ex) {
            // Wrap std exceptions
            auto enhancedEx = SystemException(0, "Unhandled exception: {}",
                                             ErrorContext(...), ex.what());
            return atom::type::make_unexpected(enhancedEx);
        } catch (...) {
            auto enhancedEx = SystemException(0, "Unknown exception");
            return atom::type::make_unexpected(enhancedEx);
        }
    }
};

// Usage
Result<int> result = ErrorHandler::safeExecute([] {
    // May throw
    return parseInteger("123");
});

if (result.has_value()) {
    int value = result.value();
    // Use value
} else {
    EnhancedException error = result.error();
    // Handle error
}
```

**Key Details:**

- Exceptions converted to Result<T> return value
- Caller decides to check or crash (has_value, error, value)
- No exception propagation across function boundary
- Useful for library APIs, web handlers, async operations

</CodeSection>

<CodeSection>

## Dependency Error Pattern (std::optional)

**File:** `D:\Project\lithium-next\src\components\system\dependency_exception.hpp`
**Lines:** 98-175
**Purpose:** Alternative error handling for dependency resolution

```cpp
// Error type with context and JSON serialization
class DependencyError {
public:
    DependencyError(DependencyErrorCode code, std::string message,
                    lithium::exception::ErrorContext context = {})
        : code_(code), message_(std::move(message)),
          context_(std::move(context)) {}

    [[nodiscard]] auto code() const -> DependencyErrorCode { return code_; }
    [[nodiscard]] auto message() const -> const std::string& {
        return message_;
    }
    [[nodiscard]] auto context() const -> const ErrorContext& {
        return context_;
    }

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        return {{"code", static_cast<uint32_t>(code_)},
                {"message", message_},
                {"context", context_.toJson()}};
    }

private:
    DependencyErrorCode code_;
    std::string message_;
    ErrorContext context_;
};

// Result type combining success and error
template <typename T>
struct DependencyResult {
    std::optional<T> value;
    std::optional<DependencyError> error;
};

// Usage
DependencyResult<std::string> resolvePackage(const std::string& name) {
    if (auto version = findPackage(name)) {
        return {version, std::nullopt};
    } else {
        return {std::nullopt,
                DependencyError(DependencyErrorCode::DEPENDENCY_NOT_FOUND,
                               "Package not found: " + name)};
    }
}

// Caller
auto result = resolvePackage("opencv");
if (result.error) {
    // Log or handle error
    LOG_F(ERROR, "Dependency error: {}", result.error->message());
} else {
    // Use resolved package
    std::string version = result.value.value();
}
```

**Key Details:**

- Plain struct with optional value and optional error
- Error includes typed code, message, and context
- JSON serializable for logging/API responses
- No exception semantics - cleaner control flow

</CodeSection>

<CodeSection>

## Error Context Scope Pattern (RAII)

**File:** `D:\Project\lithium-next\src\exception\exception.hpp`
**Lines:** 431-450
**Purpose:** Automatically track error context through call stack

```cpp
// RAII context manager
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
        return contextStack_.empty() ? ErrorContext{}
                                     : contextStack_.back();
    }
};

// Convenience macros
#define ENHANCED_CONTEXT(operation, module, function) \
    ErrorContextScope _ctx_scope(ErrorContext(operation, module, function))

#define ENHANCED_CONTEXT_WITH_META(operation, module, function, metadata) \
    ErrorContextScope _ctx_scope(ErrorContext(operation, module, function, metadata))

// Usage
void processRequest(const std::string& requestId) {
    ENHANCED_CONTEXT("ProcessRequest", "RequestHandler", __FUNCTION__);

    try {
        // Code here can access context via ErrorContextScope::getCurrentContext()
        validateRequest(requestId);
        executeRequest(requestId);
    } catch (const EnhancedException& e) {
        // Exception automatically includes context
        ENHANCED_THROW(ServerException, 500, "Request failed: {}",
                      e.what());
    }
}
```

**Key Details:**

- Thread-local stack of contexts
- Destructor automatically pops context on scope exit
- Nested scopes create context hierarchy
- Exception handlers access context without parameter passing

</CodeSection>

<CodeSection>

## Error Collector Pattern for Batch Operations

**File:** `D:\Project\lithium-next\src\exception\exception.hpp`
**Lines:** 388-428
**Purpose:** Accumulate errors from multiple operations before reporting

```cpp
class ErrorHandler {
    // ... other methods ...

    class ErrorCollector {
    private:
        std::vector<EnhancedException> errors_;

    public:
        template <typename T>
        auto collect(Result<T> result) -> std::optional<T> {
            if (result.has_value()) {
                return result.value();
            } else {
                errors_.push_back(result.error());
                return std::nullopt;  // Indicates failure but continue
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

// Usage - process multiple files, collect all failures
ErrorHandler::ErrorCollector collector;
std::vector<std::string> files = {"a.txt", "b.txt", "c.txt"};

for (const auto& file : files) {
    auto result = ErrorHandler::safeExecute([&] {
        return parseFile(file);
    });
    collector.collect(result);
    // Continue processing remaining files even if this failed
}

if (collector.hasErrors()) {
    auto aggregateEx = collector.createAggregateException();
    // All errors now in aggregateEx.toJson()["errorList"]
    LOG_F(ERROR, "Multiple failures: {}", aggregateEx.what());
}
```

**Key Details:**

- Accumulates errors without throwing immediately
- Supports partial success scenarios
- Creates aggregate exception with all details in JSON
- Useful for file batch processing, validation, initialization

</CodeSection>

<CodeSection>

## PHD2 Exception Pattern (std::source_location)

**File:** `D:\Project\lithium-next\src\client\phd2\exceptions.hpp`
**Lines:** 23-170
**Purpose:** Exception handling using C++20 source_location directly

```cpp
namespace phd2 {

// Base PHD2 exception with source location
class PHD2Exception : public std::runtime_error {
public:
    explicit PHD2Exception(
        std::string_view message,
        std::source_location location = std::source_location::current())
        : std::runtime_error(std::format("[{}:{}] PHD2 Error: {}",
                                         location.file_name(),
                                         location.line(), message)),
          location_(location) {}

    [[nodiscard]] auto location() const noexcept
        -> const std::source_location& {
        return location_;
    }

private:
    std::source_location location_;
};

// Specialized exceptions with additional context
class RpcException : public PHD2Exception {
public:
    RpcException(std::string_view message, int errorCode,
                 std::source_location location
                     = std::source_location::current())
        : PHD2Exception(std::format("RPC error (code {}): {}",
                                    errorCode, message),
                        location),
          code_(errorCode) {}

    [[nodiscard]] auto code() const noexcept -> int { return code_; }

private:
    int code_;
};

class TimeoutException : public PHD2Exception {
public:
    TimeoutException(std::string_view message, int timeoutMs = 0,
                     std::source_location location
                         = std::source_location::current())
        : PHD2Exception(std::format("Timeout ({}ms): {}", timeoutMs, message),
                        location),
          timeoutMs_(timeoutMs) {}

    [[nodiscard]] auto timeoutMs() const noexcept -> int {
        return timeoutMs_;
    }

private:
    int timeoutMs_;
};

// Usage
void PHD2Client::waitForResponse(int timeoutMs) {
    if (socket_.waitData(timeoutMs) == 0) {
        throw TimeoutException("No data received within timeout", timeoutMs);
        // source_location captured automatically
    }
}

}  // namespace phd2
```

**Key Details:**

- Derives from std::runtime_error (not atom::error::Exception)
- source_location passed as template default parameter
- std::format used for message composition
- Specialized exceptions store domain-specific values (error codes, timeouts)
- No macro-based throwing - direct constructor calls

</CodeSection>

---

## Findings Section

### 1. Macro-Based Exception Throwing is Primary Pattern

**100+ pre-defined throwing macros** cover most scenarios:
- **Generic:** THROW_EXCEPTION, THROW_RUNTIME_ERROR, THROW_LOGIC_ERROR
- **Argument validation:** THROW_INVALID_ARGUMENT, THROW_MISSING_ARGUMENT, THROW_WRONG_ARGUMENT
- **File operations:** THROW_FILE_NOT_FOUND, THROW_FAIL_TO_OPEN_FILE, THROW_FAIL_TO_READ_FILE, THROW_FAIL_TO_WRITE_FILE
- **Object lifecycle:** THROW_OBJ_NOT_EXIST, THROW_OBJ_ALREADY_EXIST, THROW_OBJ_UNINITIALIZED
- **Range errors:** THROW_OUT_OF_RANGE, THROW_OVERFLOW, THROW_UNDERFLOW
- **JSON/Data:** THROW_JSON_PARSE_ERROR, THROW_JSON_VALUE_ERROR
- **Network:** THROW_CURL_INITIALIZATION_ERROR, THROW_CURL_RUNTIME_ERROR
- **Process:** THROW_FAIL_TO_CREATE_PROCESS, THROW_FAIL_TO_TERMINATE_PROCESS

**Benefits:**
- Eliminates boilerplate of passing __FILE__, __LINE__, __func__
- Intent clear from macro name
- Consistent exception types across codebase
- Source location automatically captured

### 2. Callback/Hook Protection Pattern is Standard

**Two usage sites confirmed:**
- Device event callback (DeviceManager::emitEvent)
- WebSocket message handlers
- Network I/O operations

**Pattern:**
```cpp
try {
    userCallback(...);
} catch (const std::exception& e) {
    LOG_F(ERROR, "Context: %s", e.what());
    // Do NOT propagate - continue operation
}
```

**Rationale:** User-provided code must not crash internal event/message loop

### 3. Batch Operation Loop Protection

Device manager connectAllDevices() pattern:
- Each device wrapped in try-catch
- Per-device exception captured, logged, state updated
- Loop continues even if device connection fails
- Event emitted for UI notification

**Key insight:** Partial success is acceptable; one failure doesn't invalidate others

### 4. Exception Re-throwing for Caller Decision

INDI client setup pattern:
```cpp
try {
    setupConnection();
} catch (const std::exception& e) {
    LOG_F(ERROR, "Setup error: {}", e.what());
    throw;  // Let caller decide what to do
}
```

Used when function cannot handle error but must log it

### 5. Generic Catch (...) at Subsystem Boundaries

INDI client I/O operations use catch (...) because:
- Socket layer may throw unknown exceptions
- handleDisconnection() must execute regardless
- No recovery possible - operation fails silently
- Logging provides audit trail

**Guidelines:** Only at system calls, I/O boundaries where exception type unknown

### 6. Result<T> Pattern for Library APIs

EnhancedException supports Result<T> = expected<T, EnhancedException>

**Advantages:**
- No exception propagation across library boundary
- Caller explicitly checks has_value() or crashes
- Composable with other Result-based APIs
- Zero-cost abstraction (empty exception = no allocation)

**Used for:**
- ErrorHandler::safeExecute for any risky function
- DependencyResult for package manager operations
- Web response handlers need deterministic error info

### 7. PHD2 Exception Pattern is Independent

PHD2 client uses different approach:
- Derives from std::runtime_error (not atom::error::Exception)
- Uses C++20 source_location directly
- No throwing macros
- Direct constructor calls

**Rationale:** PHD2 is third-party integration; independent exception hierarchy

### 8. Error Context Stackable via RAII

ErrorContextScope demonstrates:
- Thread-local context stack
- Automatic push/pop with scope
- Nested contexts form call chain
- Exception handlers access context without parameter passing

**Use:** Complex operations that may be called from multiple paths

### 9. Error Collector Enables Partial Success

ErrorCollector pattern for batch operations:
- Process all items, accumulate failures
- Only report aggregate exception at end
- Useful for file validation, bulk imports, initialization

**Example:** Process 100 config files, report which 5 failed

### 10. Exception Chaining Preserves Root Cause

THROW_NESTED_EXCEPTION uses std::throw_with_nested:
- Original exception saved in exception_ptr
- Can be re-thrown and examined later
- Debugging: Understand full propagation path

**Note:** Rarely used in current codebase - mostly forward throwing

### 11. No Exception Translation from System Errors

Exception hierarchy includes SystemErrorException, but not used for:
- errno conversion (POSIX)
- HRESULT/COM errors (Windows)
- Network error codes (except in specialized exceptions)

**Pattern:** Catch system errors, log, throw domain exception (THROW_FAIL_TO_OPEN_FILE)

### 12. Configuration Errors Specialized

BadConfigException and subclasses:
- InvalidConfigException
- ConfigNotFoundException
- ConfigIOException

**Pattern:** catch (const lithium::config::BadConfigException&) at config load point

### 13. Device Exceptions Minimal

Only two device exception types defined:
- DeviceNotFoundException
- DeviceTypeNotFoundException

**Other device errors:** Logged but not thrown (cooling errors, calibration failures)

### 14. Logging Always Accompanies Exceptions

**Three patterns:**
1. Macro-based: LOG_F(ERROR, ...) in catch block
2. Source location in exception: what() includes file:line
3. ErrorContext: includes operation, module, function

Enables audit trail without additional context passing

### 15. No Exception Safety Guarantees Documented

Code follows basic exception safety:
- RAII prevents resource leaks
- Callbacks wrapped to prevent propagation
- No transactional semantics (all-or-nothing)
- Partial success typical for batch operations
