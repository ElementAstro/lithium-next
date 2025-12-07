/**
 * @file logging.hpp
 * @brief Main aggregated header for Lithium Logging library.
 *
 * This is the primary include file for the logging library.
 * Include this file to get access to all logging functionality.
 *
 * @par Usage Example:
 * @code
 * #include "logging/logging.hpp"
 *
 * using namespace lithium::logging;
 *
 * // Get logging manager instance
 * auto& manager = LoggingManager::instance();
 *
 * // Configure logging
 * manager.setGlobalLevel(spdlog::level::debug);
 *
 * // Get or create a logger
 * auto logger = manager.getLogger("my_logger");
 * logger->info("Hello, logging!");
 *
 * // Export logs
 * LogExporter exporter;
 * exporter.exportToFile(logs, "output.json", ExportOptions{});
 * @endcode
 *
 * @date 2024-11-28
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_LOGGING_LOGGING_HPP
#define LITHIUM_LOGGING_LOGGING_HPP

// ============================================================================
// Core Module
// ============================================================================
// Types, logging manager, and logger registry
//
// Components:
// - LogEntry: Log entry structure for buffered/streamed logs
// - LoggingManager: Central logging manager with spdlog integration
// - LoggerRegistry: Registry for managing named loggers

#include "core/core.hpp"

// ============================================================================
// Sinks Module
// ============================================================================
// Log sink implementations
//
// Components:
// - RingBufferSink: In-memory ring buffer sink for HTTP/WebSocket streaming
// - SinkFactory: Factory for creating various sink types

#include "sinks/sinks.hpp"

// ============================================================================
// Utils Module
// ============================================================================
// Logging utilities
//
// Components:
// - LogStatistics: Log statistics and analysis utilities
// - LogExporter: Log export utilities for various formats (JSON, CSV, HTML,
// etc.)

#include "utils/utils.hpp"

namespace lithium::logging {

/**
 * @brief Lithium Logging library version.
 */
inline constexpr const char* LOGGING_VERSION = "1.0.0";

/**
 * @brief Get logging library version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getLoggingVersion() noexcept {
    return LOGGING_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

// Core types (from core/core.hpp)
// - LogEntry, LoggingManager, LoggerRegistry

// Sink types (from sinks/sinks.hpp)
// - RingBufferSink, SinkFactory

// Utils types (from utils/utils.hpp)
// - LogStatistics, LogExporter, ExportFormat, ExportOptions

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_LOGGING_HPP
