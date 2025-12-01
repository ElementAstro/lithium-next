/*
 * test_log_exporter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Comprehensive tests for LogExporter

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logging/log_exporter.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace lithium::logging;
using namespace std::chrono_literals;

class LogExporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory
        test_dir_ = std::filesystem::temp_directory_path() / "log_exporter_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    std::vector<LogEntry> createTestEntries(size_t count = 3) {
        std::vector<LogEntry> entries;
        auto base_time = std::chrono::system_clock::now();

        for (size_t i = 0; i < count; ++i) {
            LogEntry entry;
            entry.timestamp = base_time + std::chrono::seconds(i);
            entry.level = static_cast<spdlog::level::level_enum>(i % 5);
            entry.logger_name = "test_logger_" + std::to_string(i);
            entry.message = "Test message " + std::to_string(i);
            entry.thread_id = std::to_string(1000 + i);
            entry.source_file = "test" + std::to_string(i) + ".cpp";
            entry.source_line = static_cast<int>(i * 10);
            entries.push_back(entry);
        }

        return entries;
    }

    LogEntry createSingleEntry() {
        LogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.level = spdlog::level::info;
        entry.logger_name = "single_logger";
        entry.message = "Single message";
        entry.thread_id = "12345";
        entry.source_file = "single.cpp";
        entry.source_line = 42;
        return entry;
    }

    std::filesystem::path test_dir_;
};

// ============================================================================
// ExportOptions Tests
// ============================================================================

class ExportOptionsTest : public ::testing::Test {};

TEST_F(ExportOptionsTest, DefaultConstruction) {
    ExportOptions options;

    EXPECT_EQ(options.format, ExportFormat::JSON);
    EXPECT_TRUE(options.include_timestamp);
    EXPECT_TRUE(options.include_level);
    EXPECT_TRUE(options.include_logger);
    EXPECT_FALSE(options.include_thread_id);
    EXPECT_FALSE(options.include_source);
    EXPECT_EQ(options.timestamp_format, "%Y-%m-%d %H:%M:%S");
    EXPECT_EQ(options.csv_delimiter, ",");
    EXPECT_TRUE(options.csv_include_header);
    EXPECT_FALSE(options.pretty_print);
}

TEST_F(ExportOptionsTest, ToJsonContainsAllFields) {
    ExportOptions options;
    options.format = ExportFormat::CSV;
    options.include_thread_id = true;
    options.pretty_print = true;

    auto json = options.toJson();

    EXPECT_TRUE(json.contains("format"));
    EXPECT_TRUE(json.contains("include_timestamp"));
    EXPECT_TRUE(json.contains("include_level"));
    EXPECT_TRUE(json.contains("include_logger"));
    EXPECT_TRUE(json.contains("include_thread_id"));
    EXPECT_TRUE(json.contains("include_source"));
    EXPECT_TRUE(json.contains("timestamp_format"));
    EXPECT_TRUE(json.contains("csv_delimiter"));
    EXPECT_TRUE(json.contains("csv_include_header"));
    EXPECT_TRUE(json.contains("pretty_print"));
}

TEST_F(ExportOptionsTest, ToJsonFormatStrings) {
    ExportOptions options;

    options.format = ExportFormat::JSON;
    EXPECT_EQ(options.toJson()["format"], "json");

    options.format = ExportFormat::JSONL;
    EXPECT_EQ(options.toJson()["format"], "jsonl");

    options.format = ExportFormat::CSV;
    EXPECT_EQ(options.toJson()["format"], "csv");

    options.format = ExportFormat::TEXT;
    EXPECT_EQ(options.toJson()["format"], "text");

    options.format = ExportFormat::HTML;
    EXPECT_EQ(options.toJson()["format"], "html");
}

TEST_F(ExportOptionsTest, FromJsonBasic) {
    nlohmann::json json = {{"format", "csv"},
                           {"include_timestamp", false},
                           {"include_level", true},
                           {"include_logger", false},
                           {"include_thread_id", true},
                           {"include_source", true},
                           {"timestamp_format", "%H:%M:%S"},
                           {"csv_delimiter", ";"},
                           {"csv_include_header", false},
                           {"pretty_print", true}};

    auto options = ExportOptions::fromJson(json);

    EXPECT_EQ(options.format, ExportFormat::CSV);
    EXPECT_FALSE(options.include_timestamp);
    EXPECT_TRUE(options.include_level);
    EXPECT_FALSE(options.include_logger);
    EXPECT_TRUE(options.include_thread_id);
    EXPECT_TRUE(options.include_source);
    EXPECT_EQ(options.timestamp_format, "%H:%M:%S");
    EXPECT_EQ(options.csv_delimiter, ";");
    EXPECT_FALSE(options.csv_include_header);
    EXPECT_TRUE(options.pretty_print);
}

TEST_F(ExportOptionsTest, FromJsonMissingFields) {
    nlohmann::json json = {{"format", "text"}};

    auto options = ExportOptions::fromJson(json);

    EXPECT_EQ(options.format, ExportFormat::TEXT);
    // Other fields should have defaults
    EXPECT_TRUE(options.include_timestamp);
    EXPECT_TRUE(options.include_level);
}

TEST_F(ExportOptionsTest, FromJsonEmptyObject) {
    nlohmann::json json = nlohmann::json::object();

    auto options = ExportOptions::fromJson(json);

    EXPECT_EQ(options.format, ExportFormat::JSON);  // Default
}

TEST_F(ExportOptionsTest, RoundTripConversion) {
    ExportOptions original;
    original.format = ExportFormat::HTML;
    original.include_thread_id = true;
    original.include_source = true;
    original.csv_delimiter = "|";
    original.pretty_print = true;

    auto json = original.toJson();
    auto restored = ExportOptions::fromJson(json);

    EXPECT_EQ(restored.format, original.format);
    EXPECT_EQ(restored.include_thread_id, original.include_thread_id);
    EXPECT_EQ(restored.include_source, original.include_source);
    EXPECT_EQ(restored.csv_delimiter, original.csv_delimiter);
    EXPECT_EQ(restored.pretty_print, original.pretty_print);
}

// ============================================================================
// ExportResult Tests
// ============================================================================

class ExportResultTest : public ::testing::Test {};

TEST_F(ExportResultTest, DefaultConstruction) {
    ExportResult result;

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.content.empty());
    EXPECT_TRUE(result.file_path.empty());
    EXPECT_EQ(result.entry_count, 0);
    EXPECT_EQ(result.byte_count, 0);
    EXPECT_TRUE(result.error_message.empty());
}

TEST_F(ExportResultTest, ToJsonSuccess) {
    ExportResult result;
    result.success = true;
    result.entry_count = 100;
    result.byte_count = 5000;
    result.file_path = "/tmp/export.json";

    auto json = result.toJson();

    EXPECT_TRUE(json["success"].get<bool>());
    EXPECT_EQ(json["entry_count"], 100);
    EXPECT_EQ(json["byte_count"], 5000);
    EXPECT_EQ(json["file_path"], "/tmp/export.json");
    EXPECT_FALSE(json.contains("error"));
}

TEST_F(ExportResultTest, ToJsonFailure) {
    ExportResult result;
    result.success = false;
    result.error_message = "File not found";

    auto json = result.toJson();

    EXPECT_FALSE(json["success"].get<bool>());
    EXPECT_EQ(json["error"], "File not found");
}

TEST_F(ExportResultTest, ToJsonNoFilePath) {
    ExportResult result;
    result.success = true;
    result.content = "exported content";

    auto json = result.toJson();

    EXPECT_FALSE(json.contains("file_path"));
}

// ============================================================================
// JSON Export Tests
// ============================================================================

TEST_F(LogExporterTest, ExportToJsonEmpty) {
    std::vector<LogEntry> entries;
    ExportOptions options;
    options.format = ExportFormat::JSON;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.entry_count, 0);
    EXPECT_EQ(result.content, "[]");
}

TEST_F(LogExporterTest, ExportToJsonSingleEntry) {
    std::vector<LogEntry> entries = {createSingleEntry()};
    ExportOptions options;
    options.format = ExportFormat::JSON;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.entry_count, 1);

    auto json = nlohmann::json::parse(result.content);
    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 1);
}

TEST_F(LogExporterTest, ExportToJsonMultipleEntries) {
    auto entries = createTestEntries(5);
    ExportOptions options;
    options.format = ExportFormat::JSON;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.entry_count, 5);

    auto json = nlohmann::json::parse(result.content);
    EXPECT_EQ(json.size(), 5);
}

TEST_F(LogExporterTest, ExportToJsonPrettyPrint) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::JSON;
    options.pretty_print = true;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // Pretty printed JSON should contain newlines and indentation
    EXPECT_TRUE(result.content.find('\n') != std::string::npos);
    EXPECT_TRUE(result.content.find("  ") != std::string::npos);
}

TEST_F(LogExporterTest, ExportToJsonCompact) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::JSON;
    options.pretty_print = false;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // Compact JSON should be on single line (no newlines except in content)
    auto first_newline = result.content.find('\n');
    // Either no newlines or only at the very end
    EXPECT_TRUE(first_newline == std::string::npos ||
                first_newline == result.content.size() - 1);
}

// ============================================================================
// JSONL Export Tests
// ============================================================================

TEST_F(LogExporterTest, ExportToJsonlEmpty) {
    std::vector<LogEntry> entries;
    ExportOptions options;
    options.format = ExportFormat::JSONL;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.empty());
}

TEST_F(LogExporterTest, ExportToJsonlMultipleEntries) {
    auto entries = createTestEntries(3);
    ExportOptions options;
    options.format = ExportFormat::JSONL;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);

    // Each line should be valid JSON
    std::istringstream iss(result.content);
    std::string line;
    int line_count = 0;
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            EXPECT_NO_THROW(nlohmann::json::parse(line));
            ++line_count;
        }
    }
    EXPECT_EQ(line_count, 3);
}

TEST_F(LogExporterTest, ExportToJsonlLineEndings) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::JSONL;

    auto result = LogExporter::exportToString(entries, options);

    // Each entry should end with newline
    size_t newline_count =
        std::count(result.content.begin(), result.content.end(), '\n');
    EXPECT_EQ(newline_count, 2);
}

// ============================================================================
// CSV Export Tests
// ============================================================================

TEST_F(LogExporterTest, ExportToCsvEmpty) {
    std::vector<LogEntry> entries;
    ExportOptions options;
    options.format = ExportFormat::CSV;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // Should only have header
    EXPECT_FALSE(result.content.empty());
}

TEST_F(LogExporterTest, ExportToCsvWithHeader) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::CSV;
    options.csv_include_header = true;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // First line should be header
    EXPECT_TRUE(result.content.find("timestamp") != std::string::npos);
    EXPECT_TRUE(result.content.find("level") != std::string::npos);
    EXPECT_TRUE(result.content.find("message") != std::string::npos);
}

TEST_F(LogExporterTest, ExportToCsvWithoutHeader) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::CSV;
    options.csv_include_header = false;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // Should not start with header keywords
    EXPECT_TRUE(result.content.find("timestamp,level") == std::string::npos);
}

TEST_F(LogExporterTest, ExportToCsvCustomDelimiter) {
    auto entries = createTestEntries(1);
    ExportOptions options;
    options.format = ExportFormat::CSV;
    options.csv_delimiter = ";";

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.find(';') != std::string::npos);
}

TEST_F(LogExporterTest, ExportToCsvWithThreadId) {
    auto entries = createTestEntries(1);
    ExportOptions options;
    options.format = ExportFormat::CSV;
    options.include_thread_id = true;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.find("thread_id") != std::string::npos);
}

TEST_F(LogExporterTest, ExportToCsvWithSource) {
    auto entries = createTestEntries(1);
    ExportOptions options;
    options.format = ExportFormat::CSV;
    options.include_source = true;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.find("source_file") != std::string::npos);
}

TEST_F(LogExporterTest, ExportToCsvEscapesCommas) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = spdlog::level::info;
    entry.logger_name = "test";
    entry.message = "Message with, comma";

    std::vector<LogEntry> entries = {entry};
    ExportOptions options;
    options.format = ExportFormat::CSV;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // Message with comma should be quoted
    EXPECT_TRUE(result.content.find("\"Message with, comma\"") !=
                std::string::npos);
}

TEST_F(LogExporterTest, ExportToCsvEscapesQuotes) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = spdlog::level::info;
    entry.logger_name = "test";
    entry.message = "Message with \"quotes\"";

    std::vector<LogEntry> entries = {entry};
    ExportOptions options;
    options.format = ExportFormat::CSV;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // Quotes should be escaped
    EXPECT_TRUE(result.content.find("\"\"") != std::string::npos);
}

// ============================================================================
// TEXT Export Tests
// ============================================================================

TEST_F(LogExporterTest, ExportToTextEmpty) {
    std::vector<LogEntry> entries;
    ExportOptions options;
    options.format = ExportFormat::TEXT;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.empty());
}

TEST_F(LogExporterTest, ExportToTextBasic) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::TEXT;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.content.empty());
    // Should contain log messages
    EXPECT_TRUE(result.content.find("Test message") != std::string::npos);
}

TEST_F(LogExporterTest, ExportToTextWithAllFields) {
    auto entries = createTestEntries(1);
    ExportOptions options;
    options.format = ExportFormat::TEXT;
    options.include_timestamp = true;
    options.include_level = true;
    options.include_logger = true;
    options.include_thread_id = true;
    options.include_source = true;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // Should contain brackets for formatted fields
    size_t bracket_count =
        std::count(result.content.begin(), result.content.end(), '[');
    EXPECT_GE(bracket_count, 4);  // timestamp, level, logger, thread_id
}

TEST_F(LogExporterTest, ExportToTextMinimalFields) {
    auto entries = createTestEntries(1);
    ExportOptions options;
    options.format = ExportFormat::TEXT;
    options.include_timestamp = false;
    options.include_level = false;
    options.include_logger = false;
    options.include_thread_id = false;
    options.include_source = false;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // Should only contain message
    EXPECT_TRUE(result.content.find("Test message 0") != std::string::npos);
}

// ============================================================================
// HTML Export Tests
// ============================================================================

TEST_F(LogExporterTest, ExportToHtmlEmpty) {
    std::vector<LogEntry> entries;
    ExportOptions options;
    options.format = ExportFormat::HTML;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.find("<html>") != std::string::npos);
    EXPECT_TRUE(result.content.find("</html>") != std::string::npos);
    EXPECT_TRUE(result.content.find("<table>") != std::string::npos);
}

TEST_F(LogExporterTest, ExportToHtmlBasic) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::HTML;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.find("<tr>") != std::string::npos);
    EXPECT_TRUE(result.content.find("<td>") != std::string::npos);
    EXPECT_TRUE(result.content.find("Test message") != std::string::npos);
}

TEST_F(LogExporterTest, ExportToHtmlContainsStyles) {
    auto entries = createTestEntries(1);
    ExportOptions options;
    options.format = ExportFormat::HTML;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.find("<style>") != std::string::npos);
    EXPECT_TRUE(result.content.find("level-") != std::string::npos);
}

TEST_F(LogExporterTest, ExportToHtmlEscapesHtml) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = spdlog::level::info;
    entry.logger_name = "test";
    entry.message = "<script>alert('xss')</script>";

    std::vector<LogEntry> entries = {entry};
    ExportOptions options;
    options.format = ExportFormat::HTML;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // HTML should be escaped
    EXPECT_TRUE(result.content.find("&lt;script&gt;") != std::string::npos);
    EXPECT_TRUE(result.content.find("<script>alert") == std::string::npos);
}

TEST_F(LogExporterTest, ExportToHtmlEntryCount) {
    auto entries = createTestEntries(5);
    ExportOptions options;
    options.format = ExportFormat::HTML;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.find("Total entries: 5") != std::string::npos);
}

// ============================================================================
// File Export Tests
// ============================================================================

TEST_F(LogExporterTest, ExportToFileJson) {
    auto entries = createTestEntries(3);
    auto file_path = test_dir_ / "export.json";
    ExportOptions options;
    options.format = ExportFormat::JSON;

    auto result = LogExporter::exportToFile(entries, file_path, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(file_path));
    EXPECT_EQ(result.file_path, file_path.string());

    // Verify file content
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    auto json = nlohmann::json::parse(content);
    EXPECT_EQ(json.size(), 3);
}

TEST_F(LogExporterTest, ExportToFileCreatesDirectory) {
    auto entries = createTestEntries(1);
    auto file_path = test_dir_ / "subdir" / "nested" / "export.json";
    ExportOptions options;
    options.format = ExportFormat::JSON;

    auto result = LogExporter::exportToFile(entries, file_path, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(file_path));
}

TEST_F(LogExporterTest, ExportToFileCsv) {
    auto entries = createTestEntries(2);
    auto file_path = test_dir_ / "export.csv";
    ExportOptions options;
    options.format = ExportFormat::CSV;

    auto result = LogExporter::exportToFile(entries, file_path, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(file_path));
}

TEST_F(LogExporterTest, ExportToFileHtml) {
    auto entries = createTestEntries(2);
    auto file_path = test_dir_ / "export.html";
    ExportOptions options;
    options.format = ExportFormat::HTML;

    auto result = LogExporter::exportToFile(entries, file_path, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::filesystem::exists(file_path));

    // Verify it's valid HTML
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("<!DOCTYPE html>") != std::string::npos);
}

// ============================================================================
// Streaming Export Tests
// ============================================================================

TEST_F(LogExporterTest, ExportStreamingJsonl) {
    auto entries = createTestEntries(3);
    ExportOptions options;
    options.format = ExportFormat::JSONL;

    std::vector<std::string> chunks;
    auto result = LogExporter::exportStreaming(
        entries, options, [&chunks](const std::string& chunk) {
            chunks.push_back(chunk);
        });

    EXPECT_TRUE(result.success);
    EXPECT_EQ(chunks.size(), 3);  // One chunk per entry
}

TEST_F(LogExporterTest, ExportStreamingCsv) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::CSV;
    options.csv_include_header = true;

    std::vector<std::string> chunks;
    auto result = LogExporter::exportStreaming(
        entries, options, [&chunks](const std::string& chunk) {
            chunks.push_back(chunk);
        });

    EXPECT_TRUE(result.success);
    EXPECT_EQ(chunks.size(), 3);  // Header + 2 entries
}

TEST_F(LogExporterTest, ExportStreamingCsvNoHeader) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::CSV;
    options.csv_include_header = false;

    std::vector<std::string> chunks;
    auto result = LogExporter::exportStreaming(
        entries, options, [&chunks](const std::string& chunk) {
            chunks.push_back(chunk);
        });

    EXPECT_TRUE(result.success);
    EXPECT_EQ(chunks.size(), 2);  // Just entries, no header
}

TEST_F(LogExporterTest, ExportStreamingOtherFormats) {
    auto entries = createTestEntries(2);
    ExportOptions options;
    options.format = ExportFormat::JSON;  // Non-streaming format

    std::vector<std::string> chunks;
    auto result = LogExporter::exportStreaming(
        entries, options, [&chunks](const std::string& chunk) {
            chunks.push_back(chunk);
        });

    EXPECT_TRUE(result.success);
    EXPECT_EQ(chunks.size(), 1);  // All at once
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(LogExporterTest, GetFileExtensionJson) {
    EXPECT_EQ(LogExporter::getFileExtension(ExportFormat::JSON), ".json");
}

TEST_F(LogExporterTest, GetFileExtensionJsonl) {
    EXPECT_EQ(LogExporter::getFileExtension(ExportFormat::JSONL), ".jsonl");
}

TEST_F(LogExporterTest, GetFileExtensionCsv) {
    EXPECT_EQ(LogExporter::getFileExtension(ExportFormat::CSV), ".csv");
}

TEST_F(LogExporterTest, GetFileExtensionText) {
    EXPECT_EQ(LogExporter::getFileExtension(ExportFormat::TEXT), ".txt");
}

TEST_F(LogExporterTest, GetFileExtensionHtml) {
    EXPECT_EQ(LogExporter::getFileExtension(ExportFormat::HTML), ".html");
}

TEST_F(LogExporterTest, GetMimeTypeJson) {
    EXPECT_EQ(LogExporter::getMimeType(ExportFormat::JSON), "application/json");
}

TEST_F(LogExporterTest, GetMimeTypeJsonl) {
    EXPECT_EQ(LogExporter::getMimeType(ExportFormat::JSONL),
              "application/x-ndjson");
}

TEST_F(LogExporterTest, GetMimeTypeCsv) {
    EXPECT_EQ(LogExporter::getMimeType(ExportFormat::CSV), "text/csv");
}

TEST_F(LogExporterTest, GetMimeTypeText) {
    EXPECT_EQ(LogExporter::getMimeType(ExportFormat::TEXT), "text/plain");
}

TEST_F(LogExporterTest, GetMimeTypeHtml) {
    EXPECT_EQ(LogExporter::getMimeType(ExportFormat::HTML), "text/html");
}

TEST_F(LogExporterTest, ParseFormatJson) {
    EXPECT_EQ(LogExporter::parseFormat("json"), ExportFormat::JSON);
    EXPECT_EQ(LogExporter::parseFormat("JSON"), ExportFormat::JSON);
    EXPECT_EQ(LogExporter::parseFormat("Json"), ExportFormat::JSON);
}

TEST_F(LogExporterTest, ParseFormatJsonl) {
    EXPECT_EQ(LogExporter::parseFormat("jsonl"), ExportFormat::JSONL);
    EXPECT_EQ(LogExporter::parseFormat("ndjson"), ExportFormat::JSONL);
}

TEST_F(LogExporterTest, ParseFormatCsv) {
    EXPECT_EQ(LogExporter::parseFormat("csv"), ExportFormat::CSV);
    EXPECT_EQ(LogExporter::parseFormat("CSV"), ExportFormat::CSV);
}

TEST_F(LogExporterTest, ParseFormatText) {
    EXPECT_EQ(LogExporter::parseFormat("text"), ExportFormat::TEXT);
    EXPECT_EQ(LogExporter::parseFormat("txt"), ExportFormat::TEXT);
}

TEST_F(LogExporterTest, ParseFormatHtml) {
    EXPECT_EQ(LogExporter::parseFormat("html"), ExportFormat::HTML);
    EXPECT_EQ(LogExporter::parseFormat("HTML"), ExportFormat::HTML);
}

TEST_F(LogExporterTest, ParseFormatUnknown) {
    EXPECT_EQ(LogExporter::parseFormat("unknown"), ExportFormat::JSON);
    EXPECT_EQ(LogExporter::parseFormat(""), ExportFormat::JSON);
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(LogExporterTest, ExportLargeDataset) {
    auto entries = createTestEntries(1000);
    ExportOptions options;
    options.format = ExportFormat::JSON;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.entry_count, 1000);
    EXPECT_GT(result.byte_count, 0);
}

TEST_F(LogExporterTest, ExportUnicodeContent) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = spdlog::level::info;
    entry.logger_name = "unicode_test";
    entry.message = "Unicode: 你好世界 🌍 αβγδ";

    std::vector<LogEntry> entries = {entry};
    ExportOptions options;
    options.format = ExportFormat::JSON;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.content.find("你好世界") != std::string::npos);
}

TEST_F(LogExporterTest, ExportNewlinesInMessage) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = spdlog::level::info;
    entry.logger_name = "newline_test";
    entry.message = "Line 1\nLine 2\nLine 3";

    std::vector<LogEntry> entries = {entry};
    ExportOptions options;
    options.format = ExportFormat::CSV;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    // Message with newlines should be quoted in CSV
    EXPECT_TRUE(result.content.find("\"Line 1\nLine 2\nLine 3\"") !=
                std::string::npos);
}

TEST_F(LogExporterTest, ExportEmptyMessage) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = spdlog::level::info;
    entry.logger_name = "empty_msg";
    entry.message = "";

    std::vector<LogEntry> entries = {entry};
    ExportOptions options;
    options.format = ExportFormat::JSON;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    auto json = nlohmann::json::parse(result.content);
    EXPECT_EQ(json[0]["message"], "");
}

TEST_F(LogExporterTest, ByteCountAccuracy) {
    auto entries = createTestEntries(5);
    ExportOptions options;
    options.format = ExportFormat::JSON;

    auto result = LogExporter::exportToString(entries, options);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.byte_count, result.content.size());
}
