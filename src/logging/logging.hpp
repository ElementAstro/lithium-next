/*
 * logging.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Main header for the logging module - includes all components

**************************************************/

#ifndef LITHIUM_LOGGING_LOGGING_HPP
#define LITHIUM_LOGGING_LOGGING_HPP

// Core types and utilities
#include "types.hpp"

// Sinks
#include "sinks/ring_buffer_sink.hpp"
#include "sinks/sink_factory.hpp"

// Logger registry
#include "logger_registry.hpp"

// Statistics and analysis
#include "log_statistics.hpp"

// Export utilities
#include "log_exporter.hpp"

// Main manager
#include "logging_manager.hpp"

#endif  // LITHIUM_LOGGING_LOGGING_HPP
