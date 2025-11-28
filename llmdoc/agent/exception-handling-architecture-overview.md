# Exception Handling Architecture Overview

## Executive Summary

The lithium-next project implements a comprehensive, multi-layered exception handling system combining modern C++23 features with domain-specific error handling. The architecture features a base `atom::error::Exception` class from the atom library, enhanced exception types at the lithium namespace level, and specialized domain exceptions (configuration, PHD2, dependency management, devices).

---

## Evidence Section

<CodeSection>

### Core Exception Base Class

**File:** `D:\Project\lithium-next\libs\atom\atom\error\exception.hpp`
**Lines:** 28-104
**Purpose:** Implements the foundational exception class with source location tracking and stack trace support

```cpp
class Exception : public std::exception {
public:
    template <typename... Args>
    Exception(const char *file, int line, const char *func, Args &&...args)
        : file_(file), line_(line), func_(func) {
        std::ostringstream oss;
        ((oss << std::forward<Args>(args)), ...);
        message_ = oss.str();
    }

    auto what() const ATOM_NOEXCEPT -> const char * override;
    auto getFile() const -> std::string;
    auto getLine() const -> int;
    auto getFunction() const -> std::string;
    auto getMessage() const -> std::string;
    auto getThreadId() const -> std::thread::id;

private:
    std::string file_;
    int line_;
    std::string func_;
    std::string message_;
    std::thread::id thread_id_;
    StackTrace stack_trace_;
};
```

**Key Details:**

- Base exception derives from `std::exception`
- Captures source location (file, line, function name) via macro parameters
- Stores thread ID at exception creation time
- Integrates with stack trace system (cpptrace, boost::stacktrace, or custom)
- Uses fold expressions for variadic argument streaming

</CodeSection>

<CodeSection>

### Exception Macro System

**File:** `D:\Project\lithium-next\libs\atom\atom\macro.hpp`
**Lines:** 300-310
**Purpose:** Provides compiler-independent macros for capturing source location information

```cpp
#if defined(__cpp_lib_source_location)
#define ATOM_FILE_LINE std::source_location::current().line()
#define ATOM_FILE_NAME std::source_location::current().file_name()
#define ATOM_FUNC_NAME std::source_location::current().function_name()
#else
#define ATOM_FILE_LINE ATOM_MAKE_STRING(__LINE__)
#define ATOM_FILE_NAME __FILE__
#define ATOM_FUNC_NAME __func__
#endif
```

**Key Details:**

- Uses C++20 source_location when available (GCC 13+, Clang 16+, MSVC 19.28+)
- Falls back to compiler builtins on older compilers
- Macros automatically capture location at exception throw point
- ATOM_NOEXCEPT defined as `noexcept` for C++ (line 231)

</CodeSection>

<CodeSection>

### Exception Throwing Macros

**File:** `D:\Project\lithium-next\libs\atom\atom\error\exception.hpp`
**Lines:** 130-144
**Purpose:** Standardized macro-based exception throwing with automatic location capture

```cpp
#define THROW_EXCEPTION(...)                                     \
    throw atom::error::Exception(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_EXCEPTION(...)                                       \
    atom::error::Exception::rethrowNested(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                          ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_SYSTEM_ERROR(error_code, ...)                                 \
    static_assert(std::is_integral<decltype(error_code)>::value,            \
                  "Error code must be an integral type");                   \
    static_assert(error_code != 0, "Error code must be non-zero");          \
    throw atom::error::SystemErrorException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                            ATOM_FUNC_NAME, error_code,     \
                                            __VA_ARGS__)
```

**Key Details:**

- THROW_EXCEPTION: Basic exception with variadic message arguments
- THROW_NESTED_EXCEPTION: Re-throws current exception with additional context
- THROW_SYSTEM_ERROR: Attaches system error codes and messages
- Compile-time validation of error codes

</CodeSection>

<CodeSection>

### Specialized Exception Classes in atom::error

**File:** `D:\Project\lithium-next\libs\atom\atom\error\exception.hpp`
**Lines:** 150-546
**Purpose:** Pre-defined exception types for common error scenarios

**Common exceptions:**
- RuntimeError (line 150)
- LogicError (line 186)
- UnlawfulOperation (line 195)
- OutOfRange (line 204)
- OverflowException, UnderflowException, LengthException (lines 213-238)

