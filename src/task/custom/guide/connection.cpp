#include "connection.hpp"
#include "exception/exception.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "client/phd2/types.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

static phd2::SettleParams createSettleParams(double tolerance, int time,
                                             int timeout = 60) {
    phd2::SettleParams params;
    params.pixels = tolerance;
    params.time = time;
    params.timeout = timeout;
    return params;
}

GuiderConnectTask::GuiderConnectTask()
    : Task("GuiderConnect",
           [this](const json& params) { connectToPHD2(params); }) {
    setTaskType("GuiderConnect");
    setPriority(7);
    setTimeout(std::chrono::seconds(30));
    addParamDefinition("host", "string", false, json("localhost"),
                       "Guider host address");
    addParamDefinition("port", "integer", false, json(4400),
                       "Guider port number (1-65535)");
    addParamDefinition("timeout", "integer", false, json(30),
                       "Connection timeout in seconds (1-300)");
}

void GuiderConnectTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting guider connection");
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw lithium::exception::SystemException(
                1001, errorMsg,
                {"GuiderConnect", "GuiderConnectTask", __FUNCTION__});
        }
        Task::execute(params);
    } catch (const lithium::exception::EnhancedException& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Guider connection failed: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Guider connection failed: " + std::string(e.what()));
        throw lithium::exception::SystemException(
            1002, "Guider connection failed: {}",
            {"GuiderConnect", "GuiderConnectTask", __FUNCTION__},
            e.what());
    }
}

void GuiderConnectTask::connectToPHD2(const json& params) {
    std::string host = params.value("host", "localhost");
    int port = params.value("port", 4400);
    int timeout = params.value("timeout", 30);

    if (port < 1 || port > 65535) {
        throw lithium::exception::SystemException(
            1003, "Port must be between 1 and 65535 (got {})",
            {"connectToPHD2", "GuiderConnectTask", __FUNCTION__},
            port);
    }
    if (timeout < 1 || timeout > 300) {
        throw lithium::exception::SystemException(
            1004, "Timeout must be between 1 and 300 seconds (got {})",
            {"connectToPHD2", "GuiderConnectTask", __FUNCTION__},
            timeout);
    }
    if (host.empty()) {
        throw lithium::exception::SystemException(
            1005, "Host cannot be empty",
            {"connectToPHD2", "GuiderConnectTask", __FUNCTION__});
    }
    spdlog::info("Connecting to guider at {}:{} with timeout {}s", host, port,
                 timeout);
    addHistoryEntry("Attempting connection to " + host + ":" +
                    std::to_string(port));
    auto phd2_client = GetPtrOrCreate<phd2::Client>(
        Constants::PHD2_CLIENT,
        [host, port]() { return std::make_shared<phd2::Client>(host, port); });
    if (!phd2_client) {
        throw lithium::exception::SystemException(
            1006, "Failed to get or create PHD2 client",
            {"connectToPHD2", "GuiderConnectTask", __FUNCTION__});
    }
    if (!phd2_client->connect(timeout * 1000)) {
        throw lithium::exception::SystemException(
            1007, "Failed to connect to PHD2 at {}:{}",
            {"connectToPHD2", "GuiderConnectTask", __FUNCTION__},
            host, port);
    }
}

std::string GuiderConnectTask::taskName() { return "GuiderConnect"; }

std::unique_ptr<Task> GuiderConnectTask::createEnhancedTask() {
    return std::make_unique<GuiderConnectTask>();
}

GuiderDisconnectTask::GuiderDisconnectTask()
    : Task("GuiderDisconnect",
           [this](const json& params) { disconnectFromPHD2(params); }) {
    setTaskType("GuiderDisconnect");
    setPriority(6);
    setTimeout(std::chrono::seconds(10));
    addParamDefinition(
        "force", "boolean", false, json(false),
        "Force disconnection even if operations are in progress");
}

void GuiderDisconnectTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting guider disconnection");
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw lithium::exception::SystemException(
                2001, errorMsg,
                {"GuiderDisconnect", "GuiderDisconnectTask", __FUNCTION__});
        }
        Task::execute(params);
    } catch (const lithium::exception::EnhancedException& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Guider disconnection failed: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Guider disconnection failed: " + std::string(e.what()));
        throw lithium::exception::SystemException(
            2002, "Guider disconnection failed: {}",
            {"GuiderDisconnect", "GuiderDisconnectTask", __FUNCTION__},
            e.what());
    }
}

void GuiderDisconnectTask::disconnectFromPHD2(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw lithium::exception::SystemException(
            2003, "PHD2 client not found in global manager",
            {"disconnectFromPHD2", "GuiderDisconnectTask", __FUNCTION__});
    }
    bool force = params.value("force", false);
    if (force) {
        spdlog::info("Force disconnecting from guider");
        addHistoryEntry("Force disconnection initiated");
    } else {
        spdlog::info("Disconnecting from guider");
        addHistoryEntry("Normal disconnection initiated");
    }
    phd2_client.value()->disconnect();
    spdlog::info("Guider disconnected");
    addHistoryEntry("Disconnection completed");
}

std::string GuiderDisconnectTask::taskName() { return "GuiderDisconnect"; }

std::unique_ptr<Task> GuiderDisconnectTask::createEnhancedTask() {
    return std::make_unique<GuiderDisconnectTask>();
}

}  // namespace lithium::task::guide
