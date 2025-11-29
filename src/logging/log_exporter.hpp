/*
 * log_exporter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Log export utilities for various formats

**************************************************/

#ifndef LITHIUM_LOGGING_LOG_EXPORTER_HPP
#define LITHIUM_LOGGING_LOG_EXPORTER_HPP

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "types.hpp"

namespace lithium::logging {

/**
 * @brief Export format enumeration
 */
enum class ExportFormat {
    JSON,   // JSON array of log entries
    JSONL,  // JSON Lines (one JSON object per line)
    CSV,    // Comma-separated values
    TEXT,   // Plain text format
    HTML    // HTML table format
};

/**
 * @brief Export options
 */
struct ExportOptions {
    ExportFormat format{ExportFormat::JSON};
    bool include_timestamp{true};
    bool include_level{true};
    bool include_logger{true};
    bool include_thread_id{false};
    bool include_source{false};
    std::string timestamp_format{"%Y-%m-%d %H:%M:%S"};
    std::string csv_delimiter{","};
    bool csv_include_header{true};
    bool pretty_print{false};

    [[nodiscard]] static auto fromJson(const nlohmann::json& j)
        -> ExportOptions;
    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Export result
 */
struct ExportResult {
    bool success{false};
    std::string content;
    std::string file_path;
    size_t entry_count{0};
    size_t byte_count{0};
    std::string error_message;

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Log exporter for various formats
 *
 * Supports exporting log entries to:
 * - JSON (array format)
 * - JSONL (JSON Lines)
 * - CSV
 * - Plain text
 * - HTML table
 */
class LogExporter {
public:
    /**
     * @brief Export log entries to string
     * @param entries Log entries to export
     * @param options Export options
     * @return Export result with content
     */
    [[nodiscard]] static auto exportToString(
        const std::vector<LogEntry>& entries, const ExportOptions& options = {})
        -> ExportResult;

    /**
     * @brief Export log entries to file
     * @param entries Log entries to export
     * @param file_path Output file path
     * @param options Export options
     * @return Export result
     */
    [[nodiscard]] static auto exportToFile(
        const std::vector<LogEntry>& entries,
        const std::filesystem::path& file_path,
        const ExportOptions& options = {}) -> ExportResult;

    /**
     * @brief Export log entries with streaming callback
     * @param entries Log entries to export
     * @param options Export options
     * @param callback Called for each chunk of output
     * @return Export result
     */
    [[nodiscard]] static auto exportStreaming(
        const std::vector<LogEntry>& entries, const ExportOptions& options,
        std::function<void(const std::string&)> callback) -> ExportResult;

    /**
     * @brief Get file extension for format
     * @param format Export format
     * @return File extension (e.g., ".json")
     */
    [[nodiscard]] static auto getFileExtension(ExportFormat format)
        -> std::string;

    /**
     * @brief Get MIME type for format
     * @param format Export format
     * @return MIME type string
     */
    [[nodiscard]] static auto getMimeType(ExportFormat format) -> std::string;

    /**
     * @brief Parse format from string
     * @param format_str Format string (e.g., "json", "csv")
     * @return Export format enum
     */
    [[nodiscard]] static auto parseFormat(const std::string& format_str)
        -> ExportFormat;

private:
    static auto exportToJson(const std::vector<LogEntry>& entries,
                             const ExportOptions& options) -> std::string;
    static auto exportToJsonl(const std::vector<LogEntry>& entries,
                              const ExportOptions& options) -> std::string;
    static auto exportToCsv(const std::vector<LogEntry>& entries,
                            const ExportOptions& options) -> std::string;
    static auto exportToText(const std::vector<LogEntry>& entries,
                             const ExportOptions& options) -> std::string;
    static auto exportToHtml(const std::vector<LogEntry>& entries,
                             const ExportOptions& options) -> std::string;

    static auto formatTimestamp(const std::chrono::system_clock::time_point& tp,
                                const std::string& format) -> std::string;
    static auto escapeHtml(const std::string& text) -> std::string;
    static auto escapeCsv(const std::string& text, const std::string& delimiter)
        -> std::string;
};

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_LOG_EXPORTER_HPP