**Object-related exceptions:**
- ObjectAlreadyExist, ObjectAlreadyInitialized, ObjectNotExist, ObjectUninitialized (lines 253-288)
- NullPointer (line 294)
- NotFound (line 303)

**Argument exceptions:**
- WrongArgument, InvalidArgument, MissingArgument (lines 320-345)

**File I/O exceptions:**
- FileNotFound, FileNotReadable, FileNotWritable (lines 351-376)
- FailToOpenFile, FailToCloseFile, FailToCreateFile, FailToDeleteFile (lines 378-412)
- FailToCopyFile, FailToMoveFile, FailToReadFile, FailToWriteFile (lines 414-448)

**Dynamic library exceptions:**
- FailToLoadDll, FailToUnloadDll, FailToLoadSymbol (lines 454-479)

**Process/JSON/Network exceptions:**
- FailToCreateProcess, FailToTerminateProcess (lines 485-501)
- JsonParseError, JsonValueError (lines 507-523)
- CurlInitializationError, CurlRuntimeError (lines 529-545)

</CodeSection>

<CodeSection>

### Enhanced Exception System with C++23

**File:** `D:\Project\lithium-next\src\exception\exception.hpp`
**Lines:** 32-264
**Purpose:** Modern exception system with severity levels, error categories, and JSON serialization

```cpp
enum class ErrorSeverity : uint8_t {
    TRACE = 0,    DEBUG = 1,    INFO = 2,    WARNING = 3,
    ERROR = 4,    CRITICAL = 5, FATAL = 6
};

enum class ErrorCategory : uint16_t {
    UNKNOWN = 0,           SYSTEM = 100,        NETWORK = 200,
    DATABASE = 300,        FILESYSTEM = 400,    MEMORY = 500,
    COMPONENT = 600,       SERVER = 700,        DEBUG = 800,
    SECURITY = 900,        CONFIGURATION = 1000,
    USER_INPUT = 1100,     EXTERNAL_SERVICE = 1200
};

class EnhancedException : public atom::error::Exception {
private:
    ErrorSeverity severity_;
    ErrorCategory category_;
    uint32_t errorCode_;
    ErrorContext context_;
    std::stacktrace stackTrace_;
    std::vector<std::string> tags_;
    std::optional<std::exception_ptr> innerException_;
};
```

**Key Details:**

- Extends atom::error::Exception with severity and category classification
- Includes C++23 std::stacktrace support
- Carries operation context and metadata
- Supports exception chaining via innerException_
- Provides JSON serialization for error tracking systems

</CodeSection>

<CodeSection>

### Error Context and Handler

**File:** `D:\Project\lithium-next\src\exception\exception.hpp`
**Lines:** 63-429
**Purpose:** Captures error context and provides safe execution utilities

```cpp
struct ErrorContext {
    std::string operation;
    std::string module;
    std::string function;
    json metadata;
    std::chrono::steady_clock::time_point timestamp;
    std::thread::id threadId;

    [[nodiscard]] auto toJson() const -> json;
};

class ErrorHandler {
public:
    template <typename Func, typename... Args>
    static auto safeExecute(Func&& func, Args&&... args)
        -> Result<std::invoke_result_t<Func, Args...>>;

    template <typename T>
    static auto handleAsync(Result<T> result) -> std::optional<T>;

    class ErrorCollector {
        auto collect(Result<T> result) -> std::optional<T>;
        [[nodiscard]] auto createAggregateException() const;
    };
};
```

**Key Details:**

- ErrorContext captures operation metadata with timestamp
- ErrorHandler::safeExecute wraps function calls in try-catch
- Returns Result<T> using atom::type::expected for error handling
- ErrorCollector aggregates multiple errors for batch operations

</CodeSection>

<CodeSection>

### Configuration-Specific Exceptions

**File:** `D:\Project\lithium-next\src\config\core\exception.hpp`
**Lines:** 20-72
**Purpose:** Domain-specific exceptions for configuration management

```cpp
namespace lithium::config {

class BadConfigException : public atom::error::Exception {
    using atom::error::Exception::Exception;
};

class InvalidConfigException : public BadConfigException {
    using BadConfigException::BadConfigException;
};

class ConfigNotFoundException : public BadConfigException {
    using BadConfigException::BadConfigException;
};

class ConfigIOException : public BadConfigException {
    using BadConfigException::BadConfigException;
};

}  // namespace lithium::config
```

