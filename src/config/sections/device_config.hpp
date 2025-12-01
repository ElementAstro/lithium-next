/*
 * device_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: Unified device configuration

**************************************************/

#ifndef LITHIUM_CONFIG_SECTIONS_DEVICE_CONFIG_HPP
#define LITHIUM_CONFIG_SECTIONS_DEVICE_CONFIG_HPP

#include <chrono>
#include <string>
#include <vector>

#include "../core/config_section.hpp"

namespace lithium::config {

/**
 * @brief Retry strategy enumeration
 */
enum class RetryStrategy {
    None,        ///< No retry
    Linear,      ///< Fixed delay between retries
    Exponential  ///< Exponential backoff
};

/**
 * @brief Convert RetryStrategy to string
 */
[[nodiscard]] inline std::string retryStrategyToString(RetryStrategy strategy) {
    switch (strategy) {
        case RetryStrategy::None: return "none";
        case RetryStrategy::Linear: return "linear";
        case RetryStrategy::Exponential: return "exponential";
    }
    return "exponential";
}

/**
 * @brief Convert string to RetryStrategy
 */
[[nodiscard]] inline RetryStrategy retryStrategyFromString(const std::string& str) {
    if (str == "none") return RetryStrategy::None;
    if (str == "linear") return RetryStrategy::Linear;
    if (str == "exponential") return RetryStrategy::Exponential;
    return RetryStrategy::Exponential;
}

/**
 * @brief Retry configuration
 */
struct RetryConfig {
    std::string strategy{"exponential"};  ///< Retry strategy: none, linear, exponential
    int maxRetries{3};                    ///< Maximum retry attempts
    size_t initialDelayMs{100};           ///< Initial delay in milliseconds
    size_t maxDelayMs{5000};              ///< Maximum delay in milliseconds
    float multiplier{2.0f};               ///< Multiplier for exponential backoff

    [[nodiscard]] json toJson() const {
        return {
            {"strategy", strategy},
            {"maxRetries", maxRetries},
            {"initialDelayMs", initialDelayMs},
            {"maxDelayMs", maxDelayMs},
            {"multiplier", multiplier}
        };
    }

    [[nodiscard]] static RetryConfig fromJson(const json& j) {
        RetryConfig cfg;
        cfg.strategy = j.value("strategy", cfg.strategy);
        cfg.maxRetries = j.value("maxRetries", cfg.maxRetries);
        cfg.initialDelayMs = j.value("initialDelayMs", cfg.initialDelayMs);
        cfg.maxDelayMs = j.value("maxDelayMs", cfg.maxDelayMs);
        cfg.multiplier = j.value("multiplier", cfg.multiplier);
        return cfg;
    }
};

/**
 * @brief Health monitoring configuration
 */
struct HealthMonitorConfig {
    bool enabled{true};                   ///< Enable health monitoring
    size_t checkIntervalSeconds{30};      ///< Health check interval
    float warningThreshold{0.5f};         ///< Health warning threshold (0.0-1.0)
    float criticalThreshold{0.2f};        ///< Health critical threshold (0.0-1.0)
    int maxConsecutiveErrors{5};          ///< Max errors before marking unhealthy
    bool autoReconnect{true};             ///< Auto-reconnect on failure

    [[nodiscard]] json toJson() const {
        return {
            {"enabled", enabled},
            {"checkIntervalSeconds", checkIntervalSeconds},
            {"warningThreshold", warningThreshold},
            {"criticalThreshold", criticalThreshold},
            {"maxConsecutiveErrors", maxConsecutiveErrors},
            {"autoReconnect", autoReconnect}
        };
    }

    [[nodiscard]] static HealthMonitorConfig fromJson(const json& j) {
        HealthMonitorConfig cfg;
        cfg.enabled = j.value("enabled", cfg.enabled);
        cfg.checkIntervalSeconds = j.value("checkIntervalSeconds", cfg.checkIntervalSeconds);
        cfg.warningThreshold = j.value("warningThreshold", cfg.warningThreshold);
        cfg.criticalThreshold = j.value("criticalThreshold", cfg.criticalThreshold);
        cfg.maxConsecutiveErrors = j.value("maxConsecutiveErrors", cfg.maxConsecutiveErrors);
        cfg.autoReconnect = j.value("autoReconnect", cfg.autoReconnect);
        return cfg;
    }
};

/**
 * @brief INDI server configuration
 */
struct IndiConfig {
    bool enabled{true};                 ///< Enable INDI support
    std::string host{"localhost"};      ///< INDI server host
    int port{7624};                     ///< INDI server port
    size_t connectionTimeoutMs{10000};  ///< Connection timeout
    size_t responseTimeoutMs{5000};     ///< Response timeout
    bool autoStartServer{false};        ///< Auto-start INDI server
    std::string serverPath{"/usr/bin/indiserver"};  ///< Path to indiserver
    std::vector<std::string> defaultDrivers;        ///< Default drivers to load

