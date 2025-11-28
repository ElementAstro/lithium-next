# Logging System: Key Files and Usage Patterns

## Evidence

### Core Logging Implementation Files

<CodeSection>

## Code Section: Core Library Structure

**Location:** `libs/atom/atom/log/`

The atom library contains three primary logging components:

1. **atomlog.hpp/cpp** - Class-based logger
   - File: `libs/atom/atom/log/atomlog.hpp` (205 lines)
   - File: `libs/atom/atom/log/atomlog.cpp` (433 lines)
   - Purpose: Feature-rich Logger class with C++20 templates

2. **logger.hpp/cpp** - Log management utility
   - File: `libs/atom/atom/log/logger.hpp` (84 lines)
   - File: `libs/atom/atom/log/logger.cpp` (201 lines)
   - Purpose: LoggerManager for log file scanning, analysis, and upload

3. **loguru.hpp/cpp** - Macro-based logging library
   - File: `libs/atom/atom/log/loguru.hpp` (1390 lines)
   - File: `libs/atom/atom/log/loguru.cpp` (1000+ lines)
   - Purpose: Primary macro-based logging used throughout project

**Key Details:**
- All three components coexist in the atom library
- Loguru is the active/primary logging system
- AtomLog provides alternative class-based interface
- LoggerManager provides post-processing utilities

</CodeSection>

<CodeSection>

## Code Section: Application Entry Point Setup

**File:** `src/app.cpp`
**Lines:** 56-74, 142

```cpp
// Function to set up logging file output
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

// In main() function:
loguru::init(argc, argv);
```

**Key Details:**
- `setupLogFile()` creates logs directory and registers file output
- Log files use timestamp naming: `YYYYMMDD_HHMMSS.log`
- `loguru::add_file()` registers file output at `Verbosity_MAX` (all levels)
- Fatal handler saves crash logs via `atom::system::saveCrashLog()`
- `loguru::init()` parses `-v` verbosity argument and sets up signal handlers

</CodeSection>

### Log Macro Definition Locations

<CodeSection>

## Code Section: Macro Categories in loguru.hpp

**File:** `libs/atom/atom/log/loguru.hpp`

**Category 1: Standard Logging Macros (Lines 1240-1276)**
```cpp
#define LOG_F(verbosity_name, ...) \
    VLOG_F(loguru::Verbosity_##verbosity_name, __VA_ARGS__)

#define VLOG_F(verbosity, ...) \
    ((verbosity) > loguru::current_verbosity_cutoff()) \
        ? (void)0 \
        : loguru::log(verbosity, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_IF_F(verbosity_name, cond, ...) \
    VLOG_IF_F(loguru::Verbosity_##verbosity_name, cond, __VA_ARGS__)

#define LOG_SCOPE_F(verbosity_name, ...) \
    VLOG_SCOPE_F(loguru::Verbosity_##verbosity_name, __VA_ARGS__)

#define LOG_SCOPE_FUNCTION(verbosity_name) \
    LOG_SCOPE_F(verbosity_name, __func__)
```

**Category 2: Raw Logging Macros (Lines 1264-1276)**
```cpp
#define RAW_LOG_F(verbosity_name, ...) \
    RAW_VLOG_F(loguru::Verbosity_##verbosity_name, __VA_ARGS__)

#define RAW_VLOG_F(verbosity, ...) \
    ((verbosity) > loguru::current_verbosity_cutoff()) \
        ? (void)0 \
        : loguru::raw_log(verbosity, __FILE__, __LINE__, __VA_ARGS__)
```

**Category 3: Debug Macros (Lines 1325-1341)**
```cpp
#if LOGURU_DEBUG_LOGGING
#define DLOG_F(verbosity_name, ...) LOG_F(verbosity_name, __VA_ARGS__)
#define DVLOG_F(verbosity, ...) VLOG_F(verbosity, __VA_ARGS__)
#define DLOG_IF_F(verbosity_name, ...) LOG_IF_F(verbosity_name, __VA_ARGS__)
#define DVLOG_IF_F(verbosity, ...) VLOG_IF_F(verbosity, __VA_ARGS__)
#define DRAW_LOG_F(verbosity_name, ...) RAW_LOG_F(verbosity_name, __VA_ARGS__)
#define DRAW_VLOG_F(verbosity, ...) RAW_VLOG_F(verbosity, __VA_ARGS__)
#else
#define DLOG_F(verbosity_name, ...)
#define DVLOG_F(verbosity, ...)
#define DLOG_IF_F(verbosity_name, ...)
#define DVLOG_IF_F(verbosity, ...)
#define DRAW_LOG_F(verbosity_name, ...)
#define DRAW_VLOG_F(verbosity, ...)
#endif
```

