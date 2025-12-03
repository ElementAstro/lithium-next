#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

#include "crow/compression.h"

#include "components/loader.hpp"
#include "components/manager.hpp"

#include "device/manager.hpp"

#include "script/check.hpp"
#include "script/interpreter_pool.hpp"
#include "script/python_caller.hpp"
#include "script/script_service.hpp"
#include "script/shell/script_manager.hpp"
#include "script/tools/tool_registry.hpp"
#include "script/venv/venv_manager.hpp"
#include "script/isolated/runner.hpp"

#include "config/config.hpp"
#include "config/core/config_registry.hpp"
#include "config/sections/sections.hpp"
#include "constant/constant.hpp"
#include "server/command.hpp"
#include "server/controller/controller.hpp"
#include "server/eventloop.hpp"

#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/system/crash.hpp"
#include "atom/system/env.hpp"
#include "atom/utils/argsview.hpp"

#include "debug/terminal.hpp"

// System Controllers
#include "server/controller/system/config.hpp"
#include "server/controller/system/search.hpp"

// Device Controllers
#include "server/controller/device/camera.hpp"
#include "server/controller/device/filterwheel.hpp"
#include "server/controller/device/focuser.hpp"
#include "server/controller/device/mount.hpp"

// Script Controllers
#include "server/controller/script/python.hpp"
#include "server/controller/script/isolated.hpp"
#include "server/controller/script/shell.hpp"
#include "server/controller/script/tool_registry.hpp"
#include "server/controller/script/venv.hpp"

// Sequencer
#include "server/controller/sequencer.hpp"
#include "server/websocket.hpp"

using namespace std::string_literals;
namespace fs = std::filesystem;

void registerControllers(
    crow::SimpleApp &app,
    const std::vector<std::shared_ptr<Controller>> &controllers) {
    for (const auto &controller : controllers) {
        controller->registerRoutes(app);
    }
}

/**
 * @brief Initialize logging system from unified configuration
 * @param loggingConfig Unified logging configuration
 */
void setupLoggingFromConfig(const lithium::config::LoggingConfig& loggingConfig) {
    lithium::log::LogConfig config;
    config.log_dir = loggingConfig.logDir;
    config.log_filename = loggingConfig.logFilename;
    config.max_file_size = loggingConfig.maxFileSize;
    config.max_files = loggingConfig.maxFiles;

    // Convert string levels to enum
    auto parseLevel = [](const std::string& level) -> lithium::log::Level {
        if (level == "trace") return lithium::log::Level::Trace;
        if (level == "debug") return lithium::log::Level::Debug;
        if (level == "info") return lithium::log::Level::Info;
        if (level == "warn" || level == "warning") return lithium::log::Level::Warn;
        if (level == "error" || level == "err") return lithium::log::Level::Error;
        if (level == "critical" || level == "fatal") return lithium::log::Level::Critical;
        return lithium::log::Level::Info;
    };

    config.console_level = parseLevel(loggingConfig.consoleLevel);
    config.file_level = parseLevel(loggingConfig.fileLevel);
    config.async_mode = loggingConfig.asyncMode;
    config.main_thread_name = loggingConfig.mainThreadName;

    lithium::log::init(config);
}

/**
 * @brief Initialize logging system with default configuration
 * @note This is called before ConfigRegistry is available
 */
void setupLoggingDefault() {
    lithium::log::LogConfig config;
    config.log_dir = "logs";
    config.log_filename = "lithium";
    config.max_file_size = 10 * 1024 * 1024;  // 10 MB
    config.max_files = 5;
    config.console_level = lithium::log::Level::Info;
    config.file_level = lithium::log::Level::Trace;
    config.async_mode = true;
    config.main_thread_name = "main";

    lithium::log::init(config);
}

