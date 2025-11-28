# Logging System Architecture and Implementation Analysis

## Evidence

### Core Logging Libraries in atom Library

<CodeSection>

## Code Section: AtomLog - Enhanced Logger with C++20 Features

**File:** `libs/atom/atom/log/atomlog.hpp`
**Lines:** 1-205
**Purpose:** Provides a thread-safe, file-based logging system with log rotation, custom log levels, and sink support.

```cpp
enum class LogLevel {
    TRACE = 0,      ///< Trace level logging
    DEBUG,          ///< Debug level logging
    INFO,           ///< Info level logging
    WARN,           ///< Warn level logging
    ERROR,          ///< Error level logging
    CRITICAL,       ///< Critical level logging
    OFF             ///< Used to disable logging
};

class Logger {
public:
    explicit Logger(const fs::path& file_name,
                    LogLevel min_level = LogLevel::TRACE,
                    size_t max_file_size = 1048576, int max_files = 10);

    template <typename... Args>
    void trace(const std::string& format, Args&&... args);
    template <typename... Args>
    void debug(const std::string& format, Args&&... args);
    template <typename... Args>
    void info(const std::string& format, Args&&... args);
    template <typename... Args>
    void warn(const std::string& format, Args&&... args);
    template <typename... Args>
    void error(const std::string& format, Args&&... args);
    template <typename... Args>
    void critical(const std::string& format, Args&&... args);

    void setLevel(LogLevel level);
    void setPattern(const std::string& pattern);
    void registerSink(const std::shared_ptr<Logger>& logger);
    void enableSystemLogging(bool enable);
};
```

**Key Details:**
- Uses C++20 `std::format` for type-safe formatting
- Implements template methods for all log levels (trace, debug, info, warn, error, critical)
- Supports log file rotation with configurable max file size and max files count
- Includes thread-safe operations with `shared_mutex`
- Supports sink loggers (multiple destinations)
- Can integrate with system logging (Windows Event Log, Linux syslog, Android)

</CodeSection>

<CodeSection>

## Code Section: AtomLog Implementation - Multi-threaded Worker Pattern

**File:** `libs/atom/atom/log/atomlog.cpp`
**Lines:** 44-286
**Purpose:** Implements the logger with a background worker thread for asynchronous logging.

```cpp
class Logger::LoggerImpl : public std::enable_shared_from_this<LoggerImpl> {
public:
    LoggerImpl(fs::path file_name_, LogLevel min_level, size_t max_file_size,
               int max_files) {
        rotateLogFile();
        worker_ = std::jthread([this](std::stop_token st) { this->run(st); });
    }

    void log(LogLevel level, const std::string& msg) {
        if (static_cast<int>(level) < static_cast<int>(min_level_)) {
            return;
        }
        auto formattedMsg = formatMessage(level, msg);
        {
            std::lock_guard lock(queue_mutex_);
            log_queue_.push(formattedMsg);
        }
        cv_.notify_one();
        // System logging and sink dispatch...
    }

private:
    std::queue<std::string> log_queue_;
    std::jthread worker_;
    std::condition_variable cv_;

    void run(std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
            std::unique_lock lock(queue_mutex_);
            cv_.wait(lock, [this] { return !log_queue_.empty() || finished_; });
            // Write to file...
            log_file_ << msg << std::endl;
            // Handle rotation...
        }
    }
};
```

**Key Details:**
- Uses `std::jthread` for background worker thread
- Implements queue-based asynchronous logging
- Log message formatting includes timestamp, level, thread name, and message
- Automatic file rotation when size exceeds `max_file_size`
- Supports platform-specific system logging (Windows, Linux/macOS, Android)

</CodeSection>

<CodeSection>

## Code Section: Loguru - Lightweight Logging Library

**File:** `libs/atom/atom/log/loguru.hpp`
**Lines:** 1-1390
**Purpose:** Provides macro-based logging with verbosity levels and flexible output configuration.

