# Exception Handling: Key Files Reference

## Summary

Quick reference for all exception-related files in the lithium-next project, organized by layer and function.

---

## Core Exception Infrastructure (atom library)

### Base Exception Class and Macros

**File:** `D:\Project\lithium-next\libs\atom\atom\error\exception.hpp`
- **Lines:** 15-548
- **Purpose:** Core exception class with source location support, 50+ specialized exception types, throwing macros
- **Key exports:**
  - `class Exception` - Base class with file/line/func/message capture
  - `class SystemErrorException` - Wraps system error codes
  - `class RuntimeError, LogicError, UnlawfulOperation` - Common exceptions
  - 50+ macro-based exceptions for specific scenarios
  - Macros: THROW_EXCEPTION, THROW_NESTED_EXCEPTION, THROW_SYSTEM_ERROR
  - THROW_* series: FILE_*, ARGUMENT_*, OBJECT_*, PROCESS_*, JSON_*, CURL_*

**Key classes:**
- Exception (base, 31-104)
- SystemErrorException (107-128)
- RuntimeError (150), LogicError (186), UnlawfulOperation (195)
- OutOfRange (204), OverflowException (213), UnderflowException (222), LengthException (231)
- ObjectAlreadyExist (253), ObjectNotExist (271), ObjectUninitialized (280)
- NullPointer (294), NotFound (303)
- WrongArgument (320), InvalidArgument (329), MissingArgument (338)
- FileNotFound (351), FileNotReadable (360), FileNotWritable (369)
- FailToOpenFile (378), FailToCloseFile (387), FailToCreateFile (396)
- FailToDeleteFile (405), FailToCopyFile (414), FailToMoveFile (423)
- FailToReadFile (432), FailToWriteFile (441)
- FailToLoadDll (454), FailToUnloadDll (463), FailToLoadSymbol (472)
- FailToCreateProcess (485), FailToTerminateProcess (494)
- JsonParseError (507), JsonValueError (516)
- CurlInitializationError (529), CurlRuntimeError (538)

**Integration:** Used throughout lithium codebase via THROW_* macros

---

**File:** `D:\Project\lithium-next\libs\atom\atom\error\exception.cpp`
- **Lines:** 1-56
- **Purpose:** Exception::what() implementation with optional stack trace generation
- **Key functionality:**
  - Formats detailed error message with file, line, function, thread ID
  - Integrates with three stack trace backends (cpptrace, boost::stacktrace, custom)
  - Lazy message generation - only on first what() call
  - Caches formatted message in mutable full_message_ field

**Compile-time selection:**
```cpp
#if ENABLE_CPPTRACE           // Fastest: cpptrace backend
#elif ENABLE_BOOST_STACKTRACE // Medium: boost::stacktrace
#else                         // Fallback: custom StackTrace
```

---

### Macro System

**File:** `D:\Project\lithium-next\libs\atom\atom\macro.hpp`
- **Lines:** 1-310
- **Purpose:** Compiler-independent macros for source location capture
- **Key macros:**
  - ATOM_FILE_NAME (line 301): Current file name
  - ATOM_FILE_LINE (line 300): Current line number
  - ATOM_FUNC_NAME (line 305): Current function name
  - ATOM_NOEXCEPT (line 231): noexcept keyword with C compatibility
  - ATOM_NODISCARD (line 106): [[nodiscard]] or compiler equivalent
  - ATOM_META_FUNCTION_NAME (lines 51-61): Compiler-specific function signature
  - ATOM_ASSUME, ATOM_INLINE, ATOM_FORCEINLINE, etc.

**Source location selection (lines 299-310):**
- Prefers C++20 std::source_location when available (__cpp_lib_source_location)
- Falls back to compiler builtins (__FILE__, __LINE__, __func__)

**Integration:** Every THROW_* macro uses these to capture location

---

### Error Codes

