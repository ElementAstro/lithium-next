/*
 * server_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: Unified server configuration

**************************************************/

#ifndef LITHIUM_CONFIG_SECTIONS_SERVER_CONFIG_HPP
#define LITHIUM_CONFIG_SECTIONS_SERVER_CONFIG_HPP

#include <chrono>
#include <string>

#include "../core/config_section.hpp"

namespace lithium::config {

/**
 * @brief Unified server configuration
 *
 * This configuration consolidates settings for:
 * - HTTP server (Crow)
 * - WebSocket server
 * - Command dispatcher
 *
 * @example
 * ```yaml
 * lithium:
 *   server:
 *     host: 0.0.0.0
 *     port: 8000
 *     maxConnections: 1000
 *     threadPoolSize: 4
 *     enableCompression: true
 *     connectionTimeout: 60
 * ```
 */
struct ServerConfig : ConfigSection<ServerConfig> {
    /// Configuration path
    static constexpr std::string_view PATH = "/lithium/server";

    // ========================================================================
    // Network Settings
    // ========================================================================

    std::string host{"0.0.0.0"};  ///< Server bind host
    int port{8000};               ///< Server port

    // ========================================================================
    // Connection Settings
    // ========================================================================

    size_t maxConnections{1000};       ///< Maximum concurrent connections
    size_t threadPoolSize{4};          ///< Thread pool size for request handling
    bool enableCompression{true};      ///< Enable GZIP compression
    bool enableCors{true};             ///< Enable CORS headers

    // ========================================================================
    // SSL/TLS Settings
    // ========================================================================

    bool enableSsl{false};         ///< Enable SSL/TLS
    std::string sslCertPath;       ///< Path to SSL certificate
    std::string sslKeyPath;        ///< Path to SSL private key
    std::string sslCaPath;         ///< Path to CA certificate (optional)

    // ========================================================================
    // Timeout Settings
    // ========================================================================

    size_t connectionTimeout{60};   ///< Connection timeout in seconds
    size_t readTimeout{30};         ///< Read timeout in seconds
    size_t writeTimeout{30};        ///< Write timeout in seconds

    // ========================================================================
    // WebSocket Settings
    // ========================================================================

    size_t wsMaxPayloadSize{16 * 1024 * 1024};  ///< Max WebSocket payload (16 MB)
    size_t wsMessageQueueSize{1000};             ///< WebSocket message queue size
    size_t wsPingInterval{30};                   ///< WebSocket ping interval (seconds)
    size_t wsPongTimeout{10};                    ///< WebSocket pong timeout (seconds)

    // ========================================================================
    // Command Dispatcher Settings
    // ========================================================================

    size_t cmdMaxHistorySize{100};     ///< Command history size
    size_t cmdDefaultTimeoutMs{5000};  ///< Default command timeout (ms)
    size_t cmdMaxConcurrent{100};      ///< Max concurrent commands
    bool cmdEnablePriority{true};      ///< Enable command priority queue

    // ========================================================================
    // Rate Limiting
    // ========================================================================

    bool enableRateLimit{true};         ///< Enable rate limiting
    size_t rateLimitRequests{100};      ///< Max requests per window
    size_t rateLimitWindowSeconds{60};  ///< Rate limit window in seconds

    // ========================================================================
    // API Key Authentication
    // ========================================================================

    bool enableApiKeyAuth{false};       ///< Enable API key authentication
    std::string apiKeyHeader{"X-API-Key"};  ///< Header name for API key

    // ========================================================================
    // Web Panel
    // ========================================================================

    bool enableWebPanel{true};          ///< Enable web panel
    std::string webPanelPath{"www"};    ///< Path to web panel static files

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json serialize() const {
        return {
            // Network
            {"host", host},
            {"port", port},
            // Connection
            {"maxConnections", maxConnections},
            {"threadPoolSize", threadPoolSize},
            {"enableCompression", enableCompression},
            {"enableCors", enableCors},
            // SSL
            {"enableSsl", enableSsl},
            {"sslCertPath", sslCertPath},
            {"sslKeyPath", sslKeyPath},
            {"sslCaPath", sslCaPath},
            // Timeout
            {"connectionTimeout", connectionTimeout},
            {"readTimeout", readTimeout},
            {"writeTimeout", writeTimeout},
            // WebSocket
            {"wsMaxPayloadSize", wsMaxPayloadSize},
            {"wsMessageQueueSize", wsMessageQueueSize},
            {"wsPingInterval", wsPingInterval},
            {"wsPongTimeout", wsPongTimeout},
            // Command
            {"cmdMaxHistorySize", cmdMaxHistorySize},
            {"cmdDefaultTimeoutMs", cmdDefaultTimeoutMs},
            {"cmdMaxConcurrent", cmdMaxConcurrent},
            {"cmdEnablePriority", cmdEnablePriority},
            // Rate Limit
            {"enableRateLimit", enableRateLimit},
            {"rateLimitRequests", rateLimitRequests},
            {"rateLimitWindowSeconds", rateLimitWindowSeconds},
            // API Key
            {"enableApiKeyAuth", enableApiKeyAuth},
            {"apiKeyHeader", apiKeyHeader},
            // Web Panel
            {"enableWebPanel", enableWebPanel},
            {"webPanelPath", webPanelPath}
        };
    }