```cpp
enum NamedVerbosity : Verbosity {
    Verbosity_INVALID = -10,
    Verbosity_OFF = -9,
    Verbosity_FATAL = -3,
    Verbosity_ERROR = -2,
    Verbosity_WARNING = -1,
    Verbosity_INFO = 0,
    Verbosity_DEBUG = +1,
    Verbosity_2 to Verbosity_9,
    Verbosity_MAX = +9,
};

// Main logging macros
#define LOG_F(verbosity_name, ...) \
    VLOG_F(loguru::Verbosity_##verbosity_name, __VA_ARGS__)

#define DLOG_F(verbosity_name, ...) LOG_F(verbosity_name, __VA_ARGS__)  // Debug logging

// Conditional logging
#define LOG_IF_F(verbosity_name, cond, ...) \
    VLOG_IF_F(loguru::Verbosity_##verbosity_name, cond, __VA_ARGS__)

// Scope-based logging
#define LOG_SCOPE_F(verbosity_name, ...) \
    VLOG_SCOPE_F(loguru::Verbosity_##verbosity_name, __VA_ARGS__)

// Check macros
#define CHECK_F(test, ...) CHECK_WITH_INFO_F(test, #test, ##__VA_ARGS__)
#define CHECK_NOTNULL_F(x, ...) CHECK_WITH_INFO_F((x) != nullptr, #x " != nullptr", ##__VA_ARGS__)
#define CHECK_EQ_F(a, b, ...) CHECK_OP_F(a, b, ==, ##__VA_ARGS__)
```

**Key Details:**
- Supports multiple verbosity levels (TRACE to INFO to DEBUG to MAX)
- Uses `std::format` for C++20 compatibility
- Includes scope-based RAII logging for timing function execution
- Provides assertion-like CHECK macros that abort on failure
- Supports error context tracking with `ERROR_CONTEXT` macro
- Debug macros (DLOG_F) conditionally compiled based on NDEBUG

</CodeSection>

<CodeSection>

## Code Section: LoggerManager - Log File Analysis and Upload

**File:** `libs/atom/atom/log/logger.hpp` & `logger.cpp`
**Lines:** 1-200
**Purpose:** Manages log file scanning, searching, analyzing, and uploading.

```cpp
struct LogEntry {
    std::string fileName;     ///< File where entry was recorded
    int lineNumber;           ///< Line number in file
    std::string message;      ///< Log message
};

class LoggerManager {
public:
    void scanLogsFolder(const std::string &folderPath);
    std::vector<LogEntry> searchLogs(std::string_view keyword);
    void uploadFile(const std::string &filePath);
    void analyzeLogs();
};

// Implementation shows log analysis capabilities
void LoggerManager::Impl::analyzeLogs() {
    auto errorMessages = extractErrorMessages();
    std::map<std::string, int> errorTypeCount;
    for (const auto &errorMessage : errorMessages) {
        std::string errorType = getErrorType(errorMessage);
        errorTypeCount[errorType]++;
    }
}
```

**Key Details:**
- Provides utilities for post-processing log files
- Supports multi-threaded log folder scanning
- Analyzes error patterns and frequency
- Includes HTTP upload functionality for log files

</CodeSection>

### Log Initialization in Main Application

<CodeSection>

## Code Section: Log File Setup Function

**File:** `src/app.cpp`
**Lines:** 56-74
**Purpose:** Initializes loguru logging with file output and fatal handler.

```cpp
void setupLogFile() {
    std::filesystem::path logsFolder = std::filesystem::current_path() / "logs";
    if (!std::filesystem::exists(logsFolder)) {
        std::filesystem::create_directory(logsFolder);
    }
    auto now = std::chrono::system_clock::now();
    auto nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::tm *localTime = std::localtime(&nowTimeT);
    char filename[100];
    std::strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S.log", localTime);
    std::filesystem::path logFilePath = logsFolder / filename;

    loguru::add_file(logFilePath.string().c_str(), loguru::Append,
                     loguru::Verbosity_MAX);

    loguru::set_fatal_handler([](const loguru::Message &message) {
        atom::system::saveCrashLog(std::string(message.prefix) +
                                   message.message);
    });
}
```

**Key Details:**
- Creates `logs` directory if it doesn't exist
- Log files named with timestamp: `YYYYMMDD_HHMMSS.log`
- Uses `loguru::add_file()` to register file output at MAX verbosity
- Sets fatal handler to save crash logs via `atom::system::saveCrashLog()`
- Called during application initialization

</CodeSection>

<CodeSection>

## Code Section: Loguru Initialization in Main

**File:** `src/app.cpp`
**Lines:** 142
**Purpose:** Initializes loguru library with command-line argument parsing.

```cpp
loguru::init(argc, argv);
```

**Key Details:**
- Called once at application startup
- Parses `-v` verbosity flag from command-line arguments
- Sets up signal handlers for crash reporting
- Logs program arguments and working directory
- Sets main thread name to "main thread"

</CodeSection>