**Key Details:**

- Inherits from atom::error::Exception for consistency
- Uses macro-based throwing (THROW_*_EXCEPTION)
- Organized in lithium::config namespace
- Backward compatibility aliases in global namespace

</CodeSection>

<CodeSection>

### Device-Related Exceptions

**File:** `D:\Project\lithium-next\src\device\manager.hpp`
**Lines:** 14-31
**Purpose:** Device management specific exceptions

```cpp
class DeviceNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_NOT_FOUND(...)                               \
    throw DeviceNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__)

class DeviceTypeNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_TYPE_NOT_FOUND(...)                              \
    throw DeviceTypeNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)
```

**Key Details:**

- Simple inheritance from atom::error::Exception
- Paired with custom throwing macros
- Used in device manager operations (setPrimaryDevice: line 120)

</CodeSection>

<CodeSection>

### PHD2 Client Exceptions (Modern C++ Style)

**File:** `D:\Project\lithium-next\src\client\phd2\exceptions.hpp`
**Lines:** 23-170
**Purpose:** PHD2 client-specific exceptions using std::source_location

```cpp
namespace phd2 {

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
        -> const std::source_location& { return location_; }
};

class ConnectionException : public PHD2Exception { /* ... */ };
class RpcException : public PHD2Exception {
    [[nodiscard]] auto code() const noexcept -> int { return code_; }
};
class TimeoutException : public PHD2Exception {
    [[nodiscard]] auto timeoutMs() const noexcept -> int;
};
class InvalidStateException : public PHD2Exception {
    [[nodiscard]] auto currentState() const noexcept -> std::string_view;
};

}  // namespace phd2
```

**Key Details:**

- Derives from std::runtime_error (not atom::error::Exception)
- Uses std::source_location directly (C++20)
- Stores operation-specific context (error codes, timeout ms, states)
- Uses std::format for message construction
- Specialized exceptions for connection, RPC, timeout, state, parsing, calibration

</CodeSection>

<CodeSection>

### Dependency Management Exceptions

**File:** `D:\Project\lithium-next\src\components\system\dependency_exception.hpp`
**Lines:** 18-175
**Purpose:** Error handling for dependency resolution operations

```cpp
enum class DependencyErrorCode : uint32_t {
    SUCCESS = 0,
    PACKAGE_MANAGER_NOT_FOUND,
    INSTALL_FAILED,
    UNINSTALL_FAILED,
    DEPENDENCY_NOT_FOUND,
    CONFIG_LOAD_FAILED,
    INVALID_VERSION,
    NETWORK_ERROR,
    PERMISSION_DENIED,
    UNKNOWN_ERROR
};

class DependencyException : public lithium::exception::ComponentException {
public:
    DependencyException(
        DependencyErrorCode code, std::string_view message,
        lithium::exception::ErrorContext context = {},
        std::vector<std::string> tags = {},
        const std::source_location& location
            = std::source_location::current())
        : ComponentException(static_cast<uint32_t>(code), message,
                             std::move(context), tags, location),
          code_(code) {}
};

class DependencyError {
    [[nodiscard]] auto code() const -> DependencyErrorCode;
    [[nodiscard]] auto message() const -> const std::string&;
    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

template <typename T>
struct DependencyResult {
    std::optional<T> value;
    std::optional<DependencyError> error;
};
```

**Key Details:**

- Typed error codes via enum (DependencyErrorCode)
- Both exception (for throwing) and error struct (for Result types)
- DependencyError supports JSON serialization
- Result<T> pattern using std::optional for functional error handling

</CodeSection>

<CodeSection>

### Error Code Definitions

**File:** `D:\Project\lithium-next\libs\atom\atom\error\error_code.hpp`
**Lines:** 18-193
**Purpose:** Centralized error code enumeration for different domains

