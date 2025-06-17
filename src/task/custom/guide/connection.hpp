#ifndef LITHIUM_TASK_GUIDE_CONNECTION_TASKS_HPP
#define LITHIUM_TASK_GUIDE_CONNECTION_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Guider connection task.
 * Connects to PHD2 guiding software.
 */
class GuiderConnectTask : public Task {
public:
    /**
     * @brief Constructor for GuiderConnectTask
     */
    GuiderConnectTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void connectToPHD2(const json& params);
};

/**
 * @brief Guider disconnection task.
 * Disconnects from PHD2 guiding software.
 */
class GuiderDisconnectTask : public Task {
public:
    /**
     * @brief Constructor for GuiderDisconnectTask
     */
    GuiderDisconnectTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void disconnectFromPHD2(const json& params);
};

/**
 * @brief Check PHD2 connection status task.
 * Checks if PHD2 is connected and responsive.
 */
class GuiderConnectionStatusTask : public Task {
public:
    /**
     * @brief Constructor for GuiderConnectionStatusTask
     */
    GuiderConnectionStatusTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void checkConnectionStatus(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_CONNECTION_TASKS_HPP
