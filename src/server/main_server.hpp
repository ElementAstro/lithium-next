#ifndef LITHIUM_SERVER_MAIN_SERVER_HPP
#define LITHIUM_SERVER_MAIN_SERVER_HPP

#include "app.hpp"
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <asio/io_context.hpp>

#include "atom/log/spdlog_logger.hpp"
#include "atom/async/message_bus.hpp"
#include "atom/type/json.hpp"
#include "middleware/auth.hpp"
#include "websocket.hpp"
#include "eventloop.hpp"
#include "command.hpp"
#include "task_manager.hpp"

// Controllers
#include "controller/controller.hpp"
#include "controller/camera.hpp"
#include "controller/mount.hpp"
#include "controller/focuser.hpp"
#include "controller/filterwheel.hpp"
#include "controller/dome.hpp"
#include "controller/guider.hpp"
#include "controller/switch.hpp"
#include "controller/system.hpp"
#include "controller/filesystem.hpp"
#include "controller/sky.hpp"
#include "controller/components.hpp"
#include "controller/config.hpp"
#include "controller/python.hpp"
#include "controller/script.hpp"
#include "controller/search.hpp"
#include "controller/sequencer/execution.hpp"
#include "controller/sequencer/management.hpp"
#include "controller/sequencer/target.hpp"
#include "controller/sequencer/task.hpp"

// Sequencer core
#include "task/sequencer.hpp"

namespace lithium::server {

/**
 * @brief Main server application class
 * 
 * Integrates all controllers, middleware, WebSocket support, and event handling
 * to provide a complete astronomical equipment control REST API server.
 */
class MainServer {
public:
    /**
     * @brief Server configuration
     */
    struct Config {
        int port = 8080;
        int thread_count = 4;
        bool enable_ssl = false;
        std::string ssl_cert_path;
        std::string ssl_key_path;
        std::vector<std::string> api_keys;
        bool enable_cors = true;
        bool enable_logging = true;
    };

    /**
     * @brief Construct main server with configuration
     */
    explicit MainServer(const Config& config) 
        : config_(config),
          app_(),
          event_loop_(std::make_shared<app::EventLoop>(config.thread_count)),
          exposure_sequence_(std::make_shared<lithium::task::ExposureSequence>()),
          task_manager_(std::make_shared<TaskManager>(event_loop_)) {
        
        LOG_INFO( "Initializing Lithium Server v1.0.0");
        initializeMiddleware();
        initializeControllers();
        initializeWebSocket();
    }

    /**
     * @brief Start the server
     */
    void start() {
        LOG_INFO( "Starting server on port {}", config_.port);
        
        // EventLoop already starts worker threads in its constructor,
        // so we don't call run() here to avoid blocking.

        // Start MessageBus io_context in background thread
        message_bus_thread_ = std::thread([this]() {
            LOG_INFO( "MessageBus io_context running");
            message_bus_io_.run();
            LOG_INFO( "MessageBus io_context stopped");
        });

        // Start WebSocket server if initialized
        if (websocket_server_) {
            websocket_server_->start();
        }

        // Configure Crow app
        app_.port(config_.port)
            .multithreaded()
            .run();
    }

    /**
     * @brief Stop the server gracefully
     */
    void stop() {
        LOG_INFO( "Stopping server...");
        
        if (websocket_server_) {
            websocket_server_->stop();
        }
        
        // Stop MessageBus io_context and join thread
        message_bus_io_.stop();
        if (message_bus_thread_.joinable()) {
            message_bus_thread_.join();
        }

        event_loop_->stop();
        app_.stop();
        
        LOG_INFO( "Server stopped");
    }

    /**
     * @brief Get the HTTP application instance
     */
    ServerApp& getApp() {
        return app_;
    }

    /**
     * @brief Get the event loop
     */
    std::shared_ptr<app::EventLoop> getEventLoop() {
        return event_loop_;
    }

    /**
     * @brief Add an API key for authentication
     */
    void addApiKey(const std::string& key) {
        middleware::ApiKeyAuth::addApiKey(key);
        LOG_INFO( "API key added");
    }

    /**
     * @brief Revoke an API key
     */
    void revokeApiKey(const std::string& key) {
        middleware::ApiKeyAuth::revokeApiKey(key);
        LOG_INFO( "API key revoked");
    }

private:
    Config config_;
    ServerApp app_;
    std::shared_ptr<app::EventLoop> event_loop_;
    std::shared_ptr<lithium::task::ExposureSequence> exposure_sequence_;
    std::shared_ptr<TaskManager> task_manager_;
    std::shared_ptr<WebSocketServer> websocket_server_;
    std::vector<std::unique_ptr<Controller>> controllers_;
    asio::io_context message_bus_io_;
    std::thread message_bus_thread_;

    /**
     * @brief Initialize middleware components
     */
    void initializeMiddleware() {
        LOG_INFO( "Initializing middleware...");
        
        // Add default API keys from config
        for (const auto& key : config_.api_keys) {
            middleware::ApiKeyAuth::addApiKey(key);
        }

        // CORS is handled by middleware in individual routes
        // Authentication is handled by ApiKeyAuth middleware
        
        LOG_INFO( "Middleware initialized");
    }

    /**
     * @brief Initialize all controllers and register their routes
     */
    void initializeControllers() {
        LOG_INFO( "Initializing controllers...");

        // Inject shared ExposureSequence instance into sequencer controllers
        SequenceExecutionController::setExposureSequence(exposure_sequence_);
        SequenceManagementController::setExposureSequence(exposure_sequence_);
        TargetController::setExposureSequence(exposure_sequence_);

        // Inject TaskManager into task controller
        TaskManagementController::setTaskManager(task_manager_);

        // Register root endpoint
        CROW_ROUTE(app_, "/")
            ([]() {
                return crow::response(200, R"({
                    "status": "success",
                    "message": "Lithium Astronomical Equipment Control API v1.0.0",
                    "documentation": "/api/v1/docs"
                })");
            });

