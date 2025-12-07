#ifndef LITHIUM_SERVER_MAIN_SERVER_HPP
#define LITHIUM_SERVER_MAIN_SERVER_HPP

#include <asio/io_context.hpp>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include "app.hpp"

#include "atom/async/message_bus.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "command.hpp"
#include "eventloop.hpp"
#include "middleware/auth.hpp"
#include "task_manager.hpp"
#include "websocket.hpp"

// Base Controller
#include "controller/controller.hpp"

// Device Controllers
#include "controller/device/camera.hpp"
#include "controller/device/device_plugin.hpp"
#include "controller/device/dome.hpp"
#include "controller/device/filterwheel.hpp"
#include "controller/device/focuser.hpp"
#include "controller/device/guider.hpp"
#include "controller/device/mount.hpp"
#include "controller/device/sky.hpp"
#include "controller/device/switch.hpp"

// System Controllers
#include "controller/system/config.hpp"
#include "controller/system/database.hpp"
#include "controller/system/filesystem.hpp"
#include "controller/system/logging.hpp"
#include "controller/system/os.hpp"
#include "controller/system/search.hpp"
#include "controller/system/server_status.hpp"

// Script Controllers
#include "controller/script/isolated.hpp"
#include "controller/script/python.hpp"
#include "controller/script/shell.hpp"
#include "controller/script/tool_registry.hpp"
#include "controller/script/venv.hpp"

// Plugin Controllers
#include "controller/plugin/components.hpp"
#include "controller/plugin/plugin.hpp"

// Sequencer Controllers
#include "controller/sequencer/execution.hpp"
#include "controller/sequencer/management.hpp"
#include "controller/sequencer/target.hpp"
#include "controller/sequencer/task.hpp"

// Plugin system
#include "plugin/plugin_manager.hpp"

// Logging system
#include "logging/logging_manager.hpp"
#include "websocket/log_stream.hpp"

// Sequencer core
#include "task/core/sequencer.hpp"

// Device subsystem
#include "device/device.hpp"

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

        // Logging configuration
        logging::LoggingConfig logging_config;

        // Plugin configuration
        plugin::PluginManagerConfig plugin_config;
        bool enable_plugins = true;
        bool auto_load_plugins = true;

        // Device subsystem configuration
        nlohmann::json device_config;
        bool enable_device_plugins = true;
        bool auto_load_device_plugins = true;
    };

    /**
     * @brief Construct main server with configuration
     */
    explicit MainServer(const Config& config)
        : config_(config),
          app_(),
          event_loop_(std::make_shared<app::EventLoop>(config.thread_count)),
          exposure_sequence_(
              std::make_shared<lithium::task::ExposureSequence>()),
          task_manager_(std::make_shared<TaskManager>(event_loop_)),
          plugin_manager_(
              plugin::PluginManager::createShared(config.plugin_config)) {
        LOG_INFO("Initializing Lithium Server v1.0.0");
        initializeLogging();
        initializeDeviceSubsystem();
        initializeMiddleware();
        initializeControllers();
        initializeWebSocket();
        initializePlugins();
    }

    /**
     * @brief Start the server
     */
    void start() {
        LOG_INFO("Starting server on port {}", config_.port);

        // EventLoop already starts worker threads in its constructor,
        // so we don't call run() here to avoid blocking.

        // Start MessageBus io_context in background thread
        message_bus_thread_ = std::thread([this]() {
            LOG_INFO("MessageBus io_context running");
            message_bus_io_.run();
            LOG_INFO("MessageBus io_context stopped");
        });

        // Start WebSocket server if initialized
        if (websocket_server_) {
            websocket_server_->start();
        }

        // Schedule periodic task cleanup (every 5 minutes, remove tasks older
        // than 1 hour)
        if (task_manager_) {
            cleanup_task_id_ = task_manager_->schedulePeriodicTask(
                "TaskCleanup", std::chrono::minutes(5), [this]() {
                    if (auto tm = task_manager_) {
                        auto removed =
                            tm->cleanupOldTasks(std::chrono::hours(1));
                        if (removed > 0) {
                            LOG_INFO("Periodic cleanup: removed {} old tasks",
                                     removed);
                        }
                    }
                });
            LOG_INFO("Scheduled periodic task cleanup");
        }

        // Configure Crow app
        app_.port(config_.port).multithreaded().run();
    }

    /**
     * @brief Stop the server gracefully
     */
    void stop() {
        LOG_INFO("Stopping server...");

        // Cancel periodic cleanup task
        if (task_manager_ && !cleanup_task_id_.empty()) {
            task_manager_->cancelPeriodicTask(cleanup_task_id_);
        }

        // Stop log streaming first
        LogStreamManager::getInstance().shutdown();

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

        // Shutdown device subsystem
        lithium::shutdownDeviceSubsystem();

        // Shutdown logging last to capture all shutdown messages
        logging::LoggingManager::getInstance().shutdown();

        LOG_INFO("Server stopped");
    }

    /**
     * @brief Get the HTTP application instance
     */
    ServerApp& getApp() { return app_; }

    /**
     * @brief Get the event loop
     */
    std::shared_ptr<app::EventLoop> getEventLoop() { return event_loop_; }

    /**
     * @brief Add an API key for authentication
     */
    void addApiKey(const std::string& key) {
        middleware::ApiKeyAuth::addApiKey(key);
        LOG_INFO("API key added");
    }

    /**
     * @brief Revoke an API key
     */
    void revokeApiKey(const std::string& key) {
        middleware::ApiKeyAuth::revokeApiKey(key);
        LOG_INFO("API key revoked");
    }

