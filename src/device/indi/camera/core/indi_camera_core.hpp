#ifndef LITHIUM_INDI_CAMERA_CORE_HPP
#define LITHIUM_INDI_CAMERA_CORE_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../../../template/camera.hpp"

namespace lithium::device::indi::camera {

// Forward declarations
class ComponentBase;

/**
 * @brief Core INDI camera functionality
 *
 * This class provides the foundational INDI camera operations including
 * device connection, property management, and basic INDI BaseClient functionality.
 * It serves as the central hub for all camera components.
 */
class INDICameraCore : public INDI::BaseClient {
public:
    explicit INDICameraCore(const std::string& deviceName);
    ~INDICameraCore() override = default;

    // Basic device operations
    auto initialize() -> bool;
    auto destroy() -> bool;
    auto connect(const std::string& deviceName, int timeout = 5000, int maxRetry = 3) -> bool;
    auto disconnect() -> bool;
    auto isConnected() const -> bool;
    auto scan() -> std::vector<std::string>;

    // Device access
    auto getDevice() -> INDI::BaseDevice&;
    auto getDevice() const -> const INDI::BaseDevice&;
    auto getDeviceName() const -> const std::string&;

    // Component management
    auto registerComponent(std::shared_ptr<ComponentBase> component) -> void;
    auto unregisterComponent(ComponentBase* component) -> void;

    // INDI BaseClient overrides
    void newDevice(INDI::BaseDevice device) override;
    void removeDevice(INDI::BaseDevice device) override;
    void newProperty(INDI::Property property) override;
    void updateProperty(INDI::Property property) override;
    void removeProperty(INDI::Property property) override;
    void serverConnected() override;
    void serverDisconnected(int exit_code) override;

    // Property utilities
    void sendNewProperty(INDI::Property property);
    auto getDevices() const -> std::vector<INDI::BaseDevice>;
    void setPropertyNumber(std::string_view propertyName, double value);

    // Device watching
    void watchDevice(const char* deviceName,
                     const std::function<void(INDI::BaseDevice)>& callback);
    void connectDevice(const char* deviceName);
    void disconnectDevice(const char* deviceName);

    // State management
    auto isServerConnected() const -> bool;
    auto updateCameraState(CameraState state) -> void;
    auto getCameraState() const -> CameraState;

    // Current frame access
    auto getCurrentFrame() -> std::shared_ptr<AtomCameraFrame>;
    auto setCurrentFrame(std::shared_ptr<AtomCameraFrame> frame) -> void;

protected:
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;

private:
    // Device information
    std::string deviceName_;
    std::string name_;

    // Connection state
    std::atomic_bool isConnected_{false};
    std::atomic_bool serverConnected_{false};
    CameraState currentState_{CameraState::IDLE};

    // INDI device management
    INDI::BaseDevice device_;
    std::map<std::string, std::function<void(INDI::BaseDevice)>> deviceCallbacks_;
    mutable std::mutex devicesMutex_;
    std::vector<INDI::BaseDevice> devices_;

    // Component management
    std::vector<std::shared_ptr<ComponentBase>> components_;
    mutable std::mutex componentsMutex_;

    // Current frame
    std::shared_ptr<AtomCameraFrame> currentFrame_;
    mutable std::mutex frameMutex_;

    // Helper methods
    auto findDevice(const std::string& name) -> INDI::BaseDevice;
    void notifyComponents(INDI::Property property);
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_CORE_HPP
