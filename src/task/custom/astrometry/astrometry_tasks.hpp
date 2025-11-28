/**
 * @file astrometry_tasks.hpp
 * @brief Astrometry tasks for plate solving and target centering
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_ASTROMETRY_TASKS_HPP
#define LITHIUM_TASK_ASTROMETRY_TASKS_HPP

#include "../common/task_base.hpp"
#include "../common/types.hpp"
#include "../common/validation.hpp"

namespace lithium::task::astrometry {

using namespace lithium::task;

/**
 * @brief Plate solve an image file
 *
 * Performs astrometric plate solving on an existing image file
 * to determine its celestial coordinates.
 */
class PlateSolveTask : public TaskBase {
public:
    PlateSolveTask() : TaskBase("PlateSolve") { setupParameters(); }
    PlateSolveTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "PlateSolve"; }
    static std::string getStaticTaskTypeName() { return "PlateSolve"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    PlateSolveResult solve(const std::string& imagePath, const json& hints);
};

/**
 * @brief Take an exposure and plate solve it
 *
 * Takes a short exposure and immediately plate solves it
 * to determine the current pointing position.
 */
class PlateSolveExposureTask : public TaskBase {
public:
    PlateSolveExposureTask() : TaskBase("PlateSolveExposure") { setupParameters(); }
    PlateSolveExposureTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "PlateSolveExposure"; }
    static std::string getStaticTaskTypeName() { return "PlateSolveExposure"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Center on target using plate solving
 *
 * Iteratively slews and plate solves to precisely center
 * the telescope on a target coordinate.
 */
class CenteringTask : public TaskBase {
public:
    CenteringTask() : TaskBase("Centering") { setupParameters(); }
    CenteringTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "Centering"; }
    static std::string getStaticTaskTypeName() { return "Centering"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    double calculateSeparation(double ra1, double dec1, double ra2, double dec2);
};

/**
 * @brief Sync mount to plate solve result
 *
 * Syncs the mount's internal coordinates to match
 * a plate solve result.
 */
class SyncToSolveTask : public TaskBase {
public:
    SyncToSolveTask() : TaskBase("SyncToSolve") { setupParameters(); }
    SyncToSolveTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "SyncToSolve"; }
    static std::string getStaticTaskTypeName() { return "SyncToSolve"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Blind plate solve without hints
 *
 * Performs a blind plate solve without any coordinate hints,
 * searching the entire sky if necessary.
 */
class BlindSolveTask : public TaskBase {
public:
    BlindSolveTask() : TaskBase("BlindSolve") { setupParameters(); }
    BlindSolveTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "BlindSolve"; }
    static std::string getStaticTaskTypeName() { return "BlindSolve"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

}  // namespace lithium::task::astrometry

#endif  // LITHIUM_TASK_ASTROMETRY_TASKS_HPP
