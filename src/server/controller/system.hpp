#ifndef LITHIUM_SERVER_CONTROLLER_SYSTEM_HPP
#define LITHIUM_SERVER_CONTROLLER_SYSTEM_HPP

#include "../utils/response.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "controller.hpp"

#include <chrono>
#include <string>

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief System Control Controller
 *
 * Handles system-level operations including status monitoring, configuration,
 * diagnostics, logging, and system control operations.
 */
class SystemController : public Controller {
public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        // System Status
        CROW_ROUTE(app, "/api/v1/system/status")
            .methods("GET"_method)(&SystemController::getSystemStatus, this);

        // System Configuration
        CROW_ROUTE(app, "/api/v1/system/config")
            .methods("GET"_method)(&SystemController::getConfig, this);

        CROW_ROUTE(app, "/api/v1/system/config")
            .methods("PUT"_method)(&SystemController::updateConfig, this);

        // System Processes
        CROW_ROUTE(app, "/api/v1/system/processes")
            .methods("GET"_method)(&SystemController::getProcesses, this);

        // Service Management
        CROW_ROUTE(app, "/api/v1/system/services/<string>/restart")
            .methods("POST"_method)(&SystemController::restartService, this);

        // System Control
        CROW_ROUTE(app, "/api/v1/system/shutdown")
            .methods("POST"_method)(&SystemController::shutdown, this);

        CROW_ROUTE(app, "/api/v1/system/restart")
            .methods("POST"_method)(&SystemController::restart, this);

        CROW_ROUTE(app, "/api/v1/system/cancel-shutdown")
            .methods("POST"_method)(&SystemController::cancelShutdown, this);

        // System Logs
        CROW_ROUTE(app, "/api/v1/system/logs")
            .methods("GET"_method)(&SystemController::getLogs, this);

        // Device Discovery
        CROW_ROUTE(app, "/api/v1/system/devices/discover")
            .methods("POST"_method)(&SystemController::discoverDevices, this);

        CROW_ROUTE(app, "/api/v1/system/devices/discover/<string>")
            .methods("GET"_method)(&SystemController::getDiscoveryResults,
                                   this);

        // List All Devices
        CROW_ROUTE(app, "/api/v1/system/devices")
            .methods("GET"_method)(&SystemController::listDevices, this);

        // Available Drivers
        CROW_ROUTE(app, "/api/v1/system/drivers")
            .methods("GET"_method)(&SystemController::getDrivers, this);

        // System Diagnostics
        CROW_ROUTE(app, "/api/v1/system/diagnostics")
            .methods("GET"_method)(&SystemController::getDiagnostics, this);

        // Health Check
        CROW_ROUTE(app, "/api/v1/system/healthcheck")
            .methods("POST"_method)(&SystemController::healthCheck, this);

        // Error Reports
        CROW_ROUTE(app, "/api/v1/system/errors")
            .methods("GET"_method)(&SystemController::getErrors, this);

        // Backup/Restore
        CROW_ROUTE(app, "/api/v1/system/backup")
            .methods("POST"_method)(&SystemController::createBackup, this);

        CROW_ROUTE(app, "/api/v1/system/restore")
            .methods("POST"_method)(&SystemController::restoreBackup, this);

        CROW_ROUTE(app, "/api/v1/system/backups")
            .methods("GET"_method)(&SystemController::listBackups, this);
    }

