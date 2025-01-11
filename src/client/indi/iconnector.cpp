/*
 * manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-29

Description: INDI Device Manager

**************************************************/

#include "iconnector.hpp"
#include "container.hpp"

#include "atom/error/exception.hpp"
#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/command.hpp"
#include "atom/system/software.hpp"

namespace {
constexpr int MAX_BUFFER_SIZE = 4096;
constexpr int MAX_RETRY_COUNT = 3;

std::mutex server_mutex;
std::mutex driver_mutex;
}  // namespace

INDIConnector::INDIConnector(const std::string &hst, int prt,
                             const std::string &cfg, const std::string &dta,
                             const std::string &fif)
    : host_(hst),
      port_(prt),
      config_path_(cfg),
      data_path_(dta),
      fifo_path_(fif) {
    LOG_F(INFO, "Initializing INDI Connector - Host: {}, Port: {}", host_,
          port_);

    if (port_ <= 0 || port_ > 65535) {
        THROW_RUNTIME_ERROR("Invalid port number");
    }

    validatePaths();
}

void INDIConnector::validatePaths() {
    if (!atom::io::isFolderExists(config_path_)) {
        LOG_F(WARNING, "Config directory does not exist: {}", config_path_);
        if (!atom::io::createDirectory(config_path_)) {
            THROW_RUNTIME_ERROR("Failed to create config directory");
        }
    }

    if (!atom::io::isFolderExists(data_path_)) {
        LOG_F(WARNING, "Data directory does not exist: {}", data_path_);
        if (!atom::io::createDirectory(data_path_)) {
            THROW_RUNTIME_ERROR("Failed to create data directory");
        }
    }
}

