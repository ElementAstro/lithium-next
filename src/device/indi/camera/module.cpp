#include <spdlog/spdlog.h>
#include "indi_camera.hpp"

#include "atom/components/component.hpp"
#include "atom/components/module_macro.hpp"
#include "atom/components/registry.hpp"

using namespace lithium::device::indi::camera;

/**
 * @brief Module registration for component-based INDI camera
 *
 * This module integrates the new component-based INDI camera implementation
 * with the Atom component system, replacing the monolithic implementation.
 */
ATOM_MODULE(camera_indi_components, [](Component& component) {
    spdlog::info("Registering component-based INDI camera module");

    // Register the new component-based INDI camera factory
    component.def(
        "create_indi_camera",
        [](const std::string& deviceName) -> std::shared_ptr<INDICamera> {
            spdlog::info("Creating component-based INDI camera for device: {}",
                         deviceName);

            auto camera = std::make_shared<INDICamera>(deviceName);

            if (!camera->initialize()) {
                spdlog::error(
                    "Failed to initialize component-based INDI camera");
                return nullptr;
            }

            spdlog::info(
                "Component-based INDI camera created and initialized "
                "successfully");
            return camera;
        });

    // Register component access functions for advanced usage
    component.def("get_camera_core",
                  [](std::shared_ptr<INDICamera> camera) -> INDICameraCore* {
                      return camera ? camera->getCore() : nullptr;
                  });

    component.def(
        "get_exposure_controller",
        [](std::shared_ptr<INDICamera> camera) -> ExposureController* {
            return camera ? camera->getExposureController() : nullptr;
        });

    component.def("get_video_controller",
                  [](std::shared_ptr<INDICamera> camera) -> VideoController* {
                      return camera ? camera->getVideoController() : nullptr;
                  });

    component.def(
        "get_temperature_controller",
        [](std::shared_ptr<INDICamera> camera) -> TemperatureController* {
            return camera ? camera->getTemperatureController() : nullptr;
        });

    component.def(
        "get_hardware_controller",
        [](std::shared_ptr<INDICamera> camera) -> HardwareController* {
            return camera ? camera->getHardwareController() : nullptr;
        });

    component.def("get_image_processor",
                  [](std::shared_ptr<INDICamera> camera) -> ImageProcessor* {
                      return camera ? camera->getImageProcessor() : nullptr;
                  });

    component.def("get_sequence_manager",
                  [](std::shared_ptr<INDICamera> camera) -> SequenceManager* {
                      return camera ? camera->getSequenceManager() : nullptr;
                  });

    component.def("get_property_handler",
                  [](std::shared_ptr<INDICamera> camera) -> PropertyHandler* {
                      return camera ? camera->getPropertyHandler() : nullptr;
                  });

    // Register utility functions
    component.def("scan_indi_cameras", []() -> std::vector<std::string> {
        spdlog::info("Scanning for INDI cameras...");

        // Create a temporary camera instance to scan for devices
        auto scanner = std::make_unique<INDICamera>("scanner");
        auto devices = scanner->scan();

        spdlog::info("Found {} INDI camera devices", devices.size());
        return devices;
    });

    component.def("validate_indi_camera",
                  [](std::shared_ptr<INDICamera> camera) -> bool {
                      if (!camera) {
                          return false;
                      }

                      // Perform basic validation
                      return camera->isConnected();
                  });

    spdlog::info("Component-based INDI camera module registered successfully");
    spdlog::info(
        "Available components: Core, Exposure, Video, Temperature, Hardware, "
        "Image, Sequence, Properties");
});
