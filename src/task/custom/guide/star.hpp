#ifndef LITHIUM_TASK_GUIDE_STAR_TASKS_HPP
#define LITHIUM_TASK_GUIDE_STAR_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Find star task.
 * Automatically finds a guide star.
 */
class FindStarTask : public Task {
public:
    FindStarTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void findGuideStar(const json& params);
};

/**
 * @brief Set lock position task.
 * Sets the lock position for guiding.
 */
class SetLockPositionTask : public Task {
public:
    SetLockPositionTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void setLockPosition(const json& params);
};

/**
 * @brief Get lock position task.
 * Gets current lock position.
 */
class GetLockPositionTask : public Task {
public:
    GetLockPositionTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getLockPosition(const json& params);
};

/**
 * @brief Get pixel scale task.
 * Gets the current pixel scale in arc-seconds per pixel.
 */
class GetPixelScaleTask : public Task {
public:
    GetPixelScaleTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getPixelScale(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_STAR_TASKS_HPP