auto INDIConnector::startServer() -> bool {
    std::lock_guard lock(server_mutex);

    LOG_F(INFO, "Starting INDI server on port {}", port_);

    if (isRunning()) {
        LOG_F(INFO, "INDI server already running - stopping first");
        if (!stopServer()) {
            LOG_F(ERROR, "Failed to stop existing server");
            return false;
        }
    }

    // Clean up old FIFO
    if (atom::io::isFileExists(fifo_path_)) {
        if (!atom::io::removeFile(fifo_path_)) {
            LOG_F(ERROR, "Failed to remove existing FIFO pipe");
            return false;
        }
    }

    std::stringstream cmd;
    cmd << "indiserver -p " << port_ << " -m 100 -v -f " << fifo_path_
        << " > /tmp/indiserver.log 2>&1";

    server_process_ = std::make_unique<AsyncSystemCommand>(cmd.str());
    server_process_->run();

    // 等待服务器启动
    for (int i = 0; i < 10; i++) {
        if (isRunning()) {
            LOG_F(INFO, "INDI server started successfully");
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_F(ERROR, "Failed to start INDI server");
    return false;
}

auto INDIConnector::stopServer() -> bool {
    std::lock_guard lock(server_mutex);

    if (!isRunning()) {
        DLOG_F(INFO, "INDI server is not running");
        return true;
    }

    LOG_F(INFO, "Stopping INDI server");

    if (server_process_) {
        server_process_->terminate();
        server_process_.reset();
    }

    // 确保所有驱动进程也被终止
    for (auto &[_, process] : driver_processes_) {
        process->terminate();
    }
    driver_processes_.clear();
    running_drivers_.clear();

    return !isRunning();
}

auto INDIConnector::isRunning() -> bool {
    return server_process_ && server_process_->isRunning();
}

auto INDIConnector::isInstalled() -> bool {
    try {
        return atom::system::checkSoftwareInstalled("hydrogenserver");
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Error checking installation: {}", e.what());
        return false;
    }
}

auto INDIConnector::startDriver(
    const std::shared_ptr<INDIDeviceContainer> &driver) -> bool {
    std::lock_guard lock(driver_mutex);

    if (!driver) {
        LOG_F(ERROR, "Invalid driver pointer");
        return false;
    }

    LOG_F(INFO, "Starting INDI driver: {}", driver->label);

    std::stringstream cmd;
    cmd << "echo \"start " << driver->label;
    if (!driver->skeleton.empty()) {
        cmd << " -s \\\"" << driver->skeleton << "\\\"";
    }
    cmd << "\" > " << fifo_path_;

    auto process = std::make_unique<AsyncSystemCommand>(cmd.str());
    process->run();
    driver_processes_[driver->label] = std::move(process);
    running_drivers_.emplace(driver->label, driver);

    return true;
}

auto INDIConnector::stopDriver(
    const std::shared_ptr<INDIDeviceContainer> &driver) -> bool {
    std::lock_guard lock(driver_mutex);

    auto it = driver_processes_.find(driver->label);
    if (it != driver_processes_.end()) {
        it->second->terminate();
        driver_processes_.erase(it);
        running_drivers_.erase(driver->label);
        return true;
    }

    return false;
}

auto INDIConnector::setProp(const std::string &dev, const std::string &prop,
                            const std::string &element,
                            const std::string &value) -> bool {
    std::stringstream sss;
    sss << "indi_setprop " << dev << "." << prop << "." << element << "="
        << value;
    std::string cmd = sss.str();
    DLOG_F(INFO, "Cmd: {}", cmd);
    try {
        if (atom::system::executeCommand(cmd, false) != "") {
            LOG_F(ERROR, "Failed to execute command: {}", cmd);
            return false;
        }
    } catch (const atom::error::RuntimeError &e) {
        LOG_F(ERROR, "Failed to execute command: {} with {}", cmd, e.what());
        return false;
    }
    DLOG_F(INFO, "Set property: {}.{} to {}", dev, prop, value);
    return true;
}

auto INDIConnector::getProp(const std::string &dev, const std::string &prop,
                            const std::string &element) -> std::string {
    std::stringstream sss;
#if ENABLE_INDI
    sss << "indi_getprop " << dev << "." << prop << "." << element;
#else
    sss << "indi_getprop " << dev << "." << prop << "." << element;
#endif
    std::string cmd = sss.str();
    try {
        std::string output = atom::system::executeCommand(cmd, false);
        size_t equalsPos = output.find('=');
        if (equalsPos != std::string::npos && equalsPos + 1 < output.length()) {
            return output.substr(equalsPos + 1,
                                 output.length() - equalsPos - 2);
        }
    } catch (const atom::error::RuntimeError &e) {
        LOG_F(ERROR, "Failed to execute command: {} with {}", cmd, e.what());
    }
    return "";
}

auto INDIConnector::getState(const std::string &dev,
                             const std::string &prop) -> std::string {
    return getProp(dev, prop, "_STATE");
}

#if ENABLE_FASTHASH
emhash8::HashMap<std::string, std::shared_ptr<INDIDeviceContainer>>
INDIConnector::getRunningDrivers()
#else
auto INDIConnector::getRunningDrivers()
    -> std::unordered_map<std::string, std::shared_ptr<INDIDeviceContainer>>
#endif
{
    return running_drivers_;
}

#if ENABLE_FASTHASH
std::vector<emhash8::HashMap<std::string, std::string>>
INDIConnector::getDevices()
#else
auto INDIConnector::getDevices()
    -> std::vector<std::unordered_map<std::string, std::string>>
#endif
{
#if ENABLE_FASTHASH
    std::vector<emhash8::HashMap<std::string, std::string>> devices;
#else
    std::vector<std::unordered_map<std::string, std::string>> devices;
#endif
    std::string cmd = "indi_getprop *.CONNECTION.CONNECT";

    auto result = atom::system::executeCommand(cmd, false);
    std::vector<std::string> lines = {"", ""};
    for (char token : result) {
        if (token == '\n') {
            std::unordered_map<std::string, std::string> device;
            std::stringstream sss(lines[0]);
            std::string item;
            while (getline(sss, item, '.')) {
                device["device"] = item;
            }
            device["connected"] = (lines[1] == "On") ? "true" : "false";
            devices.push_back(device);
            lines = {"", ""};
        } else if (token == '=') {
            lines[1] = lines[1].substr(0, lines[1].length() - 1);
        } else if (token == ' ') {
            continue;
        } else {
            lines[(lines[0] == "") ? 0 : 1] += token;
        }
    }
    return devices;
}