    [[nodiscard]] static ServerConfig deserialize(const json& j) {
        ServerConfig cfg;

        // Network
        cfg.host = j.value("host", cfg.host);
        cfg.port = j.value("port", cfg.port);

        // Connection
        cfg.maxConnections = j.value("maxConnections", cfg.maxConnections);
        cfg.threadPoolSize = j.value("threadPoolSize", cfg.threadPoolSize);
        cfg.enableCompression = j.value("enableCompression", cfg.enableCompression);
        cfg.enableCors = j.value("enableCors", cfg.enableCors);

        // SSL
        cfg.enableSsl = j.value("enableSsl", cfg.enableSsl);
        cfg.sslCertPath = j.value("sslCertPath", cfg.sslCertPath);
        cfg.sslKeyPath = j.value("sslKeyPath", cfg.sslKeyPath);
        cfg.sslCaPath = j.value("sslCaPath", cfg.sslCaPath);

        // Timeout
        cfg.connectionTimeout = j.value("connectionTimeout", cfg.connectionTimeout);
        cfg.readTimeout = j.value("readTimeout", cfg.readTimeout);
        cfg.writeTimeout = j.value("writeTimeout", cfg.writeTimeout);

        // WebSocket
        cfg.wsMaxPayloadSize = j.value("wsMaxPayloadSize", cfg.wsMaxPayloadSize);
        cfg.wsMessageQueueSize = j.value("wsMessageQueueSize", cfg.wsMessageQueueSize);
        cfg.wsPingInterval = j.value("wsPingInterval", cfg.wsPingInterval);
        cfg.wsPongTimeout = j.value("wsPongTimeout", cfg.wsPongTimeout);

        // Command
        cfg.cmdMaxHistorySize = j.value("cmdMaxHistorySize", cfg.cmdMaxHistorySize);
        cfg.cmdDefaultTimeoutMs = j.value("cmdDefaultTimeoutMs", cfg.cmdDefaultTimeoutMs);
        cfg.cmdMaxConcurrent = j.value("cmdMaxConcurrent", cfg.cmdMaxConcurrent);
        cfg.cmdEnablePriority = j.value("cmdEnablePriority", cfg.cmdEnablePriority);

        // Rate Limit
        cfg.enableRateLimit = j.value("enableRateLimit", cfg.enableRateLimit);
        cfg.rateLimitRequests = j.value("rateLimitRequests", cfg.rateLimitRequests);
        cfg.rateLimitWindowSeconds = j.value("rateLimitWindowSeconds", cfg.rateLimitWindowSeconds);

        // API Key
        cfg.enableApiKeyAuth = j.value("enableApiKeyAuth", cfg.enableApiKeyAuth);
        cfg.apiKeyHeader = j.value("apiKeyHeader", cfg.apiKeyHeader);

        // Web Panel
        cfg.enableWebPanel = j.value("enableWebPanel", cfg.enableWebPanel);
        cfg.webPanelPath = j.value("webPanelPath", cfg.webPanelPath);

        return cfg;
    }

    [[nodiscard]] static json generateSchema() {
        return {
            {"type", "object"},
            {"properties", {
                // Network
                {"host", {{"type", "string"}, {"default", "0.0.0.0"}}},
                {"port", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 65535},
                    {"default", 8000}
                }},
                // Connection
                {"maxConnections", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 100000},
                    {"default", 1000}
                }},
                {"threadPoolSize", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 128},
                    {"default", 4}
                }},
                {"enableCompression", {{"type", "boolean"}, {"default", true}}},
                {"enableCors", {{"type", "boolean"}, {"default", true}}},
                // SSL
                {"enableSsl", {{"type", "boolean"}, {"default", false}}},
                {"sslCertPath", {{"type", "string"}}},
                {"sslKeyPath", {{"type", "string"}}},
                {"sslCaPath", {{"type", "string"}}},
                // Timeout
                {"connectionTimeout", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 3600},
                    {"default", 60}
                }},
                {"readTimeout", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 3600},
                    {"default", 30}
                }},
                {"writeTimeout", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 3600},
                    {"default", 30}
                }},
                // WebSocket
                {"wsMaxPayloadSize", {
                    {"type", "integer"},
                    {"minimum", 1024},
                    {"default", 16777216}
                }},
                {"wsMessageQueueSize", {
                    {"type", "integer"},
                    {"minimum", 10},
                    {"default", 1000}
                }},
                {"wsPingInterval", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"default", 30}
                }},
                {"wsPongTimeout", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"default", 10}
                }},
                // Command
                {"cmdMaxHistorySize", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"default", 100}
                }},
                {"cmdDefaultTimeoutMs", {
                    {"type", "integer"},
                    {"minimum", 100},
                    {"default", 5000}
                }},
                {"cmdMaxConcurrent", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"default", 100}
                }},
                {"cmdEnablePriority", {{"type", "boolean"}, {"default", true}}},
                // Rate Limit
                {"enableRateLimit", {{"type", "boolean"}, {"default", true}}},
                {"rateLimitRequests", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"default", 100}
                }},
                {"rateLimitWindowSeconds", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"default", 60}
                }},
                // API Key
                {"enableApiKeyAuth", {{"type", "boolean"}, {"default", false}}},
                {"apiKeyHeader", {{"type", "string"}, {"default", "X-API-Key"}}},
                // Web Panel
                {"enableWebPanel", {{"type", "boolean"}, {"default", true}}},
                {"webPanelPath", {{"type", "string"}, {"default", "www"}}}
            }}
        };
    }
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_SECTIONS_SERVER_CONFIG_HPP
