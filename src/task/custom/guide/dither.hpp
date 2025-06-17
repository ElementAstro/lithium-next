#ifndef LITHIUM_TASK_GUIDE_DITHER_TASKS_HPP
#define LITHIUM_TASK_GUIDE_DITHER_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Single dither task.
 * Performs a single dither movement.
 */
class GuiderDitherTask : public Task {
public:
    GuiderDitherTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performDither(const json& params);
};

/**
 * @brief Dithering sequence task.
 * Performs multiple dithers in a sequence with settling.
 */
class DitherSequenceTask : public Task {
public:
    DitherSequenceTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performDitherSequence(const json& params);
};

/**
 * @brief Random dither task.
 * Performs a random dither movement within specified bounds.
 */
class GuiderRandomDitherTask : public Task {
public:
    GuiderRandomDitherTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performRandomDither(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_DITHER_TASKS_HPP
