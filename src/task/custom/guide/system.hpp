#ifndef LITHIUM_TASK_GUIDE_SYSTEM_TASKS_HPP
#define LITHIUM_TASK_GUIDE_SYSTEM_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Get PHD2 app state task.
 * Gets the current PHD2 application state.
 */
class GetAppStateTask : public Task {
public:
    GetAppStateTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getAppState(const json& params);
};

/**
 * @brief Get guide output enabled task.
 * Checks if guide output is enabled.
 */
class GetGuideOutputEnabledTask : public Task {
public:
    GetGuideOutputEnabledTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getGuideOutputEnabled(const json& params);
};

/**
 * @brief Set guide output enabled task.
 * Enables or disables guide output.
 */
class SetGuideOutputEnabledTask : public Task {
public:
    SetGuideOutputEnabledTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void setGuideOutputEnabled(const json& params);
};

/**
 * @brief Get paused status task.
 * Checks if PHD2 is currently paused.
 */
class GetPausedStatusTask : public Task {
public:
    GetPausedStatusTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getPausedStatus(const json& params);
};

/**
 * @brief Shutdown PHD2 task.
 * Shuts down the PHD2 application.
 */
class ShutdownPHD2Task : public Task {
public:
    ShutdownPHD2Task();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void shutdownPHD2(const json& params);
};

/**
 * @brief Send guide pulse task.
 * Sends a direct guide pulse command.
 */
class SendGuidePulseTask : public Task {
public:
    SendGuidePulseTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void sendGuidePulse(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_SYSTEM_TASKS_HPP