**File:** `D:\Project\lithium-next\libs\atom\atom\error\error_code.hpp`
- **Lines:** 1-195
- **Purpose:** Centralized error code enumeration for all domains
- **Error code ranges:**
  - 0-99: Reserved (Success=0, Failed=1, Cancelled=2)
  - 100-199: FileError (NotFound=100, OpenError=101, etc.)
  - 200-299: DeviceError (NotFound=201, NotConnected=203, etc.)
    - 210-219: Camera-specific (ExposureError=210, GainError=211, CoolingError=214)
    - 220-229: Mount-specific (GotoError=220, ParkError=221)
    - 230-299: General device (InitializationError=230, CalibrationError=233)
  - 300-399: ServerError (InvalidParameters=300, UnknownDevice=312)
  - 400-499: NetworkError (ConnectionLost=400, TimeoutError=411)
  - 500-599: DatabaseError (ConnectionFailed=500, QueryFailed=501)
  - 600-699: MemoryError (AllocationFailed=600, OutOfMemory=601)
  - 700-799: UserInputError (InvalidInput=700, OutOfRange=701)
  - 800-899: ConfigError (MissingConfig=800, InvalidConfig=801)
  - 900-999: ProcessError (ProcessNotFound=900, ThreadCreationFailed=902)

**Note:** Enums defined but not all have corresponding exception classes

---

### Error Stack Tracking

**File:** `D:\Project\lithium-next\libs\atom\atom\utils\error_stack.hpp`
- **Lines:** 1-116
- **Purpose:** Historical tracking and aggregation of errors
- **Key classes:**
  - `struct ErrorInfo` (28-36): Individual error with timestamp and UUID
  - `class ErrorStack` (57-113): Maintains error history with filtering

**Features:**
- `insertError()`: Add error to stack
- `getCompressedErrors()`: Get deduplicated error list
- `getFilteredErrorsByModule()`: Filter by module name
- `printFilteredErrorStack()`: Print to stdout
- Thread-safe unique pointers: createShared(), createUnique()

**Use case:** Application-wide error audit trail

---

**File:** `D:\Project\lithium-next\libs\atom\atom\utils\error_stack.cpp`
- **Lines:** Implementation of ErrorStack operations
- **Key function:** updateCompressedErrors() deduplication logic

---

## Enhanced Exception System (lithium namespace)

### C++23 Enhanced Exceptions

**File:** `D:\Project\lithium-next\src\exception\exception.hpp`
- **Lines:** 1-469
- **Purpose:** Modern exception system with severity, category, context, and JSON serialization
- **Key classes:**

**ErrorSeverity enum (35-43):** TRACE, DEBUG, INFO, WARNING, ERROR, CRITICAL, FATAL

**ErrorCategory enum (46-60):**
- UNKNOWN=0, SYSTEM=100, NETWORK=200, DATABASE=300, FILESYSTEM=400
- MEMORY=500, COMPONENT=600, SERVER=700, DEBUG=800, SECURITY=900
- CONFIGURATION=1000, USER_INPUT=1100, EXTERNAL_SERVICE=1200

**ErrorContext struct (63-91):**
- operation, module, function: String identifiers
- metadata: JSON for custom data
- timestamp: steady_clock time point
- threadId: Thread that created context
- toJson(): Serializes to JSON

**EnhancedException class (94-264):**
- Extends atom::error::Exception
- Adds severity_, category_, errorCode_, context_, stackTrace_, tags_, innerException_
- Supports exception chaining via setInnerException()
- toJson(): Full serialization with nested exception handling
- toString(): JSON pretty-printed string representation

**Specialized exception types (266-320):**
- SystemException (267-276)
- NetworkException (278-287)
- ComponentException (289-298)
- ServerException (300-309)
- DebugException (311-320)

**ErrorHandler class (336-429):**
- `safeExecute()` (348-374): Wraps function in try-catch, returns Result<T>
- `handleAsync()` (377-385): Extracts value from Result or returns nullopt
- `ErrorCollector` (388-428): Accumulates errors from multiple operations

**Result type (322-326):**
- `Result<T>`: atom::type::expected<T, EnhancedException>
- `VoidResult`: Result<void>

**Helper concepts (329-333):**
- `Exception`: Concept for std::exception derivatives
- `EnhancedExceptionType`: Concept for EnhancedException derivatives

**RAII Context Manager (431-450):**
- `ErrorContextScope`: RAII wrapper for error context
- thread_local static contextStack_: Per-thread context stack
- Constructor: Pushes context
- Destructor: Pops context
- `getCurrentContext()`: Returns top of stack

**Convenience macros (452-465):**
- ENHANCED_TRY_CATCH(operation): Wraps operation in safeExecute
- ENHANCED_THROW(ExceptionType, code, message, ...): Throws with current context
- ENHANCED_CONTEXT(operation, module, function): Creates scope with context
- ENHANCED_CONTEXT_WITH_META(operation, module, function, metadata): With metadata

