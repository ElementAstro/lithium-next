/*
 * log_exporter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "log_exporter.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace lithium::logging {

// ============================================================================
// ExportOptions Implementation
// ============================================================================

auto ExportOptions::fromJson(const nlohmann::json& j) -> ExportOptions {
    ExportOptions options;

    if (j.contains("format")) {
        options.format =
            LogExporter::parseFormat(j["format"].get<std::string>());
    }
    if (j.contains("include_timestamp")) {
        options.include_timestamp = j["include_timestamp"].get<bool>();
    }
    if (j.contains("include_level")) {
        options.include_level = j["include_level"].get<bool>();
    }
    if (j.contains("include_logger")) {
        options.include_logger = j["include_logger"].get<bool>();
    }
    if (j.contains("include_thread_id")) {
        options.include_thread_id = j["include_thread_id"].get<bool>();
    }
    if (j.contains("include_source")) {
        options.include_source = j["include_source"].get<bool>();
    }
    if (j.contains("timestamp_format")) {
        options.timestamp_format = j["timestamp_format"].get<std::string>();
    }
    if (j.contains("csv_delimiter")) {
        options.csv_delimiter = j["csv_delimiter"].get<std::string>();
    }
    if (j.contains("csv_include_header")) {
        options.csv_include_header = j["csv_include_header"].get<bool>();
    }
    if (j.contains("pretty_print")) {
        options.pretty_print = j["pretty_print"].get<bool>();
    }

    return options;
}

auto ExportOptions::toJson() const -> nlohmann::json {
    std::string format_str;
    switch (format) {
        case ExportFormat::JSON:
            format_str = "json";
            break;
        case ExportFormat::JSONL:
            format_str = "jsonl";
            break;
        case ExportFormat::CSV:
            format_str = "csv";
            break;
        case ExportFormat::TEXT:
            format_str = "text";
            break;
        case ExportFormat::HTML:
            format_str = "html";
            break;
    }

    return {{"format", format_str},
            {"include_timestamp", include_timestamp},
            {"include_level", include_level},
            {"include_logger", include_logger},
            {"include_thread_id", include_thread_id},
            {"include_source", include_source},
            {"timestamp_format", timestamp_format},
            {"csv_delimiter", csv_delimiter},
            {"csv_include_header", csv_include_header},
            {"pretty_print", pretty_print}};
}

// ============================================================================
// ExportResult Implementation
// ============================================================================

auto ExportResult::toJson() const -> nlohmann::json {
    nlohmann::json j = {{"success", success},
                        {"entry_count", entry_count},
                        {"byte_count", byte_count}};

    if (!file_path.empty()) {
        j["file_path"] = file_path;
    }
    if (!error_message.empty()) {
        j["error"] = error_message;
    }

    return j;
}

// ============================================================================
// LogExporter Implementation
// ============================================================================

auto LogExporter::exportToString(const std::vector<LogEntry>& entries,
                                 const ExportOptions& options) -> ExportResult {
    ExportResult result;
    result.entry_count = entries.size();

    try {
        switch (options.format) {
            case ExportFormat::JSON:
                result.content = exportToJson(entries, options);
                break;
            case ExportFormat::JSONL:
                result.content = exportToJsonl(entries, options);
                break;
            case ExportFormat::CSV:
                result.content = exportToCsv(entries, options);
                break;
            case ExportFormat::TEXT:
                result.content = exportToText(entries, options);
                break;
            case ExportFormat::HTML:
                result.content = exportToHtml(entries, options);
                break;
        }

        result.byte_count = result.content.size();
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }

    return result;
}

auto LogExporter::exportToFile(const std::vector<LogEntry>& entries,
                               const std::filesystem::path& file_path,
                               const ExportOptions& options) -> ExportResult {
    auto result = exportToString(entries, options);

    if (!result.success) {
        return result;
    }

    try {
        // Ensure parent directory exists
        if (file_path.has_parent_path()) {
            std::filesystem::create_directories(file_path.parent_path());
        }

        std::ofstream file(file_path, std::ios::out | std::ios::trunc);
        if (!file) {
            result.success = false;
            result.error_message =
                "Failed to open file for writing: " + file_path.string();
            return result;
        }

        file << result.content;
        file.close();

        result.file_path = file_path.string();
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }

    return result;
}

auto LogExporter::exportStreaming(
    const std::vector<LogEntry>& entries, const ExportOptions& options,
    std::function<void(const std::string&)> callback) -> ExportResult {
    ExportResult result;
    result.entry_count = entries.size();

    try {
        // For streaming, we process entries one at a time
        if (options.format == ExportFormat::JSONL) {
            for (const auto& entry : entries) {
                auto json = entry.toJson();
                callback(json.dump() + "\n");
                result.byte_count += json.dump().size() + 1;
            }
        } else if (options.format == ExportFormat::CSV) {
            // Send header first
            if (options.csv_include_header) {
                std::string header;
                if (options.include_timestamp)
                    header += "timestamp" + options.csv_delimiter;
                if (options.include_level)
                    header += "level" + options.csv_delimiter;
                if (options.include_logger)
                    header += "logger" + options.csv_delimiter;
                if (options.include_thread_id)
                    header += "thread_id" + options.csv_delimiter;
                header += "message";
                if (options.include_source)
                    header += options.csv_delimiter + "source";
                header += "\n";
                callback(header);
                result.byte_count += header.size();
            }

            // Send each row
            for (const auto& entry : entries) {
                std::string row;
                if (options.include_timestamp) {
                    row += formatTimestamp(entry.timestamp,
                                           options.timestamp_format) +
                           options.csv_delimiter;
                }
                if (options.include_level) {
                    row += levelToString(entry.level) + options.csv_delimiter;
                }
                if (options.include_logger) {
                    row += escapeCsv(entry.logger_name, options.csv_delimiter) +
                           options.csv_delimiter;
                }
                if (options.include_thread_id) {
                    row += entry.thread_id + options.csv_delimiter;
                }
                row += escapeCsv(entry.message, options.csv_delimiter);
                if (options.include_source) {
                    row += options.csv_delimiter + entry.source_file + ":" +
                           std::to_string(entry.source_line);
                }
                row += "\n";
                callback(row);
                result.byte_count += row.size();
            }
        } else {
            // For other formats, export all at once
            auto full_result = exportToString(entries, options);
            callback(full_result.content);
            result.byte_count = full_result.byte_count;
        }

        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }

    return result;
}

auto LogExporter::getFileExtension(ExportFormat format) -> std::string {
    switch (format) {
        case ExportFormat::JSON:
            return ".json";
        case ExportFormat::JSONL:
            return ".jsonl";
        case ExportFormat::CSV:
            return ".csv";
        case ExportFormat::TEXT:
            return ".txt";
        case ExportFormat::HTML:
            return ".html";
    }
    return ".txt";
}

auto LogExporter::getMimeType(ExportFormat format) -> std::string {
    switch (format) {
        case ExportFormat::JSON:
            return "application/json";
        case ExportFormat::JSONL:
            return "application/x-ndjson";
        case ExportFormat::CSV:
            return "text/csv";
        case ExportFormat::TEXT:
            return "text/plain";
        case ExportFormat::HTML:
            return "text/html";
    }
    return "text/plain";
}

auto LogExporter::parseFormat(const std::string& format_str) -> ExportFormat {
    std::string lower = format_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "json")
        return ExportFormat::JSON;
    if (lower == "jsonl" || lower == "ndjson")
        return ExportFormat::JSONL;
    if (lower == "csv")
        return ExportFormat::CSV;
    if (lower == "text" || lower == "txt")
        return ExportFormat::TEXT;
    if (lower == "html")
        return ExportFormat::HTML;

    return ExportFormat::JSON;  // Default
}

auto LogExporter::exportToJson(const std::vector<LogEntry>& entries,
                               const ExportOptions& options) -> std::string {
    nlohmann::json arr = nlohmann::json::array();

    for (const auto& entry : entries) {
        arr.push_back(entry.toJson());
    }

    if (options.pretty_print) {
        return arr.dump(2);
    }
    return arr.dump();
}

auto LogExporter::exportToJsonl(const std::vector<LogEntry>& entries,
                                const ExportOptions& options) -> std::string {
    std::ostringstream oss;

    for (const auto& entry : entries) {
        oss << entry.toJson().dump() << "\n";
    }

    return oss.str();
}

auto LogExporter::exportToCsv(const std::vector<LogEntry>& entries,
                              const ExportOptions& options) -> std::string {
    std::ostringstream oss;
    const std::string& d = options.csv_delimiter;

    // Header
    if (options.csv_include_header) {
        if (options.include_timestamp)
            oss << "timestamp" << d;
        if (options.include_level)
            oss << "level" << d;
        if (options.include_logger)
            oss << "logger" << d;
        if (options.include_thread_id)
            oss << "thread_id" << d;
        oss << "message";
        if (options.include_source)
            oss << d << "source_file" << d << "source_line";
        oss << "\n";
    }

    // Data rows
    for (const auto& entry : entries) {
        if (options.include_timestamp) {
            oss << formatTimestamp(entry.timestamp, options.timestamp_format)
                << d;
        }
        if (options.include_level) {
            oss << levelToString(entry.level) << d;
        }
        if (options.include_logger) {
            oss << escapeCsv(entry.logger_name, d) << d;
        }
        if (options.include_thread_id) {
            oss << entry.thread_id << d;
        }
        oss << escapeCsv(entry.message, d);
        if (options.include_source) {
            oss << d << escapeCsv(entry.source_file, d) << d
                << entry.source_line;
        }
        oss << "\n";
    }

    return oss.str();
}

auto LogExporter::exportToText(const std::vector<LogEntry>& entries,
                               const ExportOptions& options) -> std::string {
    std::ostringstream oss;

    for (const auto& entry : entries) {
        if (options.include_timestamp) {
            oss << "["
                << formatTimestamp(entry.timestamp, options.timestamp_format)
                << "] ";
        }
        if (options.include_level) {
            oss << "[" << levelToString(entry.level) << "] ";
        }
        if (options.include_logger) {
            oss << "[" << entry.logger_name << "] ";
        }
        if (options.include_thread_id) {
            oss << "[" << entry.thread_id << "] ";
        }
        oss << entry.message;
        if (options.include_source && !entry.source_file.empty()) {
            oss << " (" << entry.source_file << ":" << entry.source_line << ")";
        }
        oss << "\n";
    }

    return oss.str();
}

auto LogExporter::exportToHtml(const std::vector<LogEntry>& entries,
                               const ExportOptions& options) -> std::string {
    std::ostringstream oss;

    oss << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Log Export</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, sans-serif; margin: 20px; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #4CAF50; color: white; }
        tr:nth-child(even) { background-color: #f2f2f2; }
        tr:hover { background-color: #ddd; }
        .level-trace { color: #888; }
        .level-debug { color: #666; }
        .level-info { color: #2196F3; }
        .level-warn { color: #FF9800; }
        .level-error { color: #f44336; }
        .level-critical { color: #fff; background-color: #f44336; }
    </style>
</head>
<body>
    <h1>Log Export</h1>
    <p>Total entries: )"
        << entries.size() << R"(</p>
    <table>
        <thead>
            <tr>)";

    if (options.include_timestamp)
        oss << "<th>Timestamp</th>";
    if (options.include_level)
        oss << "<th>Level</th>";
    if (options.include_logger)
        oss << "<th>Logger</th>";
    if (options.include_thread_id)
        oss << "<th>Thread</th>";
    oss << "<th>Message</th>";
    if (options.include_source)
        oss << "<th>Source</th>";

    oss << R"(
            </tr>
        </thead>
        <tbody>)";

    for (const auto& entry : entries) {
        std::string level_class = "level-" + levelToString(entry.level);
        oss << "\n            <tr class=\"" << level_class << "\">";

        if (options.include_timestamp) {
            oss << "<td>"
                << escapeHtml(formatTimestamp(entry.timestamp,
                                              options.timestamp_format))
                << "</td>";
        }
        if (options.include_level) {
            oss << "<td>" << escapeHtml(levelToString(entry.level)) << "</td>";
        }
        if (options.include_logger) {
            oss << "<td>" << escapeHtml(entry.logger_name) << "</td>";
        }
        if (options.include_thread_id) {
            oss << "<td>" << escapeHtml(entry.thread_id) << "</td>";
        }
        oss << "<td>" << escapeHtml(entry.message) << "</td>";
        if (options.include_source) {
            oss << "<td>" << escapeHtml(entry.source_file) << ":"
                << entry.source_line << "</td>";
        }
        oss << "</tr>";
    }

    oss << R"(
        </tbody>
    </table>
</body>
</html>)";

    return oss.str();
}

auto LogExporter::formatTimestamp(
    const std::chrono::system_clock::time_point& tp, const std::string& format)
    -> std::string {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch()) %
              1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), format.c_str());

    // Append milliseconds if format contains %f placeholder
    if (format.find("%f") != std::string::npos) {
        oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    }

    return oss.str();
}

auto LogExporter::escapeHtml(const std::string& text) -> std::string {
    std::string result;
    result.reserve(text.size() * 1.1);

    for (char c : text) {
        switch (c) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\'':
                result += "&#39;";
                break;
            default:
                result += c;
        }
    }

    return result;
}

auto LogExporter::escapeCsv(const std::string& text,
                            const std::string& delimiter) -> std::string {
    bool needs_quotes = text.find(delimiter) != std::string::npos ||
                        text.find('"') != std::string::npos ||
                        text.find('\n') != std::string::npos ||
                        text.find('\r') != std::string::npos;

    if (!needs_quotes) {
        return text;
    }

    std::string result = "\"";
    for (char c : text) {
        if (c == '"') {
            result += "\"\"";
        } else {
            result += c;
        }
    }
    result += "\"";

    return result;
}

}  // namespace lithium::logging
