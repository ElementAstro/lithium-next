#include <memory>
#include <thread>
#include <vector>

#include "config/configor.hpp"
#include "constant/constant.hpp"
#include "server/controller/controller.hpp"
#include "server/middleware.hpp"

#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/crash.hpp"
#include "atom/utils/argsview.hpp"

#include "debug/terminal.hpp"
#include "server/controller/config.hpp"
#include "server/controller/python.hpp"
#include "server/controller/script.hpp"
#include "server/controller/search.hpp"
#include "server/controller/sequencer.hpp"

using namespace std::string_literals;

void registerControllers(
    crow::SimpleApp &app,
    const std::vector<std::shared_ptr<Controller>> &controllers) {
    for (const auto &controller : controllers) {
        controller->registerRoutes(app);
    }
}

/**
 * @brief setup log file
 * @note This is called in main function
 */
void setupLogFile() {
    std::filesystem::path logsFolder = std::filesystem::current_path() / "logs";
    if (!std::filesystem::exists(logsFolder)) {
        std::filesystem::create_directory(logsFolder);
    }
    auto now = std::chrono::system_clock::now();
    auto nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::tm *localTime = std::localtime(&nowTimeT);
    char filename[100];
    std::strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S.log", localTime);
    std::filesystem::path logFilePath = logsFolder / filename;
    loguru::add_file(logFilePath.string().c_str(), loguru::Append,
                     loguru::Verbosity_MAX);

    loguru::set_fatal_handler([](const loguru::Message &message) {
        atom::system::saveCrashLog(std::string(message.prefix) +
                                   message.message);
    });
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

    // Set log file
    setupLogFile();
    loguru::init(argc, argv);

    atom::utils::ArgumentParser program("Lithium Server"s);

    // NOTE: The command arguments' priority is higher than the config file
    program.addArgument("port", atom::utils::ArgumentParser::ArgType::INTEGER,
                        false, 8000, "Port of the server", {"p"});
    program.addArgument("host", atom::utils::ArgumentParser::ArgType::STRING,
                        false, "0.0.0.0"s, "Host of the server", {"h"});
    program.addArgument("config", atom::utils::ArgumentParser::ArgType::STRING,
                        false, "config.json"s, "Path to the config file",
                        {"c"});
    program.addArgument("module-path",
                        atom::utils::ArgumentParser::ArgType::STRING, false,
                        "modules"s, "Path to the modules directory", {"m"});
    program.addArgument("web-panel",
                        atom::utils::ArgumentParser::ArgType::BOOLEAN, false,
                        true, "Enable web panel", {"w"});
    program.addArgument("debug", atom::utils::ArgumentParser::ArgType::BOOLEAN,
                        false, false, "Enable debug mode", {"d"});
    program.addArgument("log-file",
                        atom::utils::ArgumentParser::ArgType::STRING, false,
                        ""s, "Path to the log file", {"l"});

    program.addDescription("Lithium Command Line Interface:");
    program.addEpilog("End.");

    std::vector<std::string> args(argv, argv + argc);
    program.parse(argc, args);

    // Parse arguments
    try {
        auto cmdHost = program.get<std::string>("host");
        auto cmdPort = program.get<int>("port");
        auto cmdConfigPath = program.get<std::string>("config");
        auto cmdModulePath = program.get<std::string>("module-path");
        auto cmdWebPanel = program.get<bool>("web-panel");
        auto cmdDebug = program.get<bool>("debug");

        auto configManager =
            GetPtr<lithium::ConfigManager>(Constants::CONFIG_MANAGER);

        if (cmdHost && !cmdHost->empty()) {
            configManager.value()->set("/lithium/server/host", cmdHost.value());
            DLOG_F(INFO, "Set server host to {}", cmdHost.value());
        }
        if (cmdPort != 8000) {
            DLOG_F(INFO, "Command line server port : {}", cmdPort);

            auto port = configManager.value()->get("/lithium/server/port");
            if (port && port.value() != cmdPort) {
                configManager.value()->set("/lithium/server/port",
                                           cmdPort.value());
                DLOG_F(INFO, "Set server port to {}", cmdPort.value());
            }
        }
        if (cmdConfigPath && !cmdConfigPath->empty()) {
            configManager.value()->set("/lithium/config/path", *cmdConfigPath);
            DLOG_F(INFO, "Set config path to {}", *cmdConfigPath);
        }
        if (cmdModulePath && !cmdModulePath->empty()) {
            configManager.value()->set("/lithium/module/path", *cmdModulePath);
            DLOG_F(INFO, "Set module path to {}", *cmdModulePath);
        }
        if (cmdWebPanel) {
            configManager.value()->set("/lithium/web-panel/enabled",
                                       *cmdWebPanel);
            DLOG_F(INFO, "Set web panel to {}", *cmdWebPanel);
        }
        if (cmdDebug) {
            configManager.value()->set("/lithium/debug/enabled", *cmdDebug);
            DLOG_F(INFO, "Set debug mode to {}", *cmdDebug);
        }
        if (program.get<std::string>("log-file")) {
            loguru::add_file(
                program.get<std::string>("log-file").value().c_str(),
                loguru::Append, loguru::Verbosity_MAX);
        }

    } catch (const std::bad_any_cast &e) {
        LOG_F(ERROR, "Invalid args format! Error: {}", e.what());
        atom::system::saveCrashLog(e.what());
        return 1;
    }

    crow::SimpleApp app;

    std::vector<std::shared_ptr<Controller>> controllers;
    controllers.push_back(std::make_shared<ConfigController>());
    controllers.push_back(std::make_shared<PythonController>());
    controllers.push_back(std::make_shared<ScriptController>());
    controllers.push_back(std::make_shared<SearchController>());
    controllers.push_back(std::make_shared<SequenceController>());

    AddPtr<lithium::ConfigManager>(Constants::CONFIG_MANAGER,
                                   std::make_shared<lithium::ConfigManager>());

    std::thread serverThread([&app, &controllers]() {
        registerControllers(app, controllers);

        auto configManager =
            GetPtr<lithium::ConfigManager>(Constants::CONFIG_MANAGER);
        if (!configManager) {
            LOG_F(ERROR, "Failed to get ConfigManager instance");
            return;
        }
        int port =
            configManager.value()->get("/lithium/server/port").value_or(8080);
        LOG_F(INFO, "Server is running on {}", port);
        app.port(port).multithreaded().run();
    });

    std::thread *debugTerminalThread = nullptr;
    if (program.get<bool>("debug").value_or(false)) {
        LOG_F(INFO, "Debug mode enabled, starting debug terminal...");
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

    return 0;
}