```cpp
enum class ErrorCodeBase {
    Success = 0,    Failed = 1,     Cancelled = 2
};

enum class FileError : int {
    None = 0,           NotFound = 100,     OpenError = 101,
    AccessDenied = 102, ReadError = 103,    WriteError = 104,
    ParseError = 106,   InvalidPath = 107,  FileCorrupted = 117
};

enum class DeviceError : int {
    None = 0,           NotFound = 201,     NotConnected = 203,
    ExposureError = 210,GainError = 211,    InitializationError = 230,
    CalibrationError = 233
};

enum class NetworkError : int {
    ConnectionLost = 400,    ConnectionRefused = 401,
    DNSLookupFailed = 402,   TimeoutError = 411,
    BandwidthExceeded = 412
};

enum class ServerError : int {
    InvalidParameters = 300,  UnknownDevice = 312,
    AuthenticationError = 322, PermissionDenied = 323
};

enum class ConfigError : int {
    MissingConfig = 800,  InvalidConfig = 801,
    ConfigParseError = 802, ConfigLocked = 807
};
```

**Key Details:**

- Organized by functional domain (File, Device, Network, Database, Memory, etc.)
- Numeric ranges prevent code collision between domains
- 100s: File errors; 200s: Device errors; 400s: Network; 500s: Memory; 800s: Config
- Enables structured error handling and categorization

</CodeSection>

<CodeSection>

### Error Stack Management

**File:** `D:\Project\lithium-next\libs\atom\atom\utils\error_stack.hpp`
**Lines:** 24-113
**Purpose:** Tracks and aggregates error information across application lifetime

```cpp
struct ErrorInfo {
    std::string errorMessage;
    std::string moduleName;
    std::string functionName;
    int line;
    std::string fileName;
    time_t timestamp;
    std::string uuid;
};

class ErrorStack {
    std::vector<ErrorInfo> errorStack_;
    std::vector<ErrorInfo> compressedErrorStack_;

public:
    void insertError(const std::string &errorMessage,
                     const std::string &moduleName,
                     const std::string &functionName, int line,
                     const std::string &fileName);
    [[nodiscard]] auto getCompressedErrors() const -> std::string;
    [[nodiscard]] auto getFilteredErrorsByModule(
        const std::string &moduleName) const;
};
```

**Key Details:**

- Maintains ordered stack of all errors for audit trail
- Compresses duplicate errors for reporting
- Filters errors by module name
- Each error tagged with UUID for tracking
- Supports both raw and compressed error retrieval

</CodeSection>

<CodeSection>

### Try-Catch Usage Pattern (Device Manager)

**File:** `D:\Project\lithium-next\src\device\manager.cpp`
**Lines:** 48-53, 142-146
**Purpose:** Demonstrates common exception handling patterns in device operations

```cpp
// Pattern 1: Event callback protection
void emitEvent(...) {
    if (eventCallback) {
        try {
            eventCallback(event, deviceType, deviceName, data);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "DeviceManager: Event callback error: %s", e.what());
        }
    }
}

// Pattern 2: Device connection with exception handling
void connectAllDevices() {
    std::shared_lock lock(pimpl->mtx);
    for (auto& [type, vec] : pimpl->devices) {
        for (auto& device : vec) {
            try {
                device->connect("7624");
                LOG_F(INFO, "Connected device {} of type {}",
                      device->getName(), type);
            } catch (const DeviceNotFoundException& e) {
                LOG_F(ERROR, "Connection error: %s", e.what());
            }
        }
    }
}

// Pattern 3: Device lookup with exception throwing
void setPrimaryDevice(const std::string& type,
                      std::shared_ptr<AtomDriver> device) {
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        if (std::find(...) != it->second.end()) {
            pimpl->primaryDevices[type] = device;
        } else {
            THROW_DEVICE_NOT_FOUND("Device not found");
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }
}
```

**Key Details:**

- Event callbacks wrapped to prevent propagation
- Device operations use try-catch for specific exception types
- Direct THROW_* macro usage for domain-specific conditions
- Logging paired with exception handling

</CodeSection>

<CodeSection>

### Try-Catch Usage Pattern (INDI Client)

**File:** `D:\Project\lithium-next\src\client\indi\indi_client.cpp`
**Lines:** 281-303, 422-426
**Purpose:** Demonstrates exception handling in network client operations

```cpp
// Pattern: Network I/O exception handling
try {
    // Network operation (lines 281-302)
    socket_->connect();
    socket_->sendMessage(message);
} catch (...) {
    // Generic catch for unexpected exceptions
    LOG_F(ERROR, "Unexpected error in INDI client");
    handleDisconnection();
}

// Pattern: Standard exception catching
try {
    setupConnection();
} catch (const std::exception& e) {
    LOG_F(ERROR, "INDI setup error: {}", e.what());
    // Cleanup on error
}
```