        // API version info endpoint
        CROW_ROUTE(app_, "/api/v1")
            ([]() {
                return crow::response(200, R"({
                    "status": "success",
                    "version": "1.0.0",
                    "endpoints": {
                        "cameras": "/api/v1/cameras",
                        "mounts": "/api/v1/mounts",
                        "focusers": "/api/v1/focusers",
                        "filterwheels": "/api/v1/filterwheels",
                        "domes": "/api/v1/domes",
                        "system": "/api/v1/system",
                        "filesystem": "/api/v1/filesystem",
                        "sky": "/api/v1/sky"
                    }
                })");
            });

        // Create and register controllers
        controllers_.push_back(std::make_unique<CameraController>());
        controllers_.push_back(std::make_unique<MountController>());
        controllers_.push_back(std::make_unique<FocuserController>());
        controllers_.push_back(std::make_unique<FilterWheelController>());
        controllers_.push_back(std::make_unique<controller::SystemController>());
        controllers_.push_back(std::make_unique<controller::FilesystemController>());
        controllers_.push_back(std::make_unique<controller::SkyController>());
        controllers_.push_back(std::make_unique<controller::DomeController>());
        controllers_.push_back(std::make_unique<GuiderController>());
        controllers_.push_back(std::make_unique<SwitchController>());
        controllers_.push_back(std::make_unique<ModuleController>());
        controllers_.push_back(std::make_unique<ConfigController>());
        controllers_.push_back(std::make_unique<PythonController>());
        controllers_.push_back(std::make_unique<ScriptController>());
        controllers_.push_back(std::make_unique<SearchController>());
        controllers_.push_back(std::make_unique<SequenceExecutionController>());
        controllers_.push_back(std::make_unique<SequenceManagementController>());
        controllers_.push_back(std::make_unique<TargetController>());
        controllers_.push_back(std::make_unique<TaskManagementController>());

        // Register all controller routes
        for (auto& controller : controllers_) {
            controller->registerRoutes(app_);
        }

        LOG_INFO( "Controllers initialized: {} controllers registered", controllers_.size());
    }

    /**
     * @brief Initialize WebSocket server
     */
    void initializeWebSocket() {
        LOG_INFO( "Initializing WebSocket server...");
        
        // Create message bus for inter-component communication
        auto message_bus = atom::async::MessageBus::createShared(message_bus_io_);
        
        // Create command dispatcher bound to the shared EventLoop
        app::CommandDispatcher::Config dispatcher_config;
        dispatcher_config.maxHistorySize = 100;
        dispatcher_config.defaultTimeout = std::chrono::milliseconds(5000);
        dispatcher_config.maxConcurrentCommands = 100;
        dispatcher_config.enablePriority = true;

        auto command_dispatcher = std::make_shared<app::CommandDispatcher>(
            event_loop_, dispatcher_config);

        // Configure WebSocket
        WebSocketServer::Config ws_config;
        ws_config.max_payload_size = UINT64_MAX;
        ws_config.enable_compression = false;
        ws_config.max_connections = 1000;
        ws_config.thread_pool_size = config_.thread_count;
        ws_config.ping_interval = 30;
        ws_config.connection_timeout = 60;

        // Create WebSocket server
        websocket_server_ = std::make_shared<WebSocketServer>(
            app_, message_bus, command_dispatcher, ws_config);

        LOG_INFO( "WebSocket server initialized at /api/v1/ws");

        // Connect TaskManager status updates to WebSocket events
        if (task_manager_ && websocket_server_) {
            std::weak_ptr<WebSocketServer> wsWeak = websocket_server_;
            task_manager_->setStatusCallback(
                [wsWeak](const TaskManager::TaskInfo& info) {
                    auto ws = wsWeak.lock();
                    if (!ws) {
                        return;
                    }

                    nlohmann::json event;
                    event["type"] = "event";
                    event["event"] = "taskUpdated";

                    nlohmann::json task;
                    task["id"] = info.id;
                    task["taskType"] = info.type;

                    std::string statusStr;
                    switch (info.status) {
                        case TaskManager::Status::Pending:
                            statusStr = "Pending";
                            break;
                        case TaskManager::Status::Running:
                            statusStr = "Running";
                            break;
                        case TaskManager::Status::Completed:
                            statusStr = "Completed";
                            break;
                        case TaskManager::Status::Failed:
                            statusStr = "Failed";
                            break;
                        case TaskManager::Status::Cancelled:
                            statusStr = "Cancelled";
                            break;
                    }
                    task["status"] = statusStr;

                    if (!info.error.empty()) {
                        task["error"] = info.error;
                    }

                    task["cancelRequested"] =
                        info.cancelRequested.load();

                    auto toMs = [](auto tp) {
                        return std::chrono::duration_cast<
                                   std::chrono::milliseconds>(
                                   tp.time_since_epoch())
                            .count();
                    };

                    task["createdAt"] = toMs(info.createdAt);
                    task["updatedAt"] = toMs(info.updatedAt);

                    if (!info.result.is_null()) {
                        task["result"] = info.result;
                    }

                    event["task"] = std::move(task);

                    ws->broadcast(event.dump());
                });
        }
    }
};

}  // namespace lithium::server

#endif  // LITHIUM_SERVER_MAIN_SERVER_HPP
