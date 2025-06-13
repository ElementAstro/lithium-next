#include "factory.hpp"
#include <stdexcept>
#include <algorithm>

namespace lithium::task::custom::focuser {

// Static registry for task creators
std::map<std::string, FocuserTaskFactory::TaskCreator>& FocuserTaskFactory::getTaskRegistry() {
    static std::map<std::string, TaskCreator> registry;
    return registry;
}

void FocuserTaskFactory::registerAllTasks() {
    auto& registry = getTaskRegistry();
    
    // Position tasks
    registry["focuser_position"] = createPositionTask;
    registry["focuser_move_absolute"] = createPositionTask;
    registry["focuser_move_relative"] = createPositionTask;
    registry["focuser_sync"] = createPositionTask;
    
    // Autofocus tasks
    registry["autofocus"] = createAutofocusTask;
    registry["autofocus_v_curve"] = createAutofocusTask;
    registry["autofocus_hyperbolic"] = createAutofocusTask;
    registry["autofocus_simple"] = createAutofocusTask;
    
    // Temperature tasks
    registry["temperature_compensation"] = createTemperatureCompensationTask;
    registry["temperature_monitor"] = createTemperatureMonitorTask;
    
    // Validation tasks
    registry["focus_validation"] = createValidationTask;
    registry["focus_quality_checker"] = createQualityCheckerTask;
    
    // Backlash tasks
    registry["backlash_compensation"] = createBacklashCompensationTask;
    registry["backlash_detector"] = createBacklashDetectorTask;
    
    // Calibration tasks
    registry["focus_calibration"] = createCalibrationTask;
    registry["quick_calibration"] = createQuickCalibrationTask;
    
    // Star analysis tasks
    registry["star_analysis"] = createStarAnalysisTask;
    registry["simple_star_detector"] = createSimpleStarDetectorTask;
}

std::shared_ptr<Task> FocuserTaskFactory::createTask(const std::string& task_name, const Json::Value& params) {
    auto& registry = getTaskRegistry();
    
    auto it = registry.find(task_name);
    if (it == registry.end()) {
        throw std::invalid_argument("Unknown focuser task: " + task_name);
    }
    
    try {
        return it->second(params);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create focuser task '" + task_name + "': " + e.what());
    }
}

std::vector<std::string> FocuserTaskFactory::getAvailableTaskNames() {
    auto& registry = getTaskRegistry();
    std::vector<std::string> names;
    
    for (const auto& pair : registry) {
        names.push_back(pair.first);
    }
    
    std::sort(names.begin(), names.end());
    return names;
}

bool FocuserTaskFactory::isTaskRegistered(const std::string& task_name) {
    auto& registry = getTaskRegistry();
    return registry.find(task_name) != registry.end();
}

void FocuserTaskFactory::registerTask(const std::string& task_name, TaskCreator creator) {
    auto& registry = getTaskRegistry();
    registry[task_name] = creator;
}

// Device extraction helpers
std::shared_ptr<device::Focuser> FocuserTaskFactory::extractFocuser(const Json::Value& params) {
    if (!params.isMember("focuser") || !params["focuser"].isString()) {
        throw std::invalid_argument("Focuser parameter is required and must be a string");
    }
    
    std::string focuser_name = params["focuser"].asString();
    
    // In a real implementation, this would get the focuser from a device manager
    // For now, we'll return nullptr and let the task handle it
    return nullptr; // DeviceManager::getInstance().getFocuser(focuser_name);
}

std::shared_ptr<device::Camera> FocuserTaskFactory::extractCamera(const Json::Value& params) {
    if (!params.isMember("camera") || !params["camera"].isString()) {
        throw std::invalid_argument("Camera parameter is required and must be a string");
    }
    
    std::string camera_name = params["camera"].asString();
    
    // In a real implementation, this would get the camera from a device manager
    return nullptr; // DeviceManager::getInstance().getCamera(camera_name);
}

std::shared_ptr<device::TemperatureSensor> FocuserTaskFactory::extractTemperatureSensor(const Json::Value& params) {
    if (!params.isMember("temperature_sensor") || !params["temperature_sensor"].isString()) {
        return nullptr; // Temperature sensor is optional
    }
    
    std::string sensor_name = params["temperature_sensor"].asString();
    
    // In a real implementation, this would get the sensor from a device manager
    return nullptr; // DeviceManager::getInstance().getTemperatureSensor(sensor_name);
}

// Task creators
std::shared_ptr<Task> FocuserTaskFactory::createPositionTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    
    FocuserPositionTask::Config config;
    
