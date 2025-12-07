/**
 * @file filterwheel_tasks.hpp
 * @brief Filter wheel-related tasks for filter management and sequences
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_COMPONENTS_FILTERWHEEL_TASKS_HPP
#define LITHIUM_TASK_COMPONENTS_FILTERWHEEL_TASKS_HPP

#include "../common/task_base.hpp"
#include "../common/types.hpp"
#include "../common/validation.hpp"

namespace lithium::task::filterwheel {

using namespace lithium::task;

/**
 * @brief Change filter to specified position or name
 */
class ChangeFilterTask : public TaskBase {
public:
    ChangeFilterTask() : TaskBase("ChangeFilter") { setupParameters(); }
    ChangeFilterTask(const std::string& name, const json& config)
        : TaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "ChangeFilter"; }
    static std::string getStaticTaskTypeName() { return "ChangeFilter"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Multi-filter imaging sequence task
 *
 * Executes a series of exposures across multiple filters with
 * configurable exposure counts and optional dithering.
 */
class FilterSequenceTask : public TaskBase {
public:
    FilterSequenceTask() : TaskBase("FilterSequence") { setupParameters(); }
    FilterSequenceTask(const std::string& name, const json& config)
        : TaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "FilterSequence"; }
    static std::string getStaticTaskTypeName() { return "FilterSequence"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief RGB color imaging sequence
 *
 * Specialized sequence for LRGB imaging with separate exposure
 * settings and counts for each color channel.
 */
class RGBSequenceTask : public TaskBase {
public:
    RGBSequenceTask() : TaskBase("RGBSequence") { setupParameters(); }
    RGBSequenceTask(const std::string& name, const json& config)
        : TaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "RGBSequence"; }
    static std::string getStaticTaskTypeName() { return "RGBSequence"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Narrowband filter imaging sequence (Ha, OIII, SII)
 *
 * Optimized sequence for narrowband imaging with support for
 * different color palettes (SHO/Hubble, HOO, HOS).
 */
class NarrowbandSequenceTask : public TaskBase {
public:
    NarrowbandSequenceTask() : TaskBase("NarrowbandSequence") {
        setupParameters();
    }
    NarrowbandSequenceTask(const std::string& name, const json& config)
        : TaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "NarrowbandSequence"; }
    static std::string getStaticTaskTypeName() { return "NarrowbandSequence"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief LRGB imaging sequence with luminance
 */
class LRGBSequenceTask : public TaskBase {
public:
    LRGBSequenceTask() : TaskBase("LRGBSequence") { setupParameters(); }
    LRGBSequenceTask(const std::string& name, const json& config)
        : TaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "LRGBSequence"; }
    static std::string getStaticTaskTypeName() { return "LRGBSequence"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

}  // namespace lithium::task::filterwheel

#endif  // LITHIUM_TASK_COMPONENTS_FILTERWHEEL_TASKS_HPP