**Integration:** Used in complex subsystems (device management, dependency resolution)

---

## Domain-Specific Exceptions

### Configuration Exceptions

**File:** `D:\Project\lithium-next\src\config\core\exception.hpp`
- **Lines:** 1-72
- **Purpose:** Configuration-specific exception hierarchy
- **Key classes:**
  - `BadConfigException` (25-27): Base for config errors
  - `InvalidConfigException` (36-38): Invalid configuration values
  - `ConfigNotFoundException` (47-49): Configuration not found
  - `ConfigIOException` (58-60): File I/O errors
- **Macros:**
  - THROW_BAD_CONFIG_EXCEPTION (29-31)
  - THROW_INVALID_CONFIG_EXCEPTION (40-42)
  - THROW_CONFIG_NOT_FOUND_EXCEPTION (51-53)
  - THROW_CONFIG_IO_EXCEPTION (62-64)
- **Backward compatibility:** Aliases in global namespace (68-70)

**Integration:** Config loading and validation operations

---

### Device Management Exceptions

**File:** `D:\Project\lithium-next\src\device\manager.hpp`
- **Lines:** 14-31
- **Purpose:** Device operation exception types
- **Key classes:**
  - `DeviceNotFoundException` (17-19): Device not found in registry
  - `DeviceTypeNotFoundException` (25-27): Device type not found
- **Macros:**
  - THROW_DEVICE_NOT_FOUND (21-23)
  - THROW_DEVICE_TYPE_NOT_FOUND (29-31)

**Integration:** Device manager setPrimaryDevice(), getDevice(), etc.

---

### Component Loading Exceptions

**File:** `D:\Project\lithium-next\src\components\manager\exceptions.hpp`
- **Lines:** 1-30
- **Purpose:** Dynamic component/plugin loading errors
- **Key classes:**
  - `FailToLoadComponent` (21-24): Component loading failed
- **Macros:**
  - THROW_FAIL_TO_LOAD_COMPONENT (26-28)

**Integration:** Component manager, module loading

---

### Dependency Management Exceptions

**File:** `D:\Project\lithium-next\src\components\system\dependency_exception.hpp`
- **Lines:** 1-178
- **Purpose:** Package manager and dependency resolution errors
- **Key enums:**
  - `DependencyErrorCode` (18-29): Typed error codes
    - SUCCESS=0, PACKAGE_MANAGER_NOT_FOUND, INSTALL_FAILED
    - UNINSTALL_FAILED, DEPENDENCY_NOT_FOUND, CONFIG_LOAD_FAILED
    - INVALID_VERSION, NETWORK_ERROR, PERMISSION_DENIED, UNKNOWN_ERROR

**Key classes:**
  - `DependencyException` (39-91): Exception for throwing
    - Extends lithium::exception::ComponentException
    - Stores DependencyErrorCode
    - Supports exception chaining via constructor overload

  - `DependencyError` (98-149): Error struct for Result pattern
    - code(): DependencyErrorCode
    - message(): String message
    - context(): ErrorContext
    - toJson(): JSON serialization

**Result types:**
  - `DependencyResult<T>` (159-162): Optional<T> + Optional<DependencyError>
  - `DependencyResult<void>` (170-174): bool success + Optional<DependencyError>

**Integration:** Package manager operations, dependency resolution

---

### PHD2 Client Exceptions (C++20 style)

**File:** `D:\Project\lithium-next\src\client\phd2\exceptions.hpp`
- **Lines:** 1-171
- **Purpose:** PHD2 guider client exception hierarchy
- **Note:** Uses std::runtime_error base, not atom::error::Exception (different approach)
- **Key classes:**
  - `PHD2Exception` (28-45): Base exception
    - Derives from std::runtime_error
    - Uses std::source_location directly (C++20)
    - Stores location_ member
    - Uses std::format for message construction

  - `ConnectionException` (50-56): Connection failures
  - `RpcException` (61-75): JSON-RPC errors with error code
  - `TimeoutException` (80-94): Operation timeout with ms
  - `InvalidStateException` (99-118): Invalid operation in current state with state string
  - `ParseException` (123-138): Parsing errors with input
  - `EquipmentNotConnectedException` (143-157): Equipment not connected with equipment name
  - `CalibrationException` (162-168): Calibration errors

**Design rationale:** Independent from atom system for PHD2 third-party integration

**Integration:** PHD2 client connection, RPC operations, state machine