**Key Details:**

- Generic catch (...) for catastrophic failures
- Standard exception catching with logging
- Error handling includes cleanup (handleDisconnection)
- Network operations prioritize safety over granularity

</CodeSection>

<CodeSection>

### THROW_ Macro Usage Examples

**File:** Multiple files (grep results)
**Purpose:** Real-world exception throwing patterns across codebase

```cpp
// File operations
THROW_FILE_NOT_FOUND("No XML files found in directory: ", path);
THROW_FAIL_TO_OPEN_FILE("Failed to open file for reading.");
THROW_FAIL_TO_OPEN_FILE("Failed to open JSON file.");

// Argument validation
THROW_RUNTIME_ERROR("Invalid port number");
THROW_INVALID_ARGUMENT("Node name cannot be empty");
THROW_INVALID_ARGUMENT("Dependency " + to + " not found for node " + from);
THROW_MISSING_ARGUMENT("Missing package name in " + path);

// Logic errors
THROW_RUNTIME_ERROR("Circular dependency detected.");
THROW_RUNTIME_ERROR("Not a valid ELF file: " + executable_);

// Range validation
THROW_OUT_OF_RANGE("Section data out of bounds");

// Configuration parsing
THROW_JSON_PARSE_ERROR("Error parsing JSON in " + path + ": " + error);

// Directory operations
THROW_RUNTIME_ERROR("Failed to create config directory");
```

**Key Details:**

- Variadic messages with operator<< streaming
- Message concatenation with + operator
- Domain-specific macro selection based on context
- Consistent pattern: THROW_[CATEGORY]_[SPECIFIC_CONDITION]

</CodeSection>

---

## Findings Section

### 1. Multi-Layer Exception Architecture

The project implements a **four-layer exception system**:

1. **Base Layer (atom library):** `atom::error::Exception` with source location and stack trace support
2. **Enhanced Layer (lithium namespace):** `EnhancedException` with severity, category, and JSON serialization
3. **Domain-Specific Layer:** Exceptions in namespaces (config::*, phd2::*, system::*, device::*)
4. **Error Codes Layer:** Typed enum-based error codes organized by functional domain

### 2. Exception Class Hierarchy

All custom exceptions ultimately derive from either:
- `atom::error::Exception` (primary pattern in lithium codebase)
- `std::runtime_error` (alternative in PHD2 client for C++20 source_location)

Pattern inheritance chain example:
```
std::exception
├─ atom::error::Exception
│  ├─ lithium::exception::EnhancedException
│  │  ├─ SystemException, NetworkException, ComponentException, etc.
│  │  └─ lithium::system::DependencyException
│  ├─ lithium::config::BadConfigException
│  │  ├─ InvalidConfigException
│  │  ├─ ConfigNotFoundException
│  │  └─ ConfigIOException
│  ├─ DeviceNotFoundException
│  ├─ DeviceTypeNotFoundException
│  ├─ lithium::FailToLoadComponent
│  └─ [50+ specialized types for common scenarios]
└─ std::runtime_error
   └─ phd2::PHD2Exception
      ├─ ConnectionException, RpcException, TimeoutException,
      ├─ InvalidStateException, ParseException
      └─ [3 more specialized types]
```

### 3. Source Location Capture Mechanism

The system uses **three-tier source location capture**:

1. **Compile-time Macros** (atom/macro.hpp):
   - ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME
   - Prefer C++20 std::source_location when available
   - Fall back to compiler builtins (__FILE__, __LINE__, __func__)

2. **Macro-Based Exception Throwing**:
   - THROW_EXCEPTION(...), THROW_NESTED_EXCEPTION(...)
   - THROW_SYSTEM_ERROR(code, ...)
   - ~100+ domain-specific macros (THROW_FILE_NOT_FOUND, THROW_INVALID_ARGUMENT, etc.)

3. **Direct source_location Passing** (C++20 only):
   - PHD2 exceptions use std::source_location::current() directly
   - Enhanced exceptions receive source_location parameter

### 4. Exception Handling Patterns