void injectPtr() {
    LOG_INFO("Injecting global pointers...");

    auto ioContext = atom::memory::makeShared<asio::io_context>();
    AddPtr<atom::async::MessageBus>(
        Constants::MESSAGE_BUS,
        atom::memory::makeShared<atom::async::MessageBus>(*ioContext));
    auto eventLoop = atom::memory::makeShared<lithium::app::EventLoop>(4);
    AddPtr<lithium::app::EventLoop>(Constants::EVENT_LOOP, eventLoop);
    AddPtr<lithium::app::CommandDispatcher>(
        Constants::COMMAND_DISPATCHER,
        atom::memory::makeShared<lithium::app::CommandDispatcher>(
            eventLoop, lithium::app::CommandDispatcher::Config{}));

    AddPtr<lithium::ConfigManager>(
        Constants::CONFIG_MANAGER,
        atom::memory::makeShared<lithium::ConfigManager>());

    auto env = atom::memory::makeShared<atom::utils::Env>();
    AddPtr<atom::utils::Env>(Constants::ENVIRONMENT, env);
    auto moduleDir = env->getEnv("LITHIUM_MODULE_DIR");

    AddPtr<lithium::ComponentManager>(
        Constants::COMPONENT_MANAGER,
        atom::memory::makeShared<lithium::ComponentManager>());
    AddPtr<lithium::ModuleLoader>(
        Constants::MODULE_LOADER,
        atom::memory::makeShared<lithium::ModuleLoader>(
            moduleDir.empty() ? "modules"s : moduleDir));

    AddPtr<lithium::DeviceManager>(
        Constants::DEVICE_MANAGER,
        atom::memory::makeShared<lithium::DeviceManager>());

    // Initialize unified ScriptService (integrates all script components)
    auto scriptAnalysisPath = env->getEnv("LITHIUM_SCRIPT_ANALYSIS_PATH");
    auto toolsDir = env->getEnv("LITHIUM_TOOLS_DIR");

    lithium::ScriptServiceConfig scriptConfig;
    scriptConfig.analysisConfigPath = scriptAnalysisPath.empty()
        ? "./config/script/analysis.json"s : scriptAnalysisPath;
    scriptConfig.toolsDirectory = toolsDir.empty()
        ? "./python/tools"s : toolsDir;
    scriptConfig.poolSize = 4;
    scriptConfig.autoDiscoverTools = true;
    scriptConfig.enableSecurityAnalysis = true;

    auto scriptService = atom::memory::makeShared<lithium::ScriptService>(scriptConfig);
    auto initResult = scriptService->initialize();
    if (!initResult) {
        LOG_ERROR("Failed to initialize ScriptService: {}",
                  lithium::scriptServiceErrorToString(initResult.error()));
    }
    AddPtr<lithium::ScriptService>(Constants::SCRIPT_SERVICE, scriptService);

    // Also expose individual components for backward compatibility
    AddPtr<lithium::PythonWrapper>(
        Constants::PYTHON_WRAPPER,
        scriptService->getPythonWrapper());
    AddPtr<lithium::shell::ScriptManager>(
        Constants::SCRIPT_MANAGER,
        scriptService->getScriptManager());
    AddPtr<lithium::ScriptAnalyzer>(
        Constants::SCRIPT_ANALYZER,
        scriptService->getScriptAnalyzer());
    AddPtr<lithium::InterpreterPool>(
        Constants::INTERPRETER_POOL,
        scriptService->getInterpreterPool());
    AddPtr<lithium::tools::PythonToolRegistry>(
        Constants::PYTHON_TOOL_REGISTRY,
        scriptService->getToolRegistry());
    AddPtr<lithium::venv::VenvManager>(
        Constants::VENV_MANAGER,
        scriptService->getVenvManager());
    AddPtr<lithium::isolated::PythonRunner>(
        Constants::ISOLATED_PYTHON_RUNNER,
        scriptService->getIsolatedRunner());

    LOG_INFO("Global pointers injected.");
}

