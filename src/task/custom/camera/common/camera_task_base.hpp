/**
 * @file camera_task_base.hpp
 * @brief Base class for all camera-related tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_CAMERA_BASE_HPP
#define LITHIUM_TASK_CAMERA_BASE_HPP

#include "../../common/task_base.hpp"
#include "../../common/types.hpp"
#include "../../common/validation.hpp"

namespace lithium::task::camera {

using namespace lithium::task;

/**
 * @brief Base class for camera-specific tasks
 *
 * Extends TaskBase with camera-specific functionality like
 * exposure validation and camera-specific parameters.
 */
class CameraTaskBase : public TaskBase {
public:
    using TaskBase::TaskBase;

    /**
     * @brief Constructor with task type name
     * @param taskType The task type identifier
     */
    explicit CameraTaskBase(const std::string& taskType)
        : TaskBase(taskType) {
        setupCameraParameters();
    }

    /**
     * @brief Constructor with name and config
     * @param name Task instance name
     * @param config Configuration parameters
     */
    CameraTaskBase(const std::string& name, const json& config)
        : TaskBase(name, config) {
        setupCameraParameters();
    }

    virtual ~CameraTaskBase() = default;

protected:
    /**
     * @brief Setup camera-specific parameters
     */
    void setupCameraParameters() {
        addParamDefinition("gain", "integer", false, 100, "Camera gain");
        addParamDefinition("offset", "integer", false, 10, "Camera offset");
        addParamDefinition("binning_x", "integer", false, 1, "Binning X");
        addParamDefinition("binning_y", "integer", false, 1, "Binning Y");
    }

    /**
     * @brief Validate exposure time
     */
    void validateExposure(double exposure, double minExp = 0.0, double maxExp = 86400.0) {
        if (exposure < minExp || exposure > maxExp) {
            throw std::invalid_argument("Exposure must be between " + 
                std::to_string(minExp) + " and " + std::to_string(maxExp));
        }
    }

    /**
     * @brief Validate gain value
     */
    void validateGain(int gain, int minGain = 0, int maxGain = 1000) {
        if (gain < minGain || gain > maxGain) {
            throw std::invalid_argument("Gain must be between " + 
                std::to_string(minGain) + " and " + std::to_string(maxGain));
        }
    }

    /**
     * @brief Validate required parameter exists
     */
    void validateRequired(const json& params, const std::string& key) {
        if (!params.contains(key) || params[key].is_null()) {
            throw std::invalid_argument("Required parameter missing: " + key);
        }
    }

    /**
     * @brief Validate parameter type
     */
    void validateType(const json& params, const std::string& key, const std::string& type) {
        if (!params.contains(key)) return;
        
        bool valid = false;
        if (type == "string") valid = params[key].is_string();
        else if (type == "number") valid = params[key].is_number();
        else if (type == "integer") valid = params[key].is_number_integer();
        else if (type == "boolean") valid = params[key].is_boolean();
        else if (type == "array") valid = params[key].is_array();
        else if (type == "object") valid = params[key].is_object();
        
        if (!valid) {
            throw std::invalid_argument("Parameter " + key + " must be of type " + type);
        }
    }
};

/**
 * @brief Macro for defining a camera task class
 */
#define DECLARE_CAMERA_TASK(ClassName, TaskTypeName) \
class ClassName : public CameraTaskBase { \
public: \
    ClassName() : CameraTaskBase(TaskTypeName) { setupParameters(); } \
    ClassName(const std::string& name, const json& config) \
        : CameraTaskBase(name, config) { setupParameters(); } \
    static std::string taskName() { return TaskTypeName; } \
    static std::string getStaticTaskTypeName() { return TaskTypeName; } \
protected: \
    void executeImpl(const json& params) override; \
private: \
    void setupParameters(); \
};

/**
 * @brief Exception class for validation errors
 */
class ValidationError : public std::runtime_error {
public:
    explicit ValidationError(const std::string& msg) : std::runtime_error(msg) {}
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_CAMERA_BASE_HPP