**Pattern A - Event Callback Protection:**
Wraps callback invocations in try-catch to prevent propagation (device manager, WebSocket)

**Pattern B - Operation-Level Exception Catching:**
Specific exception types (DeviceNotFoundException) caught at operation level with logging

**Pattern C - Generic Fallback:**
Catch-all (...) for unexpected exceptions at critical boundaries

**Pattern D - Exception Conversion:**
Convert std::exception to domain-specific exception with ErrorHandler::safeExecute

**Pattern E - Error Result Objects:**
DependencyResult<T> uses std::optional<T> + std::optional<DependencyError> instead of throwing

### 5. Error Context and Metadata

EnhancedException and dependency management capture:
- Operation name and module
- Function name from source location
- Execution thread ID
- Timestamp (steady_clock for durations, system_clock for absolute time)
- JSON-serializable metadata
- Exception tags for categorization
- Inner exception chaining for root cause analysis

### 6. Error Code Organization

Error codes use numeric ranges by domain:
- 0: Success base code
- 100-199: File errors
- 200-299: Device errors
- 300-399: Server errors (overlaps with Database)
- 400-499: Network errors
- 500-599: Memory errors
- 600-699: [Reserved]
- 700-799: [Reserved]
- 800-899: Configuration errors
- 900-999: Process/thread errors

### 7. Stack Tracing Infrastructure

Three alternative implementations supported:
1. **cpptrace** (fastest, most accurate)
2. **boost::stacktrace** (portable, widely available)
3. **Custom StackTrace** (lightweight fallback)

Selected at compile time via ENABLE_CPPTRACE and ENABLE_BOOST_STACKTRACE defines

### 8. Error Aggregation and Collection

ErrorHandler::ErrorCollector supports:
- Accumulating multiple errors from batch operations
- Creating aggregate exceptions with JSON array of all errors
- hasErrors() predicate for decision logic
- Functional composition with Result<T> types

### 9. Thread-Safe Exception Information

Each exception instance captures:
- std::thread::id at construction
- Timestamp at construction
- Stack trace at construction (in enhanced exceptions)
- Thread-local error context via ErrorContextScope

### 10. Key File Locations

**Core Exception Files:**
- `/libs/atom/atom/error/exception.hpp` - Base exception class, 50+ specialized types, throwing macros
- `/libs/atom/atom/error/exception.cpp` - Exception::what() implementation with stack trace
- `/libs/atom/atom/error/error_code.hpp` - Error code enumerations
- `/libs/atom/atom/utils/error_stack.hpp` - Error history tracking

**Domain-Specific Files:**
- `/src/exception/exception.hpp` - Enhanced exceptions with C++23 features
- `/src/config/core/exception.hpp` - Configuration-specific exceptions
- `/src/device/manager.hpp` - Device management exceptions
- `/src/components/manager/exceptions.hpp` - Component loading exceptions
- `/src/components/system/dependency_exception.hpp` - Dependency resolution exceptions
- `/src/client/phd2/exceptions.hpp` - PHD2 client exceptions (C++20 style)

### 11. Integration Points

Exception handling integrated with:
- **Logging:** LOG_F(ERROR, ...) called in catch blocks for structured logging
- **Web API:** EnhancedException::toJson() for error responses
- **Configuration:** BadConfigException for validation failures
- **Dependency Management:** DependencyException for package manager operations
- **Device Management:** DeviceNotFoundException and DeviceTypeNotFoundException
- **Network Clients:** PHD2Exception and generic std::exception catches

### 12. Modern C++ Features Used

- **C++20:** std::source_location (preferred), std::format
- **C++23:** std::stacktrace, std::optional, concepts (Exception, EnhancedExceptionType)
- **Variadic Templates:** Fold expressions in Exception constructor
- **SFINAE/Concepts:** Compile-time printability checking
- **RAII:** ErrorContextScope for automatic context cleanup
- **Thread-local Storage:** thread_local errorContext vector in ErrorContextScope

### 13. No Custom Exception Handling in Most Domains

Notably absent or minimal:
- Database errors: No dedicated exception classes found (ServerError enum exists but no exception types)
- Memory management: MemoryError enum but no exception types
- Authentication/Security: SECURITY error category in enums but no exception classes
- User input validation: USER_INPUT category but handled as InvalidArgument
