#ifndef LITHIUM_TASK_ADVANCED_MERIDIAN_FLIP_TASK_HPP
#define LITHIUM_TASK_ADVANCED_MERIDIAN_FLIP_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Automated Meridian Flip Task
 *
 * Performs automated meridian flip when telescope crosses the meridian,
 * including plate solving verification and autofocus after flip.
 * Inspired by NINA's meridian flip functionality.
 */
class MeridianFlipTask : public Task {
public:
    MeridianFlipTask()
        : Task("MeridianFlip",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "MeridianFlip"; }

    // Enhanced functionality
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateMeridianFlipParameters(const json& params);

private:
    void executeImpl(const json& params);
    bool checkMeridianFlipRequired(double targetRA, double currentHA);
    void performFlip();
    void verifyFlip();
    void recenterTarget();
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_MERIDIAN_FLIP_TASK_HPP