private:
    Config config_;
    ServerApp app_;
    std::shared_ptr<app::EventLoop> event_loop_;
    std::shared_ptr<lithium::task::ExposureSequence> exposure_sequence_;
    std::shared_ptr<TaskManager> task_manager_;
    std::shared_ptr<WebSocketServer> websocket_server_;
    std::shared_ptr<plugin::PluginManager> plugin_manager_;
    std::shared_ptr<app::CommandDispatcher> command_dispatcher_;
    std::vector<std::unique_ptr<Controller>> controllers_;
    asio::io_context message_bus_io_;
    std::thread message_bus_thread_;
    std::string cleanup_task_id_;  ///< ID of the periodic cleanup task

    /**
     * @brief Initialize logging system
     */
    void initializeLogging() {
        LOG_INFO("Initializing logging system...");

        // Setup default logging configuration if not provided
        logging::LoggingConfig log_config = config_.logging_config;

        // Add default sinks if none configured
        if (log_config.sinks.empty()) {
            // Console sink
            logging::SinkConfig console_sink;
            console_sink.name = "console";
            console_sink.type = "console";
            console_sink.level = spdlog::level::info;
            log_config.sinks.push_back(console_sink);

            // Rotating file sink
            logging::SinkConfig file_sink;
            file_sink.name = "file";
            file_sink.type = "rotating_file";
            file_sink.level = spdlog::level::debug;
            file_sink.file_path = "logs/lithium-server.log";
            file_sink.max_file_size = 10 * 1024 * 1024;  // 10MB
            file_sink.max_files = 5;
            log_config.sinks.push_back(file_sink);
        }

        // Initialize logging manager
        logging::LoggingManager::getInstance().initialize(log_config);

        // Initialize log stream manager for WebSocket streaming
        LogStreamManager::getInstance().initialize();

        LOG_INFO("Logging system initialized with {} sinks",
                 log_config.sinks.size());
    }

    /**
     * @brief Initialize device subsystem
     */
    void initializeDeviceSubsystem() {
        LOG_INFO("Initializing device subsystem...");

        // Initialize the device subsystem with configuration
        if (!lithium::initializeDeviceSubsystem(config_.device_config)) {
            LOG_ERROR("Failed to initialize device subsystem");
            return;
        }

        // Set up MessageBus integration for device events
        auto message_bus =
            atom::async::MessageBus::createShared(message_bus_io_);
        device::DeviceEventBus::getInstance().setMessageBus(message_bus);

        // Auto-load device plugins if configured
        if (config_.enable_device_plugins && config_.auto_load_device_plugins) {
            auto& loader = device::DevicePluginLoader::getInstance();
            size_t loaded = loader.loadAllPlugins();
            LOG_INFO("Auto-loaded {} device plugins", loaded);
        }

        // Subscribe to device events for logging
        device::DeviceEventBus::getInstance().subscribe(
            device::DeviceEventType::PluginLoaded,
            [](const device::DeviceEvent& event) {
                LOG_INFO("Device plugin loaded: {}", event.deviceName);
            });

        device::DeviceEventBus::getInstance().subscribe(
            device::DeviceEventType::PluginUnloaded,
            [](const device::DeviceEvent& event) {
                LOG_INFO("Device plugin unloaded: {}", event.deviceName);
            });

        device::DeviceEventBus::getInstance().subscribe(
            device::DeviceEventType::Error,
            [](const device::DeviceEvent& event) {
                LOG_ERROR("Device error on {}: {}", event.deviceName,
                          event.message);
            });

        LOG_INFO("Device subsystem initialized");
    }

    /**
     * @brief Initialize middleware components
     */
    void initializeMiddleware() {
        LOG_INFO("Initializing middleware...");

        // Add default API keys from config
        for (const auto& key : config_.api_keys) {
            middleware::ApiKeyAuth::addApiKey(key);
        }

        // CORS is handled by middleware in individual routes
        // Authentication is handled by ApiKeyAuth middleware

        LOG_INFO("Middleware initialized");
    }

    /**
     * @brief Initialize all controllers and register their routes
     */
    void initializeControllers() {
        LOG_INFO("Initializing controllers...");

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
                        "sky": "/api/v1/sky",
                        "device-plugins": "/api/v1/device-plugins",
                        "device-types": "/api/v1/device-types",
                        "device-events": "/api/v1/device-events"
                    }
                })");
        });

        // Create and register controllers
        // All controllers are now in lithium::server::controller namespace
        controllers_.push_back(
            std::make_unique<controller::CameraController>());
        controllers_.push_back(std::make_unique<controller::MountController>());
        controllers_.push_back(
            std::make_unique<controller::FocuserController>());
        controllers_.push_back(
            std::make_unique<controller::FilterWheelController>());
        controllers_.push_back(
            std::make_unique<controller::SystemController>());
        controllers_.push_back(
            std::make_unique<controller::FilesystemController>());
        controllers_.push_back(std::make_unique<controller::SkyController>());
        controllers_.push_back(std::make_unique<controller::DomeController>());
        controllers_.push_back(
            std::make_unique<controller::GuiderController>());
        controllers_.push_back(
            std::make_unique<controller::SwitchController>());
        controllers_.push_back(
            std::make_unique<controller::ModuleController>());
        controllers_.push_back(
            std::make_unique<controller::ConfigController>());
        controllers_.push_back(
            std::make_unique<controller::DatabaseController>());
        controllers_.push_back(
            std::make_unique<controller::PythonController>());
        controllers_.push_back(
            std::make_unique<controller::ScriptController>());
        controllers_.push_back(
            std::make_unique<controller::SearchController>());
        controllers_.push_back(
            std::make_unique<controller::SequenceExecutionController>());
        controllers_.push_back(
            std::make_unique<controller::SequenceManagementController>());
        controllers_.push_back(
            std::make_unique<controller::TargetController>());
        controllers_.push_back(
            std::make_unique<controller::TaskManagementController>());
        controllers_.push_back(
            std::make_unique<controller::LoggingController>());
        controllers_.push_back(
            std::make_unique<controller::ServerStatusController>());
        controllers_.push_back(
            std::make_unique<controller::IsolatedController>());
        controllers_.push_back(
            std::make_unique<controller::ToolRegistryController>());
        controllers_.push_back(std::make_unique<controller::VenvController>());

        // Device plugin controller
        controllers_.push_back(
            std::make_unique<controller::DevicePluginController>());

        // Register all controller routes
        for (auto& controller : controllers_) {
            controller->registerRoutes(app_);
        }

        LOG_INFO("Controllers initialized: {} controllers registered",
                 controllers_.size());
    }

    /**
     * @brief Initialize WebSocket server
     */
    void initializeWebSocket() {
        LOG_INFO("Initializing WebSocket server...");

        // Create message bus for inter-component communication
        auto message_bus =
            atom::async::MessageBus::createShared(message_bus_io_);

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

        LOG_INFO("WebSocket server initialized at /api/v1/ws");

        // Inject shared components into WebSocket server for command handlers
        websocket_server_->setTaskManager(task_manager_);
        websocket_server_->setEventLoop(event_loop_);

        // Inject shared components into ServerStatusController
        controller::ServerStatusController::setWebSocketServer(
            websocket_server_);
        controller::ServerStatusController::setTaskManager(task_manager_);
        controller::ServerStatusController::setEventLoop(event_loop_);

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

                    task["cancelRequested"] = info.cancelRequested.load();
                    task["priority"] = info.priority;
                    task["progress"] = info.progress;

                    if (!info.progressMessage.empty()) {
                        task["progressMessage"] = info.progressMessage;
                    }

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

        // Store command dispatcher for plugin system
        command_dispatcher_ = command_dispatcher;
    }

    /**
     * @brief Initialize plugin system
     */
    void initializePlugins() {
        if (!config_.enable_plugins) {
            LOG_INFO("Plugin system disabled");
            return;
        }

        LOG_INFO("Initializing plugin system...");

        if (!plugin_manager_) {
            LOG_ERROR("Plugin manager not created");
            return;
        }

        // Initialize plugin manager with app and command dispatcher
        if (!plugin_manager_->initialize(app_, command_dispatcher_)) {
            LOG_ERROR("Failed to initialize plugin manager");
            return;
        }

        // Register plugin controller
        controllers_.push_back(
            std::make_unique<controller::PluginController>());
        controllers_.back()->registerRoutes(app_);

        // Auto-load plugins if configured
        if (config_.auto_load_plugins) {
            size_t loaded = plugin_manager_->discoverAndLoadAll();
            LOG_INFO("Auto-loaded {} plugins", loaded);
        }

        // Subscribe to plugin events for logging
        plugin_manager_->subscribeToEvents([](plugin::PluginEvent event,
                                              const std::string& name,
                                              const nlohmann::json& data) {
            switch (event) {
                case plugin::PluginEvent::Loaded:
                    LOG_INFO("Plugin loaded: {}", name);
                    break;
                case plugin::PluginEvent::Unloaded:
                    LOG_INFO("Plugin unloaded: {}", name);
                    break;
                case plugin::PluginEvent::Reloaded:
                    LOG_INFO("Plugin reloaded: {}", name);
                    break;
                case plugin::PluginEvent::Error:
                    LOG_ERROR("Plugin error: {} - {}", name,
                              data.value("error", "unknown"));
                    break;
                default:
                    break;
            }
        });

        LOG_INFO("Plugin system initialized");
    }
};

}  // namespace lithium::server

#endif  // LITHIUM_SERVER_MAIN_SERVER_HPP
