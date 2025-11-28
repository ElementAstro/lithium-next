/**
 * @file main.cpp
 * @brief Main entry point for Lithium Astronomical Equipment Control Server
 * 
 * This server provides a comprehensive REST API and WebSocket interface for
 * controlling astronomical equipment including cameras, mounts, focusers,
 * filter wheels, domes, and other observatory devices.
 */

#include "main_server.hpp"
#include "atom/log/loguru.hpp"

#include <csignal>
#include <memory>

// Global server instance for signal handling
std::unique_ptr<lithium::server::MainServer> g_server;

/**
 * @brief Signal handler for graceful shutdown
 */
void signalHandler(int signal) {
    LOG_F(WARNING, "Received signal {}, initiating graceful shutdown...", signal);
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("lithium-server.log", loguru::Append, loguru::Verbosity_MAX);
    
    LOG_F(INFO, "==============================================");
    LOG_F(INFO, "  Lithium Astronomical Equipment Control API  ");
    LOG_F(INFO, "  Version: 1.0.0                             ");
    LOG_F(INFO, "==============================================");

    try {
        // Configure server
        lithium::server::MainServer::Config config;
        config.port = 8080;
        config.thread_count = 4;
        config.enable_cors = true;
        config.enable_logging = true;
        
        // Add default API keys (in production, load from secure configuration)
        config.api_keys = {
            "default-api-key-please-change-in-production"
        };

        // Check for command line arguments
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc) {
                config.port = std::stoi(argv[++i]);
            } else if (arg == "--threads" && i + 1 < argc) {
                config.thread_count = std::stoi(argv[++i]);
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: " << argv[0] << " [options]\n"
                          << "Options:\n"
                          << "  --port <number>     Server port (default: 8080)\n"
                          << "  --threads <number>  Worker threads (default: 4)\n"
                          << "  --help, -h          Show this help message\n";
                return 0;
            }
        }

        LOG_F(INFO, "Server configuration:");
        LOG_F(INFO, "  Port: {}", config.port);
        LOG_F(INFO, "  Threads: {}", config.thread_count);
        LOG_F(INFO, "  CORS: {}", config.enable_cors ? "enabled" : "disabled");

        // Create server instance
        g_server = std::make_unique<lithium::server::MainServer>(config);

        // Register signal handlers for graceful shutdown
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        LOG_F(INFO, "Server starting...");
        LOG_F(INFO, "REST API available at http://localhost:{}/api/v1", config.port);
        LOG_F(INFO, "WebSocket available at ws://localhost:{}/api/v1/ws", config.port);
        LOG_F(INFO, "API Documentation: http://localhost:{}/api/v1/docs", config.port);
        LOG_F(INFO, "");
        LOG_F(INFO, "Press Ctrl+C to stop the server");
        LOG_F(INFO, "");

        // Start server (blocking call)
        g_server->start();

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Fatal error: {}", e.what());
        return 1;
    }

    LOG_F(INFO, "Server shutdown complete");
    return 0;
}
