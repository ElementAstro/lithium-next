/*
 * logging.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Logging Management Controller - HTTP API for log management

**************************************************/

#ifndef LITHIUM_SERVER_CONTROLLER_LOGGING_HPP
#define LITHIUM_SERVER_CONTROLLER_LOGGING_HPP

#include "../utils/response.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "../controller.hpp"
#include "logging/logging_manager.hpp"

#include <chrono>
#include <string>

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Logging Management Controller
 *
 * Provides HTTP API endpoints for:
 * - Logger management (list, create, configure)
 * - Log level control
 * - Log streaming and retrieval
 * - Sink management
 * - Log rotation and buffer operations
 */
class LoggingController : public Controller {
public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        // ========== Logger Management ==========

        // List all loggers
        CROW_ROUTE(app, "/api/v1/logging/loggers")
            .methods("GET"_method)(&LoggingController::listLoggers, this);

        // Get specific logger info
        CROW_ROUTE(app, "/api/v1/logging/loggers/<string>")
            .methods("GET"_method)(&LoggingController::getLogger, this);

        // Create or update logger
        CROW_ROUTE(app, "/api/v1/logging/loggers/<string>")
            .methods("PUT"_method)(&LoggingController::updateLogger, this);

        // Delete logger
        CROW_ROUTE(app, "/api/v1/logging/loggers/<string>")
            .methods("DELETE"_method)(&LoggingController::deleteLogger, this);

        // ========== Level Management ==========

        // Set logger level
        CROW_ROUTE(app, "/api/v1/logging/loggers/<string>/level")
            .methods("PUT"_method)(&LoggingController::setLoggerLevel, this);

        // Set global level
        CROW_ROUTE(app, "/api/v1/logging/level")
            .methods("PUT"_method)(&LoggingController::setGlobalLevel, this);

        // Get global level
        CROW_ROUTE(app, "/api/v1/logging/level")
            .methods("GET"_method)(&LoggingController::getGlobalLevel, this);

        // ========== Sink Management ==========

        // List all sinks
        CROW_ROUTE(app, "/api/v1/logging/sinks")
            .methods("GET"_method)(&LoggingController::listSinks, this);

        // Add new sink
        CROW_ROUTE(app, "/api/v1/logging/sinks")
            .methods("POST"_method)(&LoggingController::addSink, this);

        // Remove sink
        CROW_ROUTE(app, "/api/v1/logging/sinks/<string>")
            .methods("DELETE"_method)(&LoggingController::removeSink, this);

        // ========== Log Retrieval ==========

        // Get recent logs
        CROW_ROUTE(app, "/api/v1/logging/logs")
            .methods("GET"_method)(&LoggingController::getLogs, this);

        // Get log buffer statistics
        CROW_ROUTE(app, "/api/v1/logging/buffer/stats")
            .methods("GET"_method)(&LoggingController::getBufferStats, this);

        // Clear log buffer
        CROW_ROUTE(app, "/api/v1/logging/buffer/clear")
            .methods("POST"_method)(&LoggingController::clearBuffer, this);

        // ========== Operations ==========

        // Flush all loggers
        CROW_ROUTE(app, "/api/v1/logging/flush")
            .methods("POST"_method)(&LoggingController::flush, this);

        // Trigger log rotation
        CROW_ROUTE(app, "/api/v1/logging/rotate")
            .methods("POST"_method)(&LoggingController::rotate, this);

        // Get logging configuration
        CROW_ROUTE(app, "/api/v1/logging/config")
            .methods("GET"_method)(&LoggingController::getConfig, this);

        // Update logging configuration
        CROW_ROUTE(app, "/api/v1/logging/config")
            .methods("PUT"_method)(&LoggingController::updateConfig, this);

        // ========== Statistics ==========

        // Get log statistics summary
        CROW_ROUTE(app, "/api/v1/logging/stats")
            .methods("GET"_method)(&LoggingController::getStats, this);

        // Get level statistics
        CROW_ROUTE(app, "/api/v1/logging/stats/levels")
            .methods("GET"_method)(&LoggingController::getLevelStats, this);

        // Get logger statistics
        CROW_ROUTE(app, "/api/v1/logging/stats/loggers")
            .methods("GET"_method)(&LoggingController::getLoggerStats, this);

        // Reset statistics
        CROW_ROUTE(app, "/api/v1/logging/stats/reset")
            .methods("POST"_method)(&LoggingController::resetStats, this);

        // ========== Search ==========

        // Search logs
        CROW_ROUTE(app, "/api/v1/logging/search")
            .methods("POST"_method)(&LoggingController::searchLogs, this);

        // ========== Export ==========

        // Export logs
        CROW_ROUTE(app, "/api/v1/logging/export")
            .methods("GET"_method)(&LoggingController::exportLogs, this);

        // Export logs to file
        CROW_ROUTE(app, "/api/v1/logging/export/file")
            .methods("POST"_method)(&LoggingController::exportLogsToFile, this);
    }