    if (params.isMember("position") && params["position"].isInt()) {
        config.target_position = params["position"].asInt();
        config.movement_type = FocuserPositionTask::MovementType::Absolute;
    } else if (params.isMember("steps") && params["steps"].isInt()) {
        config.target_position = params["steps"].asInt();
        config.movement_type = FocuserPositionTask::MovementType::Relative;
    } else if (params.isMember("sync") && params["sync"].isBool()) {
        config.movement_type = FocuserPositionTask::MovementType::Sync;
    }
    
    if (params.isMember("speed") && params["speed"].isInt()) {
        config.movement_speed = params["speed"].asInt();
    }
    
    if (params.isMember("timeout") && params["timeout"].isInt()) {
        config.timeout_seconds = std::chrono::seconds(params["timeout"].asInt());
    }
    
    return std::make_shared<FocuserPositionTask>(focuser, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createAutofocusTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    auto camera = extractCamera(params);
    
    AutofocusTask::Config config;
    
    if (params.isMember("algorithm") && params["algorithm"].isString()) {
        std::string algorithm = params["algorithm"].asString();
        if (algorithm == "v_curve") {
            config.algorithm = AutofocusTask::Algorithm::VCurve;
        } else if (algorithm == "hyperbolic") {
            config.algorithm = AutofocusTask::Algorithm::Hyperbolic;
        } else if (algorithm == "simple") {
            config.algorithm = AutofocusTask::Algorithm::Simple;
        }
    }
    
    if (params.isMember("initial_step_size") && params["initial_step_size"].isInt()) {
        config.initial_step_size = params["initial_step_size"].asInt();
    }
    
    if (params.isMember("fine_step_size") && params["fine_step_size"].isInt()) {
        config.fine_step_size = params["fine_step_size"].asInt();
    }
    
    if (params.isMember("max_iterations") && params["max_iterations"].isInt()) {
        config.max_iterations = params["max_iterations"].asInt();
    }
    
    if (params.isMember("tolerance") && params["tolerance"].isDouble()) {
        config.tolerance = params["tolerance"].asDouble();
    }
    
    if (params.isMember("search_range") && params["search_range"].isInt()) {
        config.search_range = params["search_range"].asInt();
    }
    
    return std::make_shared<AutofocusTask>(focuser, camera, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createTemperatureCompensationTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    auto sensor = extractTemperatureSensor(params);
    
    TemperatureCompensationTask::Config config;
    
    if (params.isMember("temperature_coefficient") && params["temperature_coefficient"].isDouble()) {
        config.temperature_coefficient = params["temperature_coefficient"].asDouble();
    }
    
    if (params.isMember("min_temperature_change") && params["min_temperature_change"].isDouble()) {
        config.min_temperature_change = params["min_temperature_change"].asDouble();
    }
    
    if (params.isMember("monitoring_interval") && params["monitoring_interval"].isInt()) {
        config.monitoring_interval = std::chrono::seconds(params["monitoring_interval"].asInt());
    }
    
    if (params.isMember("auto_compensation") && params["auto_compensation"].isBool()) {
        config.auto_compensation = params["auto_compensation"].asBool();
    }
    
    return std::make_shared<TemperatureCompensationTask>(focuser, sensor, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createTemperatureMonitorTask(const Json::Value& params) {
    auto sensor = extractTemperatureSensor(params);
    
    TemperatureMonitorTask::Config config;
    
    if (params.isMember("interval") && params["interval"].isInt()) {
        config.interval = std::chrono::seconds(params["interval"].asInt());
    }
    
    if (params.isMember("log_to_file") && params["log_to_file"].isBool()) {
        config.log_to_file = params["log_to_file"].asBool();
    }
    
    if (params.isMember("log_file_path") && params["log_file_path"].isString()) {
        config.log_file_path = params["log_file_path"].asString();
    }
    
    return std::make_shared<TemperatureMonitorTask>(sensor, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createValidationTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    auto camera = extractCamera(params);
    
    FocusValidationTask::Config config;
    
    if (params.isMember("hfr_threshold") && params["hfr_threshold"].isDouble()) {
        config.hfr_threshold = params["hfr_threshold"].asDouble();
    }
    
    if (params.isMember("fwhm_threshold") && params["fwhm_threshold"].isDouble()) {
        config.fwhm_threshold = params["fwhm_threshold"].asDouble();
    }
    
    if (params.isMember("min_star_count") && params["min_star_count"].isInt()) {
        config.min_star_count = params["min_star_count"].asInt();
    }
    
    if (params.isMember("validation_interval") && params["validation_interval"].isInt()) {
        config.validation_interval = std::chrono::seconds(params["validation_interval"].asInt());
    }
    
    if (params.isMember("auto_correction") && params["auto_correction"].isBool()) {
        config.auto_correction = params["auto_correction"].asBool();
    }
    
    return std::make_shared<FocusValidationTask>(focuser, camera, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createQualityCheckerTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    auto camera = extractCamera(params);
    
    FocusQualityChecker::Config config;
    
    if (params.isMember("exposure_time_ms") && params["exposure_time_ms"].isInt()) {
        config.exposure_time_ms = params["exposure_time_ms"].asInt();
    }
    
    if (params.isMember("use_binning") && params["use_binning"].isBool()) {
        config.use_binning = params["use_binning"].asBool();
    }
    
    if (params.isMember("binning_factor") && params["binning_factor"].isInt()) {
        config.binning_factor = params["binning_factor"].asInt();
    }
    
    return std::make_shared<FocusQualityChecker>(focuser, camera, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createBacklashCompensationTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    auto camera = extractCamera(params);
    
    BacklashCompensationTask::Config config;
    
    if (params.isMember("measurement_range") && params["measurement_range"].isInt()) {
        config.measurement_range = params["measurement_range"].asInt();
    }
    
    if (params.isMember("measurement_steps") && params["measurement_steps"].isInt()) {
        config.measurement_steps = params["measurement_steps"].asInt();
    }
    
    if (params.isMember("overshoot_steps") && params["overshoot_steps"].isInt()) {
        config.overshoot_steps = params["overshoot_steps"].asInt();
    }
    
    if (params.isMember("auto_measurement") && params["auto_measurement"].isBool()) {
        config.auto_measurement = params["auto_measurement"].asBool();
    }
    
    if (params.isMember("auto_compensation") && params["auto_compensation"].isBool()) {
        config.auto_compensation = params["auto_compensation"].asBool();
    }
    
    return std::make_shared<BacklashCompensationTask>(focuser, camera, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createBacklashDetectorTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    auto camera = extractCamera(params);
    
    BacklashDetector::Config config;
    
    if (params.isMember("test_range") && params["test_range"].isInt()) {
        config.test_range = params["test_range"].asInt();
    }
    
    if (params.isMember("test_steps") && params["test_steps"].isInt()) {
        config.test_steps = params["test_steps"].asInt();
    }
    
    if (params.isMember("settling_time") && params["settling_time"].isInt()) {
        config.settling_time = std::chrono::seconds(params["settling_time"].asInt());
    }
    
    return std::make_shared<BacklashDetector>(focuser, camera, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createCalibrationTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    auto camera = extractCamera(params);
    auto sensor = extractTemperatureSensor(params);
    
    FocusCalibrationTask::CalibrationConfig config;
    
    if (params.isMember("full_range_start") && params["full_range_start"].isInt()) {
        config.full_range_start = params["full_range_start"].asInt();
    }
    
    if (params.isMember("full_range_end") && params["full_range_end"].isInt()) {
        config.full_range_end = params["full_range_end"].asInt();
    }
    
    if (params.isMember("coarse_step_size") && params["coarse_step_size"].isInt()) {
        config.coarse_step_size = params["coarse_step_size"].asInt();
    }
    
    if (params.isMember("fine_step_size") && params["fine_step_size"].isInt()) {
        config.fine_step_size = params["fine_step_size"].asInt();
    }
    
    if (params.isMember("calibrate_temperature") && params["calibrate_temperature"].isBool()) {
        config.calibrate_temperature = params["calibrate_temperature"].asBool();
    }
    
    if (params.isMember("create_focus_model") && params["create_focus_model"].isBool()) {
        config.create_focus_model = params["create_focus_model"].asBool();
    }
    
    return std::make_shared<FocusCalibrationTask>(focuser, camera, sensor, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createQuickCalibrationTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    auto camera = extractCamera(params);
    
    QuickFocusCalibration::Config config;
    
    if (params.isMember("search_range") && params["search_range"].isInt()) {
        config.search_range = params["search_range"].asInt();
    }
    
    if (params.isMember("step_size") && params["step_size"].isInt()) {
        config.step_size = params["step_size"].asInt();
    }
    
    if (params.isMember("fine_step_size") && params["fine_step_size"].isInt()) {
        config.fine_step_size = params["fine_step_size"].asInt();
    }
    
    if (params.isMember("settling_time") && params["settling_time"].isInt()) {
        config.settling_time = std::chrono::seconds(params["settling_time"].asInt());
    }
    
    return std::make_shared<QuickFocusCalibration>(focuser, camera, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createStarAnalysisTask(const Json::Value& params) {
    auto focuser = extractFocuser(params);
    auto camera = extractCamera(params);
    
    StarAnalysisTask::Config config;
    
    if (params.isMember("detection_threshold") && params["detection_threshold"].isDouble()) {
        config.detection_threshold = params["detection_threshold"].asDouble();
    }
    
    if (params.isMember("min_star_radius") && params["min_star_radius"].isInt()) {
        config.min_star_radius = params["min_star_radius"].asInt();
    }
    
    if (params.isMember("max_star_radius") && params["max_star_radius"].isInt()) {
        config.max_star_radius = params["max_star_radius"].asInt();
    }
    
    if (params.isMember("detailed_psf_analysis") && params["detailed_psf_analysis"].isBool()) {
        config.detailed_psf_analysis = params["detailed_psf_analysis"].asBool();
    }
    
    if (params.isMember("save_detection_overlay") && params["save_detection_overlay"].isBool()) {
        config.save_detection_overlay = params["save_detection_overlay"].asBool();
    }
    
    return std::make_shared<StarAnalysisTask>(focuser, camera, config);
}

std::shared_ptr<Task> FocuserTaskFactory::createSimpleStarDetectorTask(const Json::Value& params) {
    auto camera = extractCamera(params);
    
    SimpleStarDetector::Config config;
    
    if (params.isMember("threshold_sigma") && params["threshold_sigma"].isDouble()) {
        config.threshold_sigma = params["threshold_sigma"].asDouble();
    }
    
    if (params.isMember("min_star_size") && params["min_star_size"].isInt()) {
        config.min_star_size = params["min_star_size"].asInt();
    }
    
    if (params.isMember("max_stars") && params["max_stars"].isInt()) {
        config.max_stars = params["max_stars"].asInt();
    }
    
    return std::make_shared<SimpleStarDetector>(camera, config);
}

// FocuserTaskConfigBuilder implementation

FocuserTaskConfigBuilder::FocuserTaskConfigBuilder() {
    config_ = Json::Value(Json::objectValue);
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withFocuser(const std::string& focuser_name) {
    config_["focuser"] = focuser_name;
    return *this;
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withCamera(const std::string& camera_name) {
    config_["camera"] = camera_name;
    return *this;
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withTemperatureSensor(const std::string& sensor_name) {
    config_["temperature_sensor"] = sensor_name;
    return *this;
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withAbsolutePosition(int position) {
    config_["position"] = position;
    return *this;
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withRelativePosition(int steps) {
    config_["steps"] = steps;
    return *this;
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withAutofocusAlgorithm(const std::string& algorithm) {
    config_["algorithm"] = algorithm;
    return *this;
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withFocusRange(int start, int end) {
    config_["range_start"] = start;
    config_["range_end"] = end;
    return *this;
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withStepSize(int coarse, int fine) {
    config_["coarse_step_size"] = coarse;
    config_["fine_step_size"] = fine;
    return *this;
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withTemperatureCoefficient(double coefficient) {
    config_["temperature_coefficient"] = coefficient;
    return *this;
}

FocuserTaskConfigBuilder& FocuserTaskConfigBuilder::withQualityThresholds(double hfr_threshold, double fwhm_threshold) {
    config_["hfr_threshold"] = hfr_threshold;
    config_["fwhm_threshold"] = fwhm_threshold;
    return *this;
}

Json::Value FocuserTaskConfigBuilder::build() const {
    return config_;
}

// FocuserWorkflowBuilder implementation

FocuserWorkflowBuilder::FocuserWorkflowBuilder() = default;

std::vector<FocuserWorkflowBuilder::WorkflowStep> FocuserWorkflowBuilder::createBasicAutofocusWorkflow() {
    std::vector<WorkflowStep> steps;
    
    // Step 1: Star analysis
    steps.push_back({
        "star_analysis",
        FocuserTaskConfigBuilder().withDetectionThreshold(3.0).build(),
        false,
        "Analyze stars for initial assessment"
    });
    
    // Step 2: Autofocus
    steps.push_back({
        "autofocus",
        FocuserTaskConfigBuilder()
            .withAutofocusAlgorithm("v_curve")
            .withStepSize(50, 5)
            .build(),
        true,
        "Perform V-curve autofocus"
    });
    
    // Step 3: Validation
    steps.push_back({
        "focus_validation",
        FocuserTaskConfigBuilder()
            .withQualityThresholds(3.0, 4.0)
            .withMinStars(3)
            .build(),
        false,
        "Validate focus quality"
    });
    
    return steps;
}

std::vector<FocuserWorkflowBuilder::WorkflowStep> FocuserWorkflowBuilder::createFullCalibrationWorkflow() {
    std::vector<WorkflowStep> steps;
    
    // Step 1: Backlash detection
    steps.push_back({
        "backlash_detector",
        FocuserTaskConfigBuilder().build(),
        false,
        "Detect backlash"
    });
    
    // Step 2: Full calibration
    steps.push_back({
        "focus_calibration",
        FocuserTaskConfigBuilder()
            .withCalibrationRange(-1000, 1000)
            .withCalibrationSteps(100, 10, 2)
            .build(),
        true,
        "Perform full focus calibration"
    });
    
    // Step 3: Temperature calibration
    steps.push_back({
        "temperature_compensation",
        FocuserTaskConfigBuilder()
            .withTemperatureCoefficient(0.0)
            .withAutoCompensation(true)
            .build(),
        false,
        "Set up temperature compensation"
    });
    
    return steps;
}

FocuserWorkflowBuilder& FocuserWorkflowBuilder::addStep(const std::string& task_name, 
                                                       const Json::Value& parameters,
                                                       bool required,
                                                       const std::string& description) {
    steps_.push_back({task_name, parameters, required, description});
    return *this;
}

std::vector<FocuserWorkflowBuilder::WorkflowStep> FocuserWorkflowBuilder::build() const {
    return steps_;
}

// FocuserTaskRegistrar implementation

FocuserTaskRegistrar::FocuserTaskRegistrar(const std::string& task_name, 
                                          FocuserTaskFactory::TaskCreator creator) {
    FocuserTaskFactory::registerTask(task_name, creator);
}

// FocuserTaskValidator implementation

bool FocuserTaskValidator::validateDeviceParameter(const Json::Value& params, const std::string& device_type) {
    return params.isMember(device_type) && params[device_type].isString() && 
           !params[device_type].asString().empty();
}

bool FocuserTaskValidator::validatePositionParameter(const Json::Value& params) {
    return params.isMember("position") && params["position"].isInt();
}

bool FocuserTaskValidator::validateAutofocusParameters(const Json::Value& params) {
    if (!validateDeviceParameter(params, "focuser") || 
        !validateDeviceParameter(params, "camera")) {
        return false;
    }
    
    if (params.isMember("initial_step_size") && 
        (!params["initial_step_size"].isInt() || params["initial_step_size"].asInt() <= 0)) {
        return false;
    }
    
    if (params.isMember("max_iterations") && 
        (!params["max_iterations"].isInt() || params["max_iterations"].asInt() <= 0)) {
        return false;
    }
    
    return true;
}

std::vector<std::string> FocuserTaskValidator::getValidationErrors(const std::string& task_name, 
                                                                  const Json::Value& params) {
    std::vector<std::string> errors;
    
    if (task_name == "autofocus" && !validateAutofocusParameters(params)) {
        errors.push_back("Invalid autofocus parameters");
    }
    
    // Add more task-specific validations...
    
    return errors;
}

} // namespace lithium::task::custom::focuser