### Logging Usage Patterns Throughout Codebase

<CodeSection>

## Code Section: Device Manager Logging

**File:** `src/device/manager.cpp`
**Lines:** 50-78
**Purpose:** Demonstrates INFO and ERROR level logging in device management.

```cpp
void DeviceManager::addDevice(const std::string& type,
                              std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->devices[type].push_back(device);
    device->setName(device->getName());
    LOG_F(INFO, "Added device {} of type {}", device->getName(), type);

    if (pimpl->primaryDevices.find(type) == pimpl->primaryDevices.end()) {
        pimpl->primaryDevices[type] = device;
        LOG_F(INFO, "Primary device for {} set to {}", type, device->getName());
    }
}

void DeviceManager::Impl::emitEvent(...) {
    if (eventCallback) {
        try {
            eventCallback(event, deviceType, deviceName, data);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "DeviceManager: Event callback error: %s", e.what());
        }
    }
}
```

**Key Details:**
- Uses `LOG_F(INFO, ...)` for device registration
- Uses `LOG_F(ERROR, ...)` for exception handling
- Demonstrates formatted logging with multiple parameters
- Pattern: `LOG_F(LEVEL, "message with {} placeholders", args)`

</CodeSection>

<CodeSection>

## Code Section: Guider Service Logging

**File:** `src/server/command/guider.cpp`
**Lines:** 29-75
**Purpose:** Shows logging in API command handlers.

```cpp
auto connectGuider(const std::string& host, int port, int timeout) -> json {
    LOG_F(INFO, "connectGuider: host={} port={} timeout={}", host, port, timeout);
    return getGuiderService()->connect(host, port, timeout);
}

auto startGuiding(double settlePixels, double settleTime, double settleTimeout,
                  bool recalibrate) -> json {
    LOG_F(INFO, "startGuiding: settlePixels={} settleTime={} settleTimeout={} recalibrate={}",
          settlePixels, settleTime, settleTimeout, recalibrate);
    return getGuiderService()->startGuiding(...);
}

auto ditherGuider(double amount, bool raOnly, double settlePixels,
                  double settleTime, double settleTimeout) -> json {
    LOG_F(INFO, "ditherGuider: amount={} raOnly={}", amount, raOnly);
    return getGuiderService()->dither(...);
}
```

**Key Details:**
- Logs function entry with parameters for debugging
- Uses `LOG_F(INFO, ...)` consistently for all API commands
- Includes parameter values in log messages for traceability
- Helpful for tracking API usage and request details

</CodeSection>

<CodeSection>

## Code Section: Configuration Macros Using Loguru

**File:** `src/config/utils/macro.hpp`
**Lines:** 45-155
**Purpose:** Provides type-safe config access with integrated logging.

```cpp
template <ConfigurationType T>
[[nodiscard]] inline auto GetConfigValue(
    const std::shared_ptr<ConfigManager>& configManager,
    std::string_view path) -> T {
    if (!configManager) {
        LOG_F(ERROR, "Config manager is null");
        throw std::runtime_error("Config manager is null");
    }

    auto opt = configManager->get_as<T>(path);
    if (!opt.has_value()) {
        LOG_F(ERROR, "Config value for {} not found or wrong type", path);
        throw std::runtime_error(...);
    }
    return opt.value();
}

#define GET_CONFIG_VALUE(configManager, path, type, outputVar) \
    type outputVar; \
    do { \
        try { \
            auto opt = (configManager)->get_as<type>(path); \
            if (!opt.has_value()) { \
                LOG_F(ERROR, "Config value for {} not found or wrong type", path); \
                THROW_BAD_CONFIG_EXCEPTION(...); \
            } \
            outputVar = opt.value(); \
        } catch (const std::exception& e) { \
            LOG_F(ERROR, "Unknown exception when getting config value for {}: {}", path, e.what()); \
            throw; \
        } \
    } while (0)
```

**Key Details:**
- Integrates logging into config access functions
- Logs errors at ERROR level when config values are missing
- Provides detailed error messages with path and context
- Uses LOG_F for consistency with main logging system

</CodeSection>

<CodeSection>

## Code Section: ELF Debug Parser Logging

**File:** `src/components/debug/elf.cpp`
**Lines:** 20-238
**Purpose:** Demonstrates extensive logging in complex parsing operations.

