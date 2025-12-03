/*
 * server.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-02

Description: Server status data models for HTTP/WebSocket responses

**************************************************/

#ifndef LITHIUM_SERVER_MODELS_SERVER_HPP
#define LITHIUM_SERVER_MODELS_SERVER_HPP

#include <string>

#include "atom/type/json.hpp"

namespace lithium::models::server {

using json = nlohmann::json;

/**
 * @brief Server health status
 */
struct HealthStatus {
    std::string status;  ///< "healthy", "degraded", "unhealthy"
    std::string timestamp;
    int64_t uptimeSeconds;

    struct ComponentStatus {
        bool available;
        bool running;
        std::string message;

        [[nodiscard]] auto toJson() const -> json {
            json j;
            j["available"] = available;
            if (available) {
                j["running"] = running;
            }
            if (!message.empty()) {
                j["message"] = message;
            }
            return j;
        }
    };

    ComponentStatus websocket;
    ComponentStatus taskManager;
    ComponentStatus eventLoop;

    [[nodiscard]] auto toJson() const -> json {
        return {{"status", status},
                {"timestamp", timestamp},
                {"uptimeSeconds", uptimeSeconds},
                {"websocket", websocket.toJson()},
                {"taskManager", taskManager.toJson()},
                {"eventLoop", eventLoop.toJson()}};
    }
};

/**
 * @brief Server uptime information
 */
struct UptimeInfo {
    int64_t uptimeSeconds;
    std::string uptimeFormatted;
    std::string startTime;

    [[nodiscard]] auto toJson() const -> json {
        return {{"uptimeSeconds", uptimeSeconds},
                {"uptimeFormatted", uptimeFormatted},
                {"startTime", startTime}};
    }

    static auto formatUptime(int64_t seconds) -> std::string {
        int64_t days = seconds / 86400;
        int64_t hours = (seconds % 86400) / 3600;
        int64_t minutes = (seconds % 3600) / 60;
        int64_t secs = seconds % 60;

        std::string result;
        if (days > 0) {
            result += std::to_string(days) + "d ";
        }
        result += std::to_string(hours) + "h ";
        result += std::to_string(minutes) + "m ";
        result += std::to_string(secs) + "s";
        return result;
    }
};

/**
 * @brief Server configuration
 */
struct ServerConfig {
    int port;
    int threadCount;
    bool enableCors;
    bool enableLogging;
    std::string version;

    [[nodiscard]] auto toJson() const -> json {
        return {{"port", port},
                {"threadCount", threadCount},
                {"enableCors", enableCors},
                {"enableLogging", enableLogging},
                {"version", version}};
    }
};

/**
 * @brief Server statistics
 */
struct ServerStats {
    int64_t uptimeSeconds;
    std::string timestamp;
    json websocketStats;
    json taskStats;

    [[nodiscard]] auto toJson() const -> json {
        return {{"uptimeSeconds", uptimeSeconds},
                {"timestamp", timestamp},
                {"websocket", websocketStats},
                {"tasks", taskStats}};
    }
};

/**
 * @brief System resource usage
 */
struct ResourceUsage {
    struct CpuInfo {
        double usage;
        double temperature;
        int cores;

        [[nodiscard]] auto toJson() const -> json {
            return {{"usage", usage},
                    {"temperature", temperature},
                    {"cores", cores}};
        }
    };

    struct MemoryInfo {
        size_t total;
        size_t used;
        size_t free;
        double usagePercent;

        [[nodiscard]] auto toJson() const -> json {
            return {{"total", total},
                    {"used", used},
                    {"free", free},
                    {"usagePercent", usagePercent}};
        }
    };

    struct DiskInfo {
        size_t total;
        size_t used;
        size_t free;
        double usagePercent;

        [[nodiscard]] auto toJson() const -> json {
            return {{"total", total},
                    {"used", used},
                    {"free", free},
                    {"usagePercent", usagePercent}};
        }
    };

    CpuInfo cpu;
    MemoryInfo memory;
    DiskInfo disk;

    [[nodiscard]] auto toJson() const -> json {
        return {{"cpu", cpu.toJson()},
                {"memory", memory.toJson()},
                {"disk", disk.toJson()}};
    }
};

}  // namespace lithium::models::server

#endif  // LITHIUM_SERVER_MODELS_SERVER_HPP