---

## Implementation Examples

### Device Manager Usage

**File:** `D:\Project\lithium-next\src\device\manager.cpp`
- **Exception throwing:** Lines 120-124 (setPrimaryDevice)
  - THROW_DEVICE_NOT_FOUND
  - THROW_DEVICE_TYPE_NOT_FOUND

- **Try-catch patterns:**
  - Event callback protection (45-55): Catch std::exception, log, continue
  - Connection loop (138-154): Catch DeviceNotFoundException, update state, emit event, continue

**Integration:** Shows real-world exception handling patterns

---

### INDI Client Usage

**File:** `D:\Project\lithium-next\src\client\indi\indi_client.cpp`
- **Exception throwing patterns:**
  - THROW_FILE_NOT_FOUND (collection.cpp)
  - THROW_RUNTIME_ERROR (iconnector.cpp)
  - THROW_FAIL_TO_OPEN_FILE (profile.cpp)

- **Try-catch patterns:**
  - Network I/O (281-303): Catch (...) for unexpected socket errors
  - Standard setup (422-426): Catch std::exception, log, re-throw

**Integration:** Shows exception handling in network operations

---

### Dependency Resolution Usage

**File:** `D:\Project\lithium-next\src/components/dependency.cpp`
- **Exception throwing:**
  - THROW_INVALID_ARGUMENT: Node validation, circular dependency detection
  - THROW_RUNTIME_ERROR: Circular dependency detected, parallel resolution error
  - THROW_FAIL_TO_OPEN_FILE: Configuration file access
  - THROW_JSON_PARSE_ERROR: JSON parsing
  - THROW_MISSING_ARGUMENT: Required fields
  - THROW_INVALID_ARGUMENT: Version parsing

**Integration:** Shows comprehensive use of exception system across operations

---

## Summary Table

| Layer | File | Purpose | Key Classes |
|-------|------|---------|-------------|
| **Core** | exception.hpp | Base exception with macros | Exception, 50+ types |
| | exception.cpp | what() implementation | Stack trace generation |
| | macro.hpp | Source location macros | ATOM_FILE_*, ATOM_FUNC_NAME |
| | error_code.hpp | Error code enumerations | FileError, DeviceError, etc. |
| | error_stack.hpp | Error history tracking | ErrorStack, ErrorInfo |
| **Enhanced** | exception.hpp (lithium) | Modern C++23 exceptions | EnhancedException, ErrorContext |
| **Domain** | config/exception.hpp | Config errors | BadConfigException, variants |
| | device/manager.hpp | Device errors | DeviceNotFoundException, variants |
| | components/exceptions.hpp | Component loading | FailToLoadComponent |
| | system/dependency_exception.hpp | Dependency errors | DependencyException, DependencyError |
| | client/phd2/exceptions.hpp | PHD2 client errors | PHD2Exception, variants |

---

## Total Exception Class Count

- **Base exceptions:** 50+ (atom library)
- **Enhanced exceptions:** 4 specialized types
- **Configuration exceptions:** 4 (BadConfig, InvalidConfig, NotFound, IO)
- **Device exceptions:** 2 (NotFound, TypeNotFound)
- **Component exceptions:** 1 (FailToLoad)
- **Dependency exceptions:** 2 (Exception, Error struct)
- **PHD2 exceptions:** 6 (Base + 5 specialized)
- **Total:** ~70 exception types in codebase

## Total Throwing Macro Count

- **Base THROW_ macros:** 50+ (one per exception class)
- **Specialized macros:** THROW_NESTED_EXCEPTION, THROW_SYSTEM_ERROR
- **Config macros:** 4 (THROW_*_CONFIG_EXCEPTION)
- **Device macros:** 2 (THROW_DEVICE_*_NOT_FOUND)
- **Component macros:** 1 (THROW_FAIL_TO_LOAD_COMPONENT)
- **Enhanced macros:** 4 (ENHANCED_CONTEXT, ENHANCED_THROW, etc.)
- **Total:** ~70 throwing macros available

---

## Compilation Requirements

- **C++ Standard:** C++20 minimum (std::source_location)
- **Compiler support:**
  - GCC 13+
  - Clang 16+
  - MSVC 19.28+ (Visual Studio 2019 or later)
- **Stack trace backends (optional):**
  - cpptrace (preferred, compile-time enabled)
  - boost::stacktrace (portable)
  - Custom implementation (fallback)