    [[nodiscard]] json toJson() const {
        return {
            {"enabled", enabled},
            {"host", host},
            {"port", port},
            {"connectionTimeoutMs", connectionTimeoutMs},
            {"responseTimeoutMs", responseTimeoutMs},
            {"autoStartServer", autoStartServer},
            {"serverPath", serverPath},
            {"defaultDrivers", defaultDrivers}
        };
    }

    [[nodiscard]] static IndiConfig fromJson(const json& j) {
        IndiConfig cfg;
        cfg.enabled = j.value("enabled", cfg.enabled);
        cfg.host = j.value("host", cfg.host);
        cfg.port = j.value("port", cfg.port);
        cfg.connectionTimeoutMs = j.value("connectionTimeoutMs", cfg.connectionTimeoutMs);
        cfg.responseTimeoutMs = j.value("responseTimeoutMs", cfg.responseTimeoutMs);
        cfg.autoStartServer = j.value("autoStartServer", cfg.autoStartServer);
        cfg.serverPath = j.value("serverPath", cfg.serverPath);
        if (j.contains("defaultDrivers") && j["defaultDrivers"].is_array()) {
            cfg.defaultDrivers = j["defaultDrivers"].get<std::vector<std::string>>();
        }
        return cfg;
    }
};

/**
 * @brief Unified device configuration
 *
 * Consolidates device manager, retry, health monitoring, and INDI settings.
 */
struct DeviceConfig : ConfigSection<DeviceConfig> {
    /// Configuration path
    static constexpr std::string_view PATH = "/lithium/device";

    // ========================================================================
    // General Settings
    // ========================================================================

    size_t maxDevicesPerType{10};         ///< Maximum devices per type
    size_t connectionTimeoutSeconds{10};  ///< Default connection timeout
    size_t operationTimeoutSeconds{30};   ///< Default operation timeout
    bool enableAutoDiscovery{true};       ///< Enable device auto-discovery
    bool enableEventLogging{true};        ///< Log device events

    // ========================================================================
    // Retry Configuration
    // ========================================================================

    RetryConfig retry;

    // ========================================================================
    // Health Monitoring
    // ========================================================================

    HealthMonitorConfig health;

    // ========================================================================
    // INDI Configuration
    // ========================================================================

    IndiConfig indi;

    // ========================================================================
    // Device Type Specific Settings
    // ========================================================================

    struct CameraSettings {
        size_t defaultExposureTimeoutMs{300000};  ///< 5 minutes default
        bool enableCoolerOnConnect{false};
        int defaultBinning{1};

        [[nodiscard]] json toJson() const {
            return {
                {"defaultExposureTimeoutMs", defaultExposureTimeoutMs},
                {"enableCoolerOnConnect", enableCoolerOnConnect},
                {"defaultBinning", defaultBinning}
            };
        }

        [[nodiscard]] static CameraSettings fromJson(const json& j) {
            CameraSettings cfg;
            cfg.defaultExposureTimeoutMs = j.value("defaultExposureTimeoutMs", cfg.defaultExposureTimeoutMs);
            cfg.enableCoolerOnConnect = j.value("enableCoolerOnConnect", cfg.enableCoolerOnConnect);
            cfg.defaultBinning = j.value("defaultBinning", cfg.defaultBinning);
            return cfg;
        }
    } camera;

    struct MountSettings {
        bool enableTracking{true};
        bool parkOnDisconnect{false};
        size_t slewTimeoutMs{120000};  ///< 2 minutes

        [[nodiscard]] json toJson() const {
            return {
                {"enableTracking", enableTracking},
                {"parkOnDisconnect", parkOnDisconnect},
                {"slewTimeoutMs", slewTimeoutMs}
            };
        }

        [[nodiscard]] static MountSettings fromJson(const json& j) {
            MountSettings cfg;
            cfg.enableTracking = j.value("enableTracking", cfg.enableTracking);
            cfg.parkOnDisconnect = j.value("parkOnDisconnect", cfg.parkOnDisconnect);
            cfg.slewTimeoutMs = j.value("slewTimeoutMs", cfg.slewTimeoutMs);
            return cfg;
        }
    } mount;

    struct FocuserSettings {
        size_t moveTimeoutMs{60000};  ///< 1 minute
        bool enableTemperatureCompensation{false};

        [[nodiscard]] json toJson() const {
            return {
                {"moveTimeoutMs", moveTimeoutMs},
                {"enableTemperatureCompensation", enableTemperatureCompensation}
            };
        }