private:
    // ========== Logger Management Handlers ==========

    crow::response listLoggers(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            auto loggers = manager.listLoggers();

            nlohmann::json loggers_json = nlohmann::json::array();
            for (const auto& logger : loggers) {
                loggers_json.push_back(logger.toJson());
            }

            nlohmann::json data = {{"loggers", loggers_json},
                                   {"count", loggers.size()}};

            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to list loggers: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response getLogger(const crow::request& /*req*/,
                             const std::string& name) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            auto loggers = manager.listLoggers();

            for (const auto& logger : loggers) {
                if (logger.name == name) {
                    return ResponseBuilder::success(logger.toJson());
                }
            }

            return ResponseBuilder::notFound("logger", name);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get logger '{}': {}", name, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response updateLogger(const crow::request& req,
                                const std::string& name) {
        try {
            auto body = nlohmann::json::parse(req.body);
            auto& manager = logging::LoggingManager::getInstance();

            // Get or create logger
            auto logger = manager.getLogger(name);

            // Update level if provided
            if (body.contains("level")) {
                auto level =
                    logging::LoggingManager::levelFromString(body["level"]);
                manager.setLoggerLevel(name, level);
            }

            // Update pattern if provided
            if (body.contains("pattern")) {
                manager.setLoggerPattern(name, body["pattern"]);
            }

            nlohmann::json data = {{"name", name}, {"updated", true}};

            return ResponseBuilder::successWithMessage(
                "Logger updated successfully.", data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to update logger '{}': {}", name, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response deleteLogger(const crow::request& /*req*/,
                                const std::string& name) {
        try {
            auto& manager = logging::LoggingManager::getInstance();

            if (manager.removeLogger(name)) {
                return ResponseBuilder::successWithMessage(
                    "Logger deleted successfully.");
            } else {
                return ResponseBuilder::error(
                    400, "delete_failed",
                    "Cannot delete logger '" + name +
                        "'. It may be a system logger.");
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to delete logger '{}': {}", name, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    // ========== Level Management Handlers ==========

    crow::response setLoggerLevel(const crow::request& req,
                                  const std::string& name) {
        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("level")) {
                return ResponseBuilder::missingField("level");
            }

            auto& manager = logging::LoggingManager::getInstance();
            auto level =
                logging::LoggingManager::levelFromString(body["level"]);

            if (manager.setLoggerLevel(name, level)) {
                nlohmann::json data = {
                    {"name", name},
                    {"level", logging::LoggingManager::levelToString(level)}};
                return ResponseBuilder::successWithMessage(
                    "Logger level updated.", data);
            } else {
                return ResponseBuilder::notFound("logger", name);
            }
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to set logger level: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response setGlobalLevel(const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("level")) {
                return ResponseBuilder::missingField("level");
            }

            auto& manager = logging::LoggingManager::getInstance();
            auto level =
                logging::LoggingManager::levelFromString(body["level"]);
            manager.setGlobalLevel(level);

            nlohmann::json data = {
                {"level", logging::LoggingManager::levelToString(level)}};
            return ResponseBuilder::successWithMessage(
                "Global log level updated.", data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to set global level: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response getGlobalLevel(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            auto config = manager.getConfig();

            nlohmann::json data = {
                {"level",
                 logging::LoggingManager::levelToString(config.default_level)},
                {"pattern", config.default_pattern}};
            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get global level: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    // ========== Sink Management Handlers ==========

    crow::response listSinks(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            auto sinks = manager.listSinks();

            nlohmann::json sinks_json = nlohmann::json::array();
            for (const auto& sink : sinks) {
                sinks_json.push_back(sink.toJson());
            }

            nlohmann::json data = {{"sinks", sinks_json},
                                   {"count", sinks.size()}};

            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to list sinks: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response addSink(const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("name") || !body.contains("type")) {
                return ResponseBuilder::missingField("name and type");
            }

            auto config = logging::SinkConfig::fromJson(body);
            auto& manager = logging::LoggingManager::getInstance();

            if (manager.addSink(config)) {
                return ResponseBuilder::successWithMessage(
                    "Sink added successfully.", {{"name", config.name}});
            } else {
                return ResponseBuilder::error(
                    409, "sink_exists",
                    "Sink '" + config.name + "' already exists.");
            }
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to add sink: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response removeSink(const crow::request& /*req*/,
                              const std::string& name) {
        try {
            auto& manager = logging::LoggingManager::getInstance();

            if (manager.removeSink(name)) {
                return ResponseBuilder::successWithMessage(
                    "Sink removed successfully.");
            } else {
                return ResponseBuilder::error(
                    400, "remove_failed",
                    "Cannot remove sink '" + name +
                        "'. It may be a system sink.");
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to remove sink '{}': {}", name, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    // ========== Log Retrieval Handlers ==========

    crow::response getLogs(const crow::request& req) {
        try {
            auto& manager = logging::LoggingManager::getInstance();

            // Parse query parameters
            auto limit_str = req.url_params.get("limit");
            auto level_str = req.url_params.get("level");
            auto logger_str = req.url_params.get("logger");
            auto since_str = req.url_params.get("since");

            size_t limit = limit_str ? std::stoul(limit_str) : 100;

            std::optional<spdlog::level::level_enum> level_filter;
            if (level_str) {
                level_filter =
                    logging::LoggingManager::levelFromString(level_str);
            }

            std::optional<std::string> logger_filter;
            if (logger_str) {
                logger_filter = std::string(logger_str);
            }

            std::vector<logging::LogEntry> logs;

            if (since_str) {
                // Parse ISO timestamp and get logs since that time
                // For simplicity, just get recent logs
                logs =
                    manager.getLogsFiltered(level_filter, logger_filter, limit);
            } else {
                logs =
                    manager.getLogsFiltered(level_filter, logger_filter, limit);
            }

            nlohmann::json logs_json = nlohmann::json::array();
            for (const auto& entry : logs) {
                logs_json.push_back(entry.toJson());
            }

            nlohmann::json data = {
                {"logs", logs_json}, {"count", logs.size()}, {"limit", limit}};

            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get logs: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response getBufferStats(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            auto stats = manager.getBufferStats();

            return ResponseBuilder::success(stats);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get buffer stats: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response clearBuffer(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            manager.clearLogBuffer();

            return ResponseBuilder::successWithMessage(
                "Log buffer cleared successfully.");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to clear buffer: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    // ========== Operations Handlers ==========

    crow::response flush(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            manager.flush();

            return ResponseBuilder::successWithMessage(
                "All loggers flushed successfully.");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to flush loggers: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response rotate(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            manager.rotate();

            return ResponseBuilder::successWithMessage(
                "Log rotation triggered successfully.");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to rotate logs: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response getConfig(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            auto config = manager.getConfig();

            return ResponseBuilder::success(config.toJson());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get config: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response updateConfig(const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);
            auto& manager = logging::LoggingManager::getInstance();

            // Update individual config options
            if (body.contains("default_level")) {
                auto level = logging::LoggingManager::levelFromString(
                    body["default_level"]);
                manager.setGlobalLevel(level);
            }

            nlohmann::json data = {{"updated", true},
                                   {"note",
                                    "Some configuration changes may require "
                                    "restart to take full effect."}};

            return ResponseBuilder::successWithMessage("Configuration updated.",
                                                       data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to update config: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    // ========== Statistics Handlers ==========

    crow::response getStats(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            auto stats = manager.getStatsSummary();
            return ResponseBuilder::success(stats);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get stats: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response getLevelStats(const crow::request& /*req*/) {
        try {
            auto& stats = logging::LogStatistics::getInstance();
            return ResponseBuilder::success(stats.getLevelStats());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get level stats: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response getLoggerStats(const crow::request& /*req*/) {
        try {
            auto& stats = logging::LogStatistics::getInstance();
            return ResponseBuilder::success(stats.getLoggerStats());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get logger stats: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response resetStats(const crow::request& /*req*/) {
        try {
            auto& manager = logging::LoggingManager::getInstance();
            manager.resetStatistics();
            return ResponseBuilder::successWithMessage(
                "Statistics reset successfully.");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to reset stats: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    // ========== Search Handlers ==========

    crow::response searchLogs(const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);
            auto query = logging::LogSearchQuery::fromJson(body);

            auto& manager = logging::LoggingManager::getInstance();
            auto result = manager.searchLogs(query);

            return ResponseBuilder::success(result.toJson());
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to search logs: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    // ========== Export Handlers ==========

    crow::response exportLogs(const crow::request& req) {
        try {
            auto& manager = logging::LoggingManager::getInstance();

            // Parse query parameters
            auto format_str = req.url_params.get("format");
            auto limit_str = req.url_params.get("limit");
            auto pretty_str = req.url_params.get("pretty");

            logging::ExportOptions options;
            if (format_str) {
                options.format = logging::LogExporter::parseFormat(format_str);
            }
            if (pretty_str && std::string(pretty_str) == "true") {
                options.pretty_print = true;
            }

            size_t limit = limit_str ? std::stoul(limit_str) : 0;

            auto result = manager.exportLogs(options, limit);

            if (!result.success) {
                return ResponseBuilder::error(500, "export_failed",
                                              result.error_message);
            }

            // Return with appropriate content type
            crow::response resp(200, result.content);
            resp.set_header("Content-Type",
                            logging::LogExporter::getMimeType(options.format));
            resp.set_header(
                "Content-Disposition",
                "attachment; filename=\"logs" +
                    logging::LogExporter::getFileExtension(options.format) +
                    "\"");
            return resp;
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to export logs: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response exportLogsToFile(const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("file_path")) {
                return ResponseBuilder::missingField("file_path");
            }

            std::string file_path = body["file_path"];
            logging::ExportOptions options;

            if (body.contains("options")) {
                options = logging::ExportOptions::fromJson(body["options"]);
            }

            size_t limit = body.value("limit", 0);

            auto& manager = logging::LoggingManager::getInstance();
            auto result = manager.exportLogsToFile(file_path, options, limit);

            if (!result.success) {
                return ResponseBuilder::error(500, "export_failed",
                                              result.error_message);
            }

            return ResponseBuilder::successWithMessage(
                "Logs exported successfully.", result.toJson());
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to export logs to file: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_LOGGING_HPP
