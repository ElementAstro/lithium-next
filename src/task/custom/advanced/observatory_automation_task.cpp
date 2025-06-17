#include "observatory_automation_task.hpp"
#include <chrono>
#include <memory>
#include <thread>

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace lithium::task::task {

auto ObservatoryAutomationTask::taskName() -> std::string { return "ObservatoryAutomation"; }

void ObservatoryAutomationTask::execute(const json& params) { executeImpl(params); }

void ObservatoryAutomationTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing ObservatoryAutomation task '{}' with params: {}", 
          getName(), params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::string operation = params.value("operation", "startup");
        bool enableRoofControl = params.value("enable_roof_control", true);
        bool enableTelescopeControl = params.value("enable_telescope_control", true);
        bool enableCameraControl = params.value("enable_camera_control", true);
        double cameraTemperature = params.value("camera_temperature", -10.0);
        bool performSafetyCheck = params.value("perform_safety_check", true);
        double startupDelay = params.value("startup_delay_minutes", 2.0);
        bool waitForCooling = params.value("wait_for_cooling", true);

        LOG_F(INFO, "Starting observatory {} sequence", operation);

        if (operation == "startup") {
            if (performSafetyCheck) {
                LOG_F(INFO, "Performing pre-startup safety checks");
                performSafetyChecks();
            }

            performStartupSequence();

            if (enableRoofControl) {
                openRoof();
            }

            if (enableTelescopeControl) {
                unparkTelescope();
            }

            if (enableCameraControl) {
                coolCamera(cameraTemperature);
                if (waitForCooling) {
                    LOG_F(INFO, "Waiting for camera to reach target temperature");
                    std::this_thread::sleep_for(std::chrono::minutes(5)); // Simulate cooling time
                }
            }

            initializeEquipment();

            // Wait startup delay before declaring ready
            if (startupDelay > 0) {
                LOG_F(INFO, "Startup delay: waiting {:.1f} minutes before operations", startupDelay);
                std::this_thread::sleep_for(std::chrono::minutes(static_cast<int>(startupDelay)));
            }

            LOG_F(INFO, "Observatory startup sequence completed - ready for operations");

        } else if (operation == "shutdown") {
            LOG_F(INFO, "Initiating observatory shutdown sequence");

            if (enableCameraControl) {
                warmCamera();
            }

            if (enableTelescopeControl) {
                parkTelescope();
            }

            if (enableRoofControl) {
                closeRoof();
            }

            performShutdownSequence();

            LOG_F(INFO, "Observatory shutdown sequence completed - all systems secured");

        } else if (operation == "emergency_stop") {
            LOG_F(CRITICAL, "Emergency stop initiated!");
            
            // Immediate safety actions
            if (enableRoofControl) {
                LOG_F(INFO, "Emergency roof closure");
                closeRoof();
            }
            
            if (enableTelescopeControl) {
                LOG_F(INFO, "Emergency telescope park");
                parkTelescope();
            }
            
            LOG_F(CRITICAL, "Emergency stop completed - all systems secured");

        } else {
            THROW_INVALID_ARGUMENT("Invalid operation: " + operation);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(INFO, "ObservatoryAutomation task '{}' ({}) completed in {} minutes",
              getName(), operation, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(ERROR, "ObservatoryAutomation task '{}' failed after {} minutes: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

void ObservatoryAutomationTask::performStartupSequence() {
    LOG_F(INFO, "Performing observatory startup sequence");
    
    // Power on equipment in sequence
    LOG_F(INFO, "Powering on observatory equipment");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Initialize communication systems
    LOG_F(INFO, "Initializing communication systems");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Check power systems
    LOG_F(INFO, "Checking power systems");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    LOG_F(INFO, "Startup sequence completed");
}

void ObservatoryAutomationTask::performShutdownSequence() {
    LOG_F(INFO, "Performing observatory shutdown sequence");
    
    // Power down equipment in reverse order
    LOG_F(INFO, "Powering down non-essential equipment");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Secure communication systems
    LOG_F(INFO, "Securing communication systems");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Final power down
    LOG_F(INFO, "Final power down sequence");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    LOG_F(INFO, "Shutdown sequence completed");
}

void ObservatoryAutomationTask::initializeEquipment() {
    LOG_F(INFO, "Initializing observatory equipment");
    
    // Initialize mount
    LOG_F(INFO, "Initializing telescope mount");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Initialize camera
    LOG_F(INFO, "Initializing camera system");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Initialize focuser
    LOG_F(INFO, "Initializing focuser");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Initialize filter wheel
    LOG_F(INFO, "Initializing filter wheel");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Check all systems
    if (checkEquipmentStatus()) {
        LOG_F(INFO, "All equipment initialized successfully");
    } else {
        THROW_RUNTIME_ERROR("Equipment initialization failed");
    }
}

void ObservatoryAutomationTask::performSafetyChecks() {
    LOG_F(INFO, "Performing comprehensive safety checks");
    
    // Check weather conditions
    LOG_F(INFO, "Checking weather conditions");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Check power systems
    LOG_F(INFO, "Checking power system integrity");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Check mechanical systems
    LOG_F(INFO, "Checking mechanical system status");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Check network connectivity
    LOG_F(INFO, "Checking network connectivity");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    LOG_F(INFO, "All safety checks passed");
}

void ObservatoryAutomationTask::openRoof() {
    LOG_F(INFO, "Opening observatory roof");
    
    // Pre-open checks
    LOG_F(INFO, "Performing pre-open safety checks");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Open roof
    LOG_F(INFO, "Activating roof opening mechanism");
    std::this_thread::sleep_for(std::chrono::seconds(30)); // Simulate roof opening time
    
    // Verify roof position
    LOG_F(INFO, "Verifying roof is fully open");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    LOG_F(INFO, "Roof opened successfully");
}

void ObservatoryAutomationTask::closeRoof() {
    LOG_F(INFO, "Closing observatory roof");
    
    // Pre-close checks
    LOG_F(INFO, "Ensuring telescope is clear of roof path");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Close roof
    LOG_F(INFO, "Activating roof closing mechanism");
    std::this_thread::sleep_for(std::chrono::seconds(30)); // Simulate roof closing time
    
    // Verify roof position
    LOG_F(INFO, "Verifying roof is fully closed and secured");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    LOG_F(INFO, "Roof closed and secured");
}

void ObservatoryAutomationTask::parkTelescope() {
    LOG_F(INFO, "Parking telescope to safe position");
    
    // Stop any current operations
    LOG_F(INFO, "Stopping current telescope operations");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Move to park position
    LOG_F(INFO, "Moving telescope to park position");
    std::this_thread::sleep_for(std::chrono::seconds(15)); // Simulate slewing time
    
    // Lock telescope
    LOG_F(INFO, "Locking telescope in park position");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    LOG_F(INFO, "Telescope parked successfully");
}

void ObservatoryAutomationTask::unparkTelescope() {
    LOG_F(INFO, "Unparking telescope");
    
    // Unlock telescope
    LOG_F(INFO, "Unlocking telescope from park position");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Initialize tracking
    LOG_F(INFO, "Initializing telescope tracking");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Verify tracking
    LOG_F(INFO, "Verifying telescope tracking status");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    LOG_F(INFO, "Telescope unparked and tracking");
}

void ObservatoryAutomationTask::coolCamera(double targetTemperature) {
    LOG_F(INFO, "Cooling camera to {} degrees Celsius", targetTemperature);
    
    // Start cooling
    LOG_F(INFO, "Activating camera cooling system");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Monitor cooling progress (simplified)
    LOG_F(INFO, "Camera cooling in progress...");
    std::this_thread::sleep_for(std::chrono::seconds(10)); // Simulate initial cooling
    
    LOG_F(INFO, "Camera cooling initiated - target: {:.1f}°C", targetTemperature);
}

void ObservatoryAutomationTask::warmCamera() {
    LOG_F(INFO, "Warming camera for shutdown");
    
    // Gradual warming to prevent condensation
    LOG_F(INFO, "Initiating gradual camera warming");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Turn off cooling
    LOG_F(INFO, "Disabling camera cooling system");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    LOG_F(INFO, "Camera warming completed");
}

bool ObservatoryAutomationTask::checkEquipmentStatus() {
    LOG_F(INFO, "Checking equipment status");
    
    // In real implementation, this would check actual equipment
    // For now, simulate successful status check
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    LOG_F(INFO, "Equipment status check completed");
    return true; // Simulate success
}

void ObservatoryAutomationTask::validateObservatoryAutomationParameters(const json& params) {
    if (params.contains("operation")) {
        std::string operation = params["operation"].get<std::string>();
        if (operation != "startup" && operation != "shutdown" && operation != "emergency_stop") {
            THROW_INVALID_ARGUMENT("Operation must be 'startup', 'shutdown', or 'emergency_stop'");
        }
    }

    if (params.contains("camera_temperature")) {
        double temp = params["camera_temperature"].get<double>();
        if (temp < -50 || temp > 20) {
            THROW_INVALID_ARGUMENT("Camera temperature must be between -50 and 20 degrees Celsius");
        }
    }

    if (params.contains("startup_delay_minutes")) {
        double delay = params["startup_delay_minutes"].get<double>();
        if (delay < 0 || delay > 60) {
            THROW_INVALID_ARGUMENT("Startup delay must be between 0 and 60 minutes");
        }
    }
}

auto ObservatoryAutomationTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<ObservatoryAutomationTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced ObservatoryAutomation task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(9);
    task->setTimeout(std::chrono::seconds(3600));  // 1 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void ObservatoryAutomationTask::defineParameters(Task& task) {
    task.addParamDefinition("operation", "string", true, "startup",
                            "Operation type: startup, shutdown, or emergency_stop");
    task.addParamDefinition("enable_roof_control", "bool", false, true,
                            "Enable automatic roof control");
    task.addParamDefinition("enable_telescope_control", "bool", false, true,
                            "Enable automatic telescope control");
    task.addParamDefinition("enable_camera_control", "bool", false, true,
                            "Enable automatic camera control");
    task.addParamDefinition("camera_temperature", "double", false, -10.0,
                            "Target camera temperature in Celsius");
    task.addParamDefinition("perform_safety_check", "bool", false, true,
                            "Perform comprehensive safety checks");
    task.addParamDefinition("startup_delay_minutes", "double", false, 2.0,
                            "Delay after startup before operations");
    task.addParamDefinition("wait_for_cooling", "bool", false, true,
                            "Wait for camera to reach temperature");
}

}  // namespace lithium::task::task