```cpp
LOG_F(INFO, "ElfParser::Impl created for file: {}", file);
LOG_F(INFO, "Parsing ELF file: {}", filePath_);
LOG_F(ERROR, "Failed to open file: {}", filePath_);
LOG_F(INFO, "Successfully parsed ELF file: {}", filePath_);
LOG_F(ERROR, "Failed to parse ELF file: {}", filePath_);
LOG_F(INFO, "Getting ELF header");
LOG_F(INFO, "Finding symbol by name: {}", name);
LOG_F(INFO, "Found symbol: {}", name);
LOG_F(WARNING, "Symbol not found: {}", name);
LOG_F(ERROR, "Invalid ELF magic number");
LOG_F(ERROR, "File size too small for ELF header");
```

**Key Details:**
- Uses multiple log levels: INFO, WARNING, ERROR
- Logs at key decision points and error conditions
- Includes variable values in log messages
- Follows consistent naming pattern: `Operation: details`

</CodeSection>

### Log Macros Definition

<CodeSection>

## Code Section: Loguru Macro Definitions

**File:** `libs/atom/atom/log/loguru.hpp`
**Lines:** 1240-1390
**Purpose:** Defines all public logging macros used in codebase.

```cpp
// Standard logging macro
#define LOG_F(verbosity_name, ...) \
    VLOG_F(loguru::Verbosity_##verbosity_name, __VA_ARGS__)

// Conditional logging
#define LOG_IF_F(verbosity_name, cond, ...) \
    VLOG_IF_F(loguru::Verbosity_##verbosity_name, cond, __VA_ARGS__)

// Scope logging (RAII)
#define LOG_SCOPE_F(verbosity_name, ...) \
    VLOG_SCOPE_F(loguru::Verbosity_##verbosity_name, __VA_ARGS__)

#define LOG_SCOPE_FUNCTION(verbosity_name) LOG_SCOPE_F(verbosity_name, __func__)

// Raw logging (no preamble)
#define RAW_LOG_F(verbosity_name, ...) \
    RAW_VLOG_F(loguru::Verbosity_##verbosity_name, __VA_ARGS__)

// Debug conditional logging
#if LOGURU_DEBUG_LOGGING
#define DLOG_F(verbosity_name, ...) LOG_F(verbosity_name, __VA_ARGS__)
#define DVLOG_F(verbosity, ...) VLOG_F(verbosity, __VA_ARGS__)
#define DLOG_IF_F(verbosity_name, ...) LOG_IF_F(verbosity_name, __VA_ARGS__)
#else
#define DLOG_F(verbosity_name, ...)
#define DVLOG_F(verbosity, ...)
#define DLOG_IF_F(verbosity_name, ...)
#endif

// Assertion checks with abort
#define CHECK_F(test, ...) CHECK_WITH_INFO_F(test, #test, ##__VA_ARGS__)
#define CHECK_NOTNULL_F(x, ...) CHECK_WITH_INFO_F((x) != nullptr, #x " != nullptr", ##__VA_ARGS__)
#define CHECK_EQ_F(a, b, ...) CHECK_OP_F(a, b, ==, ##__VA_ARGS__)
#define CHECK_NE_F(a, b, ...) CHECK_OP_F(a, b, !=, ##__VA_ARGS__)
#define CHECK_LT_F(a, b, ...) CHECK_OP_F(a, b, <, ##__VA_ARGS__)
#define CHECK_GT_F(a, b, ...) CHECK_OP_F(a, b, >, ##__VA_ARGS__)
#define CHECK_LE_F(a, b, ...) CHECK_OP_F(a, b, <=, ##__VA_ARGS__)
#define CHECK_GE_F(a, b, ...) CHECK_OP_F(a, b, >=, ##__VA_ARGS__)

// Debug checks
#if LOGURU_DEBUG_CHECKS
#define DCHECK_F(test, ...) CHECK_F(test, ##__VA_ARGS__)
#define DCHECK_EQ_F(a, b, ...) CHECK_EQ_F(a, b, ##__VA_ARGS__)
#else
#define DCHECK_F(test, ...)
#define DCHECK_EQ_F(a, b, ...)
#endif
```

**Key Details:**
- Standard logging: `LOG_F(LEVEL, format, args...)`
- Verbosity names: FATAL, ERROR, WARNING, INFO, DEBUG, and custom levels
- Debug versions disabled in Release builds (NDEBUG)
- Assertion checks abort program execution on failure
- Error context support via `ERROR_CONTEXT(description, value)` macro

</CodeSection>

## Findings

### 1. Dual Logging Architecture

The project implements a **two-tier logging system**:

**Tier 1: Loguru (Primary)**
- Lightweight macro-based logging using `LOG_F()`, `DLOG_F()`, `CHECK_F()` macros
- Used throughout the entire codebase for daily operations
- Supports 10 verbosity levels (FATAL to DEBUG to custom levels)
- File-based output with configurable rotation and filters
- Initialized in `src/app.cpp` via `loguru::init(argc, argv)`

**Tier 2: AtomLog (Alternative)**
- More feature-rich class-based logger with template methods
- Provides sink-based architecture for multiple destinations
- Supports system logging integration (Windows Event Log, Linux syslog, Android)
- Thread-safe with background worker for asynchronous I/O
- Not actively used in main codebase but available in atom library

### 2. Log Initialization Flow

**Sequence:**
1. `loguru::init(argc, argv)` called in `main()` to parse command-line arguments
2. `setupLogFile()` creates `logs/` directory and sets up file output
3. Log filename format: `YYYYMMDD_HHMMSS.log` with timestamp precision
4. Fatal handler registered to save crash logs on abnormal termination
5. All subsequent log calls write to both console (filtered by verbosity) and file

### 3. Logging Macro Patterns

**Standard Usage:**
```cpp
LOG_F(INFO, "message with {} placeholders", value);    // Always logged
DLOG_F(INFO, "debug message: {}", value);              // Only in Debug builds
LOG_IF_F(ERROR, condition, "conditional: {}", value);  // Conditional logging
CHECK_F(condition != null, "assertion: {}", value);    // Abort on failure
```

**Log Levels (from Loguru):**
- `LOG_F(FATAL, ...)` - Fatal errors, triggers abort
- `LOG_F(ERROR, ...)` - Error conditions
- `LOG_F(WARNING, ...)` - Warning conditions
- `LOG_F(INFO, ...)` - Informational messages
- `LOG_F(DEBUG, ...)` - Debug information
- `VLOG_F(verbosity, ...)` - Custom verbosity level (1-9)

### 4. Current Logging Usage in Codebase

**Device Management:**
- Device addition/removal logged at INFO level
- Event callback errors logged at ERROR level
- Location: `src/device/manager.cpp`

**API Command Handlers:**
- Function entry logged with parameter values
- Used for tracing API calls and debugging
- Location: `src/server/command/guider.cpp`

**Configuration Access:**
- Missing or invalid config values logged at ERROR level
- Integrated into `GetConfigValue()` template function
- Location: `src/config/utils/macro.hpp`

**Debug Parsing (ELF):**
- Extensive INFO logging for step-by-step progress
- WARNING for non-fatal issues (e.g., symbol not found)
- ERROR for critical issues (e.g., invalid file format)
- Location: `src/components/debug/elf.cpp`

### 5. Log File Management

**Location:** `logs/` directory in working directory
**Format:** One log file per application run with timestamp
**Configuration:**
- Loguru configured for `Verbosity_MAX` file output
- All messages from FATAL to DEBUG are written to file
- Console output filtered by `-v` command-line flag
- Supports log rotation (configurable in AtomLog alternative)

### 6. Thread Safety and Performance

**AtomLog Implementation:**
- Uses `shared_mutex` for reader-writer synchronization
- Background `std::jthread` worker for asynchronous I/O
- Lock-free queue for log message buffering
- Prevents performance impact from I/O blocking

**Loguru Implementation:**
- Macro-based, minimal runtime overhead
- Verbosity-based filtering prevents unnecessary work
- Thread-safe file operations

### 7. Macro Configuration

**Conditional Compilation:**
- `LOGURU_DEBUG_LOGGING` controls DLOG_F macro expansion
- In Release mode (NDEBUG), DLOG_F expands to nothing
- In Debug mode, DLOG_F equivalent to LOG_F
- Reduces binary size and overhead in production

**Format Support:**
- Uses C++20 `std::format` for type-safe formatting
- Falls back to fmtlib for older compilers
- Supports all standard types automatically
- Custom types require `ec_to_text()` overload for error context

### 8. Error Context and Stack Traces

**ERROR_CONTEXT Macro:**
```cpp
ERROR_CONTEXT("description", variable_to_log);
```
- RAII-based, active during scope
- Captured and printed only on crash
- Useful for debugging complex state at failure point

**Stack Traces:**
- Loguru can generate stack traces on FATAL errors
- Platform-dependent (Linux/macOS/Windows)
- Requires debug symbols in binary

</CodeSection>