**Category 4: Assertion Macros (Lines 1288-1315)**
```cpp
#define CHECK_F(test, ...) \
    CHECK_WITH_INFO_F(test, #test, ##__VA_ARGS__)

#define CHECK_NOTNULL_F(x, ...) \
    CHECK_WITH_INFO_F((x) != nullptr, #x " != nullptr", ##__VA_ARGS__)

#define CHECK_EQ_F(a, b, ...) CHECK_OP_F(a, b, ==, ##__VA_ARGS__)
#define CHECK_NE_F(a, b, ...) CHECK_OP_F(a, b, !=, ##__VA_ARGS__)
#define CHECK_LT_F(a, b, ...) CHECK_OP_F(a, b, <, ##__VA_ARGS__)
#define CHECK_GT_F(a, b, ...) CHECK_OP_F(a, b, >, ##__VA_ARGS__)
#define CHECK_LE_F(a, b, ...) CHECK_OP_F(a, b, <=, ##__VA_ARGS__)
#define CHECK_GE_F(a, b, ...) CHECK_OP_F(a, b, >=, ##__VA_ARGS__)
```

**Category 5: Debug Assertion Macros (Lines 1358-1378)**
```cpp
#if LOGURU_DEBUG_CHECKS
#define DCHECK_F(test, ...) CHECK_F(test, ##__VA_ARGS__)
#define DCHECK_NOTNULL_F(x, ...) CHECK_NOTNULL_F(x, ##__VA_ARGS__)
#define DCHECK_EQ_F(a, b, ...) CHECK_EQ_F(a, b, ##__VA_ARGS__)
#define DCHECK_NE_F(a, b, ...) CHECK_NE_F(a, b, ##__VA_ARGS__)
#define DCHECK_LT_F(a, b, ...) CHECK_LT_F(a, b, ##__VA_ARGS__)
#define DCHECK_LE_F(a, b, ...) CHECK_LE_F(a, b, ##__VA_ARGS__)
#define DCHECK_GT_F(a, b, ...) CHECK_GT_F(a, b, ##__VA_ARGS__)
#define DCHECK_GE_F(a, b, ...) CHECK_GE_F(a, b, ##__VA_ARGS__)
#else
#define DCHECK_F(test, ...)
#define DCHECK_EQ_F(a, b, ...)
// ... all expand to nothing
#endif
```

**Key Details:**
- All standard macros use VLOG_F internally with expanded verbosity name
- Supports conditional compilation for Debug/Release builds
- DEBUG macros (DLOG_F, DCHECK_F) disappear in Release mode
- CHECK macros abort program on failure (assertion-like behavior)

</CodeSection>

### Typical Usage Examples in Codebase

<CodeSection>

## Code Section: Device Service Logging Pattern

**File:** `src/device/manager.cpp`
**Pattern:** INFO level for normal operations, ERROR for exceptions

```cpp
LOG_F(INFO, "Added device {} of type {}", device->getName(), type);
LOG_F(INFO, "Primary device for {} set to {}", type, device->getName());
LOG_F(ERROR, "DeviceManager: Event callback error: %s", e.what());
```

**File:** `src/device/service/camera_service.cpp`
**Pattern:** Uses configuration utility with integrated logging

**File:** `src/device/service/mount_service.cpp`
**Pattern:** Operation-specific logging with parameter values

</CodeSection>

<CodeSection>

## Code Section: API Handler Logging Pattern

**File:** `src/server/command/guider.cpp`
**Lines:** 29-75
**Pattern:** Function entry logging with all parameters

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

auto loopGuider() -> json {
    LOG_F(INFO, "loopGuider");
    return getGuiderService()->loop();
}
```

**File:** `src/server/command/telescope.cpp`
**Pattern:** Similar function entry logging

**File:** `src/server/command/image.hpp`
**Pattern:** Command implementation logging

**Key Details:**
- Logs function name and all parameters at INFO level
- Helps trace API calls and their arguments
- Useful for debugging request handling

</CodeSection>

<CodeSection>

## Code Section: Configuration Access Logging

**File:** `src/config/utils/macro.hpp`
**Lines:** 44-135
**Pattern:** Error logging in utility functions

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
            LOG_F(ERROR, "Unknown exception when getting config value for {}: {}", \
                  path, e.what()); \
            throw; \
        } \
    } while (0)
```

**Key Details:**
- ERROR level for configuration problems
- Includes path and type information in error messages
- Throws exceptions after logging error
- Pattern used in all config-related code

</CodeSection>

<CodeSection>

## Code Section: Debug/Parsing Logging Pattern

**File:** `src/components/debug/elf.cpp`
**Lines:** 20-292
**Pattern:** Detailed INFO/WARNING/ERROR logging for complex operations