int main(int argc, char *argv[]) {
#if LITHIUM_ENABLE_CPPTRACE
    cpptrace::init();
#endif
    // NOTE: gettext is not supported yet, it will cause compilation error on
    // Mingw64
    /* Add gettext */
#if LITHIUM_ENABLE_GETTEXT
    bindtextdomain("lithium", "locale");
    /* Only write the following 2 lines if creating an executable */
    setlocale(LC_ALL, "");
    textdomain("lithium");
#endif

    // Step 1: Initialize with default logging (before config is loaded)
    setupLoggingDefault();

    // Step 2: Parse command line arguments first
    atom::utils::ArgumentParser program("Lithium Server"s);

    // NOTE: The command arguments' priority is higher than the config file
    program.addArgument("port", atom::utils::ArgumentParser::ArgType::INTEGER,
                        false, 8000, "Port of the server", {"p"});
    program.addArgument("host", atom::utils::ArgumentParser::ArgType::STRING,
                        false, "0.0.0.0"s, "Host of the server", {"h"});
    program.addArgument("config", atom::utils::ArgumentParser::ArgType::STRING,
                        false, "config.yaml"s, "Path to the config file",
                        {"c"});
    program.addArgument("module-path",
                        atom::utils::ArgumentParser::ArgType::STRING, false,
                        "modules"s, "Path to the modules directory", {"m"});
    program.addArgument("web-panel",
                        atom::utils::ArgumentParser::ArgType::BOOLEAN, false,
                        true, "Enable web panel", {"w"});
    program.addArgument("debug", atom::utils::ArgumentParser::ArgType::BOOLEAN,
                        false, false, "Enable debug mode", {"d"});
    program.addArgument("log-level",
                        atom::utils::ArgumentParser::ArgType::STRING, false,
                        "info"s, "Log level (trace/debug/info/warn/error)", {"l"});

    program.addDescription("Lithium Command Line Interface:");
    program.addEpilog("End.");

    std::vector<std::string> args(argv, argv + argc);
    program.parse(argc, args);

    // Step 3: Create ConfigManager and ConfigRegistry
    auto configManager = atom::memory::makeShared<lithium::ConfigManager>();
    AddPtr<lithium::ConfigManager>(Constants::CONFIG_MANAGER, configManager);

    auto& registry = lithium::config::ConfigRegistry::instance();
    registry.setConfigManager(configManager);

    // Step 4: Register all configuration sections
    lithium::config::registerAllSections(registry);

    // Step 5: Apply default values
    registry.applyDefaults();

    // Step 6: Load configuration file if exists
    try {
        auto cmdConfigPath = program.get<std::string>("config");
        fs::path configPath = cmdConfigPath.value_or("config.yaml"s);

        // Try multiple config file locations
        std::vector<fs::path> configPaths = {
            configPath,
            "config.json",
            "config.yaml",
            "config/config.yaml",
            "config/config.json"
        };

        bool configLoaded = false;
        for (const auto& path : configPaths) {
            if (fs::exists(path)) {
                LOG_INFO("Loading configuration from: {}", path.string());
                lithium::config::ConfigLoadOptions options;
                options.strict = true;  // Strict validation mode
                options.mergeWithExisting = true;

                if (registry.loadFromFile(path, options)) {
                    configLoaded = true;
                    break;
                }
            }
        }

        if (!configLoaded) {
            LOG_WARN("No configuration file found, using defaults");
        }

    } catch (const lithium::config::ConfigValidationException& e) {
        LOG_ERROR("Configuration validation failed:\n{}", e.what());
        atom::system::saveCrashLog(e.what());
        return 1;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load configuration: {}", e.what());
        // Continue with defaults
    }

    // Step 7: Apply command line overrides (highest priority)
    try {
        auto cmdHost = program.get<std::string>("host");
        auto cmdPort = program.get<int>("port");
        auto cmdModulePath = program.get<std::string>("module-path");
        auto cmdWebPanel = program.get<bool>("web-panel");
        auto cmdDebug = program.get<bool>("debug");
        auto cmdLogLevel = program.get<std::string>("log-level");

        if (cmdHost && !cmdHost->empty() && *cmdHost != "0.0.0.0") {
            registry.updateValue("/lithium/server/host", *cmdHost);
            LOG_DEBUG("CLI override: server host = {}", *cmdHost);
        }
        if (cmdPort && *cmdPort != 8000) {
            registry.updateValue("/lithium/server/port", *cmdPort);
            LOG_DEBUG("CLI override: server port = {}", *cmdPort);
        }
        if (cmdModulePath && !cmdModulePath->empty() && *cmdModulePath != "modules") {
            registry.updateValue("/lithium/module/path", *cmdModulePath);
            LOG_DEBUG("CLI override: module path = {}", *cmdModulePath);
        }
        if (cmdWebPanel.has_value()) {
            registry.updateValue("/lithium/server/enableWebPanel", *cmdWebPanel);
            LOG_DEBUG("CLI override: web panel = {}", *cmdWebPanel);
        }
        if (cmdDebug.has_value() && *cmdDebug) {
            registry.updateValue("/lithium/debug/enabled", *cmdDebug);
            LOG_DEBUG("CLI override: debug mode = {}", *cmdDebug);
        }
        if (cmdLogLevel && !cmdLogLevel->empty() && *cmdLogLevel != "info") {
            registry.updateValue("/lithium/logging/consoleLevel", *cmdLogLevel);
            LOG_DEBUG("CLI override: log level = {}", *cmdLogLevel);
        }

    } catch (const std::bad_any_cast &e) {
        LOG_ERROR("Invalid args format! Error: {}", e.what());
        atom::system::saveCrashLog(e.what());
        return 1;
    }

    // Step 8: Reinitialize logging with configuration (if changed)
    auto loggingConfig = registry.getSectionOrDefault<lithium::config::LoggingConfig>();
    lithium::log::shutdown();
    setupLoggingFromConfig(loggingConfig);
    LOG_INFO("Logging system initialized with configuration");

    // Step 9: Inject other global pointers
    injectPtr();

    // Step 10: Get server configuration
    auto serverConfig = registry.getSectionOrDefault<lithium::config::ServerConfig>();

    crow::SimpleApp app;

    // Enable GZIP compression based on configuration
    if (serverConfig.enableCompression) {
        app.use_compression(crow::compression::algorithm::GZIP);
    }

    std::vector<std::shared_ptr<Controller>> controllers;
    controllers.push_back(atom::memory::makeShared<ConfigController>());
    controllers.push_back(atom::memory::makeShared<ScriptController>());
    controllers.push_back(atom::memory::makeShared<SearchController>());
    controllers.push_back(atom::memory::makeShared<SequenceController>());
    controllers.push_back(atom::memory::makeShared<MountController>());
    controllers.push_back(atom::memory::makeShared<FocuserController>());
    controllers.push_back(atom::memory::makeShared<FilterWheelController>());
    controllers.push_back(atom::memory::makeShared<CameraController>());
    controllers.push_back(atom::memory::makeShared<IsolatedController>());
    controllers.push_back(atom::memory::makeShared<ToolRegistryController>());
    controllers.push_back(atom::memory::makeShared<VenvController>());
    controllers.push_back(atom::memory::makeShared<PythonServiceController>());

    std::thread serverThread([&app, &controllers, &serverConfig]() {
        registerControllers(app, controllers);

        LOG_INFO("Server starting on {}:{}", serverConfig.host, serverConfig.port);
        app.port(serverConfig.port).multithreaded().run();
    });

    WebSocketServer ws_server(
        app, GetPtr<atom::async::MessageBus>(Constants::MESSAGE_BUS).value(),
        GetPtr<lithium::app::CommandDispatcher>(Constants::COMMAND_DISPATCHER)
            .value(),
        {});
    ws_server.start();

    std::thread *debugTerminalThread = nullptr;
    if (program.get<bool>("debug").value_or(false)) {
        LOG_INFO("Debug mode enabled, starting debug terminal...");
        debugTerminalThread = new std::thread([]() {
            lithium::debug::ConsoleTerminal terminal;
            terminal.run();
        });
    }

    serverThread.join();
    if (debugTerminalThread) {
        debugTerminalThread->join();
        delete debugTerminalThread;
    }

    // Shutdown logging system (flush async queue)
    lithium::log::shutdown();

    return 0;
}