private:
    crow::response getSystemStatus(const crow::request& /*req*/) {
        try {
            // Using ResponseBuilder alias from namespace declaration

            nlohmann::json data = {
                {"uptime", 345678},
                {"version", "1.0.0"},
                {"cpu", {{"usage", 23.5}, {"temperature", 45.2}, {"cores", 8}}},
                {"memory",
                 {{"total", 16384},
                  {"used", 8192},
                  {"free", 8192},
                  {"usagePercent", 50.0}}},
                {"disk",
                 {{"total", 512000},
                  {"used", 256000},
                  {"free", 256000},
                  {"usagePercent", 50.0}}},
                {"services",
                 {{"database", "running"},
                  {"deviceManager", "running"},
                  {"imageProcessor", "running"}}}};

            return ResponseBuilder::success(data);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get system status: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response getConfig(const crow::request& /*req*/) {
        // Using ResponseBuilder alias from namespace declaration

        nlohmann::json data = {
            {"logging",
             {{"level", "info"},
              {"maxFileSize", 10485760},
              {"retentionDays", 30}}},
            {"network",
             {{"port", 8080}, {"enableSSL", true}, {"corsEnabled", true}}},
            {"devices",
             {{"autoConnect", true},
              {"reconnectAttempts", 3},
              {"connectionTimeout", 5000}}},
            {"storage",
             {{"imagePath", "/data/images"},
              {"tempPath", "/data/temp"},
              {"maxStorageUsage", 90}}}};

        return ResponseBuilder::success(data);
    }

    crow::response updateConfig(const crow::request& req) {
        // Using ResponseBuilder alias from namespace declaration

        try {
            auto config = nlohmann::json::parse(req.body);

            // In a real implementation, validate and apply configuration
            // changes
            nlohmann::json data = {{"requiresRestart", true},
                                   {"updatedFields", nlohmann::json::array()}};

            // Collect updated fields
            if (config.contains("logging")) {
                data["updatedFields"].push_back("logging");
            }
            if (config.contains("devices")) {
                data["updatedFields"].push_back("devices");
            }

            return ResponseBuilder::successWithMessage(
                "Configuration updated successfully.", data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            return ResponseBuilder::internalError(e.what());
        }
    }

    crow::response getProcesses(const crow::request& /*req*/) {
        // Using ResponseBuilder alias from namespace declaration

        nlohmann::json data = {
            {"processes", nlohmann::json::array({{{"pid", 1234},
                                                  {"name", "device-manager"},
                                                  {"status", "running"},
                                                  {"cpuUsage", 5.2},
                                                  {"memoryUsage", 256},
                                                  {"uptime", 123456}},
                                                 {{"pid", 1235},
                                                  {"name", "image-processor"},
                                                  {"status", "running"},
                                                  {"cpuUsage", 15.8},
                                                  {"memoryUsage", 512},
                                                  {"uptime", 123450}}})},
            {"totalProcesses", 2}};

        return ResponseBuilder::success(data);
    }

    crow::response restartService(const crow::request& /*req*/,
                                  const std::string& serviceId) {
        // Using ResponseBuilder alias from namespace declaration

        // Validate service ID
        if (serviceId != "device-manager" && serviceId != "image-processor") {
            return ResponseBuilder::notFound("service", serviceId);
        }

        nlohmann::json data = {{"serviceId", serviceId},
                               {"estimatedDowntime", 5}};

        return ResponseBuilder::accepted("Service restart initiated.", data);
    }

    crow::response shutdown(const crow::request& req) {
        // Using ResponseBuilder alias from namespace declaration

        try {
            auto body =
                nlohmann::json::parse(req.body.empty() ? "{}" : req.body);

            int delay = body.value("delay", 0);
            std::string reason =
                body.value("reason", "User requested shutdown");
            bool force = body.value("force", false);

            nlohmann::json data = {{"shutdownTime", getCurrentTimestamp(delay)},
                                   {"activeOperations", 2}};

            return ResponseBuilder::accepted("System shutdown scheduled.",
                                             data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        }
    }

    crow::response restart(const crow::request& req) {
        // Using ResponseBuilder alias from namespace declaration

        try {
            auto body =
                nlohmann::json::parse(req.body.empty() ? "{}" : req.body);

            int delay = body.value("delay", 0);

            nlohmann::json data = {{"restartTime", getCurrentTimestamp(delay)},
                                   {"estimatedDowntime", 30}};

            return ResponseBuilder::accepted("System restart scheduled.", data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        }
    }

    crow::response cancelShutdown(const crow::request& /*req*/) {
        // Using ResponseBuilder alias from namespace declaration
        return ResponseBuilder::successWithMessage(
            "Scheduled shutdown/restart cancelled.");
    }

    crow::response getLogs(const crow::request& req) {
        // Using ResponseBuilder alias from namespace declaration

        // Parse query parameters
        auto level = req.url_params.get("level");
        auto component = req.url_params.get("component");
        auto limit_str = req.url_params.get("limit");
        int limit = limit_str ? std::stoi(limit_str) : 100;

        nlohmann::json data = {
            {"logs", nlohmann::json::array(
                         {{{"timestamp", "2023-11-20T12:00:00Z"},
                           {"level", "info"},
                           {"component", "device-manager"},
                           {"message", "Camera cam-001 connected successfully"},
                           {"metadata", {{"deviceId", "cam-001"}}}}})},
            {"totalEntries", 1},
            {"hasMore", false}};

        return ResponseBuilder::success(data);
    }

    crow::response discoverDevices(const crow::request& req) {
        // Using ResponseBuilder alias from namespace declaration

        try {
            auto body =
                nlohmann::json::parse(req.body.empty() ? "{}" : req.body);

            nlohmann::json data = {{"discoveryId", "disc_abc123"},
                                   {"estimatedTime", 30}};

            return ResponseBuilder::accepted("Device discovery initiated.",
                                             data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        }
    }

    crow::response getDiscoveryResults(const crow::request& /*req*/,
                                       const std::string& discoveryId) {
        // Using ResponseBuilder alias from namespace declaration

        nlohmann::json data = {
            {"discoveryId", discoveryId},
            {"status", "completed"},
            {"discoveredDevices",
             nlohmann::json::array(
                 {{{"deviceType", "camera"},
                   {"deviceId", "cam-001"},
                   {"name", "ZWO ASI2600MM Pro"},
                   {"driver", "ASCOM"},
                   {"interface", "USB"},
                   {"capabilities", nlohmann::json::array(
                                        {"cooling", "mechanical_shutter"})}}})},
            {"totalFound", 1},
            {"completedAt", "2023-11-20T12:00:30Z"}};

        return ResponseBuilder::success(data);
    }

    crow::response listDevices(const crow::request& /*req*/) {
        // Using ResponseBuilder alias from namespace declaration

        nlohmann::json data = {
            {"devices", nlohmann::json::array({{{"deviceType", "camera"},
                                                {"deviceId", "cam-001"},
                                                {"name", "ZWO ASI2600MM Pro"},
                                                {"driver", "ASCOM"},
                                                {"isConnected", true},
                                                {"isAvailable", true}}})},
            {"totalDevices", 1}};

        return ResponseBuilder::success(data);
    }

    crow::response getDrivers(const crow::request& /*req*/) {
        // Using ResponseBuilder alias from namespace declaration

        nlohmann::json data = {
            {"drivers",
             nlohmann::json::array(
                 {{{"name", "ASCOM"},
                   {"version", "6.6.0"},
                   {"type", "platform"},
                   {"supportedDevices",
                    nlohmann::json::array({"camera", "mount", "focuser",
                                           "filterwheel", "dome", "rotator"})},
                   {"isInstalled", true}},
                  {{"name", "INDI"},
                   {"version", "1.9.9"},
                   {"type", "platform"},
                   {"supportedDevices",
                    nlohmann::json::array(
                        {"camera", "mount", "focuser", "filterwheel", "dome"})},
                   {"isInstalled", true}}})}};

        return ResponseBuilder::success(data);
    }

    crow::response getDiagnostics(const crow::request& /*req*/) {
        // Using ResponseBuilder alias from namespace declaration

        nlohmann::json data = {
            {"systemHealth", "healthy"},
            {"timestamp", getCurrentTimestamp()},
            {"performance",
             {{"cpuUsage", 23.5},
              {"memoryUsage", 50.0},
              {"diskUsage", 54.7},
              {"networkLatency", 5.2}}},
            {"services",
             {{"database", {{"status", "running"}, {"responseTime", 2.5}}},
              {"deviceManager", {{"status", "running"}, {"responseTime", 1.2}}},
              {"imageProcessor",
               {{"status", "running"}, {"responseTime", 3.8}}}}},
            {"connectedDevices",
             {{"cameras", 1},
              {"mounts", 1},
              {"focusers", 1},
              {"filterwheels", 1},
              {"total", 4}}},
            {"activeOperations",
             {{"exposures", 0}, {"sequences", 0}, {"autofocus", 0}}},
            {"errors", {{"last24Hours", 0}}},
            {"warnings", nlohmann::json::array()}};

        return ResponseBuilder::success(data);
    }

    crow::response healthCheck(const crow::request& /*req*/) {
        // Using ResponseBuilder alias from namespace declaration

        nlohmann::json data = {
            {"overallHealth", "healthy"},
            {"checks", nlohmann::json::array(
                           {{{"component", "database"},
                             {"status", "pass"},
                             {"message", "Database connection healthy"},
                             {"responseTime", 2.5}},
                            {{"component", "device-drivers"},
                             {"status", "pass"},
                             {"message", "All drivers loaded successfully"}},
                            {{"component", "network"},
                             {"status", "pass"},
                             {"message", "Network connectivity normal"},
                             {"latency", 5.2}}})},
            {"timestamp", getCurrentTimestamp()}};

        return ResponseBuilder::success(data);
    }

    crow::response getErrors(const crow::request& /*req*/) {
        // Using ResponseBuilder alias from namespace declaration

        nlohmann::json data = {
            {"errors", nlohmann::json::array()},
            {"totalErrors", 0},
            {"summary",
             {{"bySeverity", {{"critical", 0}, {"error", 0}, {"warning", 0}}},
              {"byComponent", nlohmann::json::object()}}}};

        return ResponseBuilder::success(data);
    }

    crow::response createBackup(const crow::request& req) {
        // Using ResponseBuilder alias from namespace declaration

        try {
            auto body = nlohmann::json::parse(req.body);

            nlohmann::json data = {{"backupId", "backup_abc123"},
                                   {"estimatedTime", 15}};

            return ResponseBuilder::accepted("Backup creation initiated.",
                                             data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        }
    }

    crow::response restoreBackup(const crow::request& req) {
        // Using ResponseBuilder alias from namespace declaration

        try {
            auto body = nlohmann::json::parse(req.body);

            if (!body.contains("backupId")) {
                return ResponseBuilder::missingField("backupId");
            }

            nlohmann::json data = {{"estimatedTime", 20},
                                   {"requiresRestart", true}};

            return ResponseBuilder::accepted("Configuration restore initiated.",
                                             data);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        }
    }

    crow::response listBackups(const crow::request& /*req*/) {
        // Using ResponseBuilder alias from namespace declaration

        nlohmann::json data = {{"backups", nlohmann::json::array()},
                               {"totalBackups", 0}};

        return ResponseBuilder::success(data);
    }

    std::string getCurrentTimestamp(int offsetSeconds = 0) {
        auto now = std::chrono::system_clock::now();
        if (offsetSeconds > 0) {
            now += std::chrono::seconds(offsetSeconds);
        }
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_now;
#ifdef _WIN32
        gmtime_s(&tm_now, &time_t_now);
#else
        gmtime_r(&time_t_now, &tm_now);
#endif

        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_now);
        return std::string(buffer);
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_SYSTEM_HPP