```cpp
LOG_F(INFO, "ElfParser::Impl created for file: {}", file);
LOG_F(INFO, "Parsing ELF file: {}", filePath_);

if (!file_stream.good()) {
    LOG_F(ERROR, "Failed to open file: {}", filePath_);
    return false;
}

if (parseElfHeader() && parseProgramHeaders() && parseSectionHeaders()) {
    LOG_F(INFO, "Successfully parsed ELF file: {}", filePath_);
} else {
    LOG_F(ERROR, "Failed to parse ELF file: {}", filePath_);
}

auto symbol = findSymbolByName(name);
if (symbol) {
    LOG_F(INFO, "Found symbol: {}", name);
} else {
    LOG_F(WARNING, "Symbol not found: {}", name);
}

if (magic != ELFMAG) {
    LOG_F(ERROR, "Invalid ELF magic number");
    return false;
}

if (fileSize < sizeof(ElfHeader)) {
    LOG_F(ERROR, "File size too small for ELF header");
    return false;
}
```

**Key Details:**
- Detailed step-by-step logging during complex operations
- INFO level for normal progress
- WARNING level for non-critical issues (e.g., symbol not found)
- ERROR level for critical failures (e.g., invalid format)
- Helps trace execution path through parsing logic

</CodeSection>

<CodeSection>

## Code Section: Logger Manager Usage

**File:** `libs/atom/atom/log/logger.cpp`
**Lines:** 99-159
**Pattern:** Post-processing of log files

```cpp
void LoggerManager::Impl::uploadFile(const std::string &filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        LOG_F(ERROR, "Failed to open file: {}", filePath);
        return;
    }

    std::string content(...);
    std::string encryptedContent = encryptFileContent(content);

    atom::web::CurlWrapper curl;
    curl.setUrl("https://lightapt.com/upload");
    curl.setRequestMethod("POST");
    curl.addHeader("Content-Type", "application/octet-stream");
    curl.setRequestBody(encryptedContent);

    curl.setOnErrorCallback([](CURLcode error) {
        LOG_F(ERROR, "Failed to upload file: curl error code {}",
              static_cast<int>(error));
    });

    curl.setOnResponseCallback([](const std::string &response) {
        DLOG_F(INFO, "File uploaded successfully. Server response: {}", response);
    });

    curl.perform();
}

void LoggerManager::Impl::analyzeLogs() {
    auto errorMessages = extractErrorMessages();

    if (errorMessages.empty()) {
        DLOG_F(INFO, "No errors found in the logs.");
        return;
    }
    DLOG_F(INFO, "Analyzing logs...");

    std::map<std::string, int> errorTypeCount;
    for (const auto &errorMessage : errorMessages) {
        std::string errorType = getErrorType(errorMessage);
        errorTypeCount[errorType]++;
    }

    DLOG_F(INFO, "Error Type Count:");
    for (const auto &[errorType, count] : errorTypeCount) {
        DLOG_F(INFO, "{} : {}", errorType, count);
    }

    std::string mostCommonErrorMessage = getMostCommonErrorMessage(errorMessages);
    DLOG_F(INFO, "Most Common Error Message: {}", mostCommonErrorMessage);
}
```

**Key Details:**
- Uses LOG_F for critical errors (file open failures)
- Uses DLOG_F for informational debug messages (only in Debug build)
- Useful for log analysis and post-mortem debugging

</CodeSection>

## Findings

### 1. Complete File Reference

**Core Logging Files:**

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| `libs/atom/atom/log/loguru.hpp` | 1,390 | Macro-based logging interface | Active/Primary |
| `libs/atom/atom/log/loguru.cpp` | 1,000+ | Loguru implementation | Active/Primary |
| `libs/atom/atom/log/atomlog.hpp` | 205 | Class-based logger interface | Available |
| `libs/atom/atom/log/atomlog.cpp` | 433 | AtomLog implementation | Available |
| `libs/atom/atom/log/logger.hpp` | 84 | Log management utility | Available |
| `libs/atom/atom/log/logger.cpp` | 201 | Log analysis/upload implementation | Available |

**Integration Files:**

| File | Purpose |
|------|---------|
| `src/app.cpp` | Loguru initialization and file setup |
| `src/config/utils/macro.hpp` | Configuration access with logging |
| `src/constant/constant.hpp` | Global constants referenced in logging |

### 2. Macro Family Summary

**Standard Logging:**
- `LOG_F(LEVEL, format, args...)` - Standard logging at specified level
- `VLOG_F(verbosity_num, format, args...)` - Log at numeric verbosity level

**Conditional Logging:**
- `LOG_IF_F(LEVEL, condition, format, args...)` - Only logs if condition is true
- `VLOG_IF_F(verbosity_num, condition, format, args...)` - Numeric verbosity version

**Scope Logging:**
- `LOG_SCOPE_F(LEVEL, format, args...)` - RAII-based scope timing
- `LOG_SCOPE_FUNCTION(LEVEL)` - Shorthand for function entry/exit

