/**
 * @file main.cpp
 * @brief Main entry point for Lithium Astronomical Equipment Control Server
 *
 * This server provides a comprehensive REST API and WebSocket interface for
 * controlling astronomical equipment including cameras, mounts, focusers,
 * filter wheels, domes, and other observatory devices.
 */

#include "atom/log/spdlog_logger.hpp"
#include "main_server.hpp"

#include <csignal>
#include <memory>

// Global server instance for signal handling
std::unique_ptr<lithium::server::MainServer> g_server;

/**
 * @brief Signal handler for graceful shutdown
 */
void signalHandler(int signal) {
    LOG_WARN("Received signal {}, initiating graceful shutdown...", signal);
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    // Logging is now initialized by MainServer via spdlog
    // See main_server.hpp::initializeLogging()

    LOG_INFO("==============================================");
    LOG_INFO("  Lithium Astronomical Equipment Control API  ");
    LOG_INFO("  Version: 1.0.0                             ");
    LOG_INFO("==============================================");

    try {
        // Configure server
        lithium::server::MainServer::Config config;
        config.port = 8080;
        config.thread_count = 4;
        config.enable_cors = true;
        config.enable_logging = true;

        // Add default API keys (in production, load from secure configuration)
        config.api_keys = {"default-api-key-please-change-in-production"};

        // Check for command line arguments
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc) {
                config.port = std::stoi(argv[++i]);
            } else if (arg == "--threads" && i + 1 < argc) {
                config.thread_count = std::stoi(argv[++i]);
            } else if (arg == "--help" || arg == "-h") {
                std::cout
                    << "Usage: " << argv[0] << " [options]\n"
                    << "Options:\n"
                    << "  --port <number>     Server port (default: 8080)\n"
                    << "  --threads <number>  Worker threads (default: 4)\n"
                    << "  --help, -h          Show this help message\n";
                return 0;
            }
        }

        LOG_INFO("Server configuration:");
        LOG_INFO("  Port: {}", config.port);
        LOG_INFO("  Threads: {}", config.thread_count);
        LOG_INFO("  CORS: {}", config.enable_cors ? "enabled" : "disabled");

        // Create server instance
        g_server = std::make_unique<lithium::server::MainServer>(config);

        // Register signal handlers for graceful shutdown
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        LOG_INFO("Server starting...");
        LOG_INFO("REST API available at http://localhost:{}/api/v1",
                 config.port);
        LOG_INFO("WebSocket available at ws://localhost:{}/api/v1/ws",
                 config.port);
        LOG_INFO("API Documentation: http://localhost:{}/api/v1/docs",
                 config.port);
        LOG_INFO("");
        LOG_INFO("Press Ctrl+C to stop the server");
        LOG_INFO("");

        // Start server (blocking call)
        g_server->start();

    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }

    LOG_INFO("Server shutdown complete");
    return 0;
}