        [[nodiscard]] static FocuserSettings fromJson(const json& j) {
            FocuserSettings cfg;
            cfg.moveTimeoutMs = j.value("moveTimeoutMs", cfg.moveTimeoutMs);
            cfg.enableTemperatureCompensation = j.value("enableTemperatureCompensation", cfg.enableTemperatureCompensation);
            return cfg;
        }
    } focuser;

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json serialize() const {
        return {
            // General
            {"maxDevicesPerType", maxDevicesPerType},
            {"connectionTimeoutSeconds", connectionTimeoutSeconds},
            {"operationTimeoutSeconds", operationTimeoutSeconds},
            {"enableAutoDiscovery", enableAutoDiscovery},
            {"enableEventLogging", enableEventLogging},
            // Retry
            {"retry", retry.toJson()},
            // Health
            {"health", health.toJson()},
            // INDI
            {"indi", indi.toJson()},
            // Device types
            {"camera", camera.toJson()},
            {"mount", mount.toJson()},
            {"focuser", focuser.toJson()}
        };
    }

    [[nodiscard]] static DeviceConfig deserialize(const json& j) {
        DeviceConfig cfg;

        // General
        cfg.maxDevicesPerType = j.value("maxDevicesPerType", cfg.maxDevicesPerType);
        cfg.connectionTimeoutSeconds = j.value("connectionTimeoutSeconds", cfg.connectionTimeoutSeconds);
        cfg.operationTimeoutSeconds = j.value("operationTimeoutSeconds", cfg.operationTimeoutSeconds);
        cfg.enableAutoDiscovery = j.value("enableAutoDiscovery", cfg.enableAutoDiscovery);
        cfg.enableEventLogging = j.value("enableEventLogging", cfg.enableEventLogging);

        // Nested configs
        if (j.contains("retry")) {
            cfg.retry = RetryConfig::fromJson(j["retry"]);
        }
        if (j.contains("health")) {
            cfg.health = HealthMonitorConfig::fromJson(j["health"]);
        }
        if (j.contains("indi")) {
            cfg.indi = IndiConfig::fromJson(j["indi"]);
        }
        if (j.contains("camera")) {
            cfg.camera = CameraSettings::fromJson(j["camera"]);
        }
        if (j.contains("mount")) {
            cfg.mount = MountSettings::fromJson(j["mount"]);
        }
        if (j.contains("focuser")) {
            cfg.focuser = FocuserSettings::fromJson(j["focuser"]);
        }

        return cfg;
    }

    [[nodiscard]] static json generateSchema() {
        return {
            {"type", "object"},
            {"properties", {
                // General
                {"maxDevicesPerType", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 100},
                    {"default", 10}
                }},
                {"connectionTimeoutSeconds", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 300},
                    {"default", 10}
                }},
                {"operationTimeoutSeconds", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 3600},
                    {"default", 30}
                }},
                {"enableAutoDiscovery", {{"type", "boolean"}, {"default", true}}},
                {"enableEventLogging", {{"type", "boolean"}, {"default", true}}},
                // Retry
                {"retry", {
                    {"type", "object"},
                    {"properties", {
                        {"strategy", {
                            {"type", "string"},
                            {"enum", {"none", "linear", "exponential"}},
                            {"default", "exponential"}
                        }},
                        {"maxRetries", {
                            {"type", "integer"},
                            {"minimum", 0},
                            {"maximum", 10},
                            {"default", 3}
                        }},
                        {"initialDelayMs", {{"type", "integer"}, {"minimum", 0}, {"default", 100}}},
                        {"maxDelayMs", {{"type", "integer"}, {"minimum", 0}, {"default", 5000}}},
                        {"multiplier", {{"type", "number"}, {"minimum", 1.0}, {"default", 2.0}}}
                    }}
                }},
                // Health
                {"health", {
                    {"type", "object"},
                    {"properties", {
                        {"enabled", {{"type", "boolean"}, {"default", true}}},
                        {"checkIntervalSeconds", {{"type", "integer"}, {"minimum", 1}, {"default", 30}}},
                        {"warningThreshold", {{"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"default", 0.5}}},
                        {"criticalThreshold", {{"type", "number"}, {"minimum", 0}, {"maximum", 1}, {"default", 0.2}}},
                        {"maxConsecutiveErrors", {{"type", "integer"}, {"minimum", 1}, {"default", 5}}},
                        {"autoReconnect", {{"type", "boolean"}, {"default", true}}}
                    }}
                }},
                // INDI
                {"indi", {
                    {"type", "object"},
                    {"properties", {
                        {"enabled", {{"type", "boolean"}, {"default", true}}},
                        {"host", {{"type", "string"}, {"default", "localhost"}}},
                        {"port", {{"type", "integer"}, {"minimum", 1}, {"maximum", 65535}, {"default", 7624}}},
                        {"connectionTimeoutMs", {{"type", "integer"}, {"default", 10000}}},
                        {"responseTimeoutMs", {{"type", "integer"}, {"default", 5000}}},
                        {"autoStartServer", {{"type", "boolean"}, {"default", false}}},
                        {"serverPath", {{"type", "string"}}},
                        {"defaultDrivers", {{"type", "array"}, {"items", {{"type", "string"}}}}}
                    }}
                }}
            }}
        };
    }
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_SECTIONS_DEVICE_CONFIG_HPP