**Raw Logging:**
- `RAW_LOG_F(LEVEL, format, args...)` - No preamble, no indentation
- `RAW_VLOG_F(verbosity_num, format, args...)` - Numeric verbosity version

**Debug Versions (disappear in Release):**
- `DLOG_F(LEVEL, format, args...)` - Debug-only logging
- `DVLOG_F(verbosity_num, format, args...)` - Debug verbosity version
- `DLOG_IF_F(LEVEL, condition, format, args...)` - Debug conditional

**Assertions (abort on failure):**
- `CHECK_F(condition, format, args...)` - Assert condition is true
- `CHECK_NOTNULL_F(ptr, format, args...)` - Assert pointer is not null
- `CHECK_EQ_F(a, b, format, args...)` - Assert equal
- `CHECK_NE_F(a, b, format, args...)` - Assert not equal
- `CHECK_LT_F(a, b, format, args...)` - Assert less than
- `CHECK_GT_F(a, b, format, args...)` - Assert greater than
- `CHECK_LE_F(a, b, format, args...)` - Assert less than or equal
- `CHECK_GE_F(a, b, format, args...)` - Assert greater than or equal

**Debug Assertions (only in Debug build):**
- `DCHECK_F(condition, format, args...)` - Debug-only assert
- `DCHECK_EQ_F(a, b, format, args...)` - Debug equality check
- (and other comparison variants)

### 3. Verbosity Levels

From highest to lowest severity:
- `FATAL` (-3) - Triggers program abort
- `ERROR` (-2) - Error conditions
- `WARNING` (-1) - Warning conditions
- `INFO` (0) - Informational messages (default for stderr)
- `DEBUG` (+1) - Debug level 1
- `2` through `9` - Custom verbosity levels (higher = more verbose)
- `MAX` (+9) - Capture all verbosity levels

### 4. Typical Usage Patterns

**Pattern 1: Function Entry with Parameters**
```cpp
LOG_F(INFO, "functionName: param1={} param2={}", param1, param2);
```
Used in API handlers, service methods for request tracing.

**Pattern 2: Error Handling**
```cpp
LOG_F(ERROR, "Operation failed: {}", error_description);
throw SomeException(...);
```
Used after logging error details before throwing.

**Pattern 3: Conditional Operations**
```cpp
if (condition) {
    LOG_F(WARNING, "Unexpected condition: {}", details);
}
```
Used for non-critical issues that don't abort execution.

**Pattern 4: Step-by-step Progress**
```cpp
LOG_F(INFO, "Step 1: {}...", step1_desc);
// do step 1
LOG_F(INFO, "Step 2: {}...", step2_desc);
// do step 2
```
Used in complex parsing/processing operations.

**Pattern 5: Debug-only Information**
```cpp
DLOG_F(INFO, "Internal state: {}", internal_value);
```
Used for detailed debugging info, removed in Release build.

**Pattern 6: Assertions**
```cpp
CHECK_F(pointer != nullptr, "Pointer must not be null");
CHECK_EQ_F(expected, actual, "Values must match");
```
Used for invariant checking, aborts on violation.

### 5. Log Output Configuration

**File Output:**
- Location: `logs/` directory (relative to working directory)
- Name format: `YYYYMMDD_HHMMSS.log`
- One file per application run
- All verbosity levels written (VERBOSITY_MAX)

**Console Output:**
- Default: ERROR, WARNING, INFO (filtered by -v flag)
- Debug messages suppressed unless `-v 1` or higher specified
- Command-line: `./lithium-next -v DEBUG` or `-v 3`

**File vs Console:**
- File captures everything (all verbosity levels)
- Console respects verbosity filter (`-v` flag)
- Independent filtering for dual output

### 6. Integration Points

**During Initialization:**
1. `loguru::init(argc, argv)` - Parse verbosity flag, set up handlers
2. `setupLogFile()` - Create logs directory, register file output
3. `loguru::set_fatal_handler()` - Save crash logs on FATAL

**During Runtime:**
- Every function/operation logs key points
- Configuration access logs errors
- Exception handling logs error details
- API calls log input parameters

**On Exit:**
- Fatal errors trigger handler and save crash log
- File is automatically flushed and closed
- All pending log messages written before exit

### 7. Conditional Compilation Features

**LOGURU_DEBUG_LOGGING:**
- Controls whether DLOG_F expands to LOG_F (Debug) or nothing (Release)
- Automatically set based on NDEBUG macro
- Reduces Release binary size by removing debug statements

**LOGURU_DEBUG_CHECKS:**
- Controls whether DCHECK_F expands to CHECK_F (Debug) or nothing (Release)
- Removes debug assertions from Release builds
- Improves Release performance by avoiding assertion overhead

</CodeSection>
