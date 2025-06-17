#ifndef LITHIUM_INDI_FOCUSER_MODULAR_FOCUSER_HPP
#define LITHIUM_INDI_FOCUSER_MODULAR_FOCUSER_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>
#include <memory>

#include "device/template/focuser.hpp"
#include "movement_controller.hpp"
#include "preset_manager.hpp"
#include "property_manager.hpp"
#include "statistics_manager.hpp"
#include "temperature_manager.hpp"
#include "types.hpp"

namespace lithium::device::indi::focuser {

/**
 * @brief Modular INDI Focuser implementation
 *
 * This class orchestrates various components to provide complete focuser
 * functionality while maintaining clean separation of concerns.
 */
class ModularINDIFocuser : public INDI::BaseClient, public AtomFocuser {
public:
    explicit ModularINDIFocuser(std::string name);
    ~ModularINDIFocuser() override = default;

    // Non-copyable, non-movable due to atomic members
    ModularINDIFocuser(const ModularINDIFocuser& other) = delete;
    ModularINDIFocuser& operator=(const ModularINDIFocuser& other) = delete;
    ModularINDIFocuser(ModularINDIFocuser&& other) = delete;
    ModularINDIFocuser& operator=(ModularINDIFocuser&& other) = delete;

    // AtomFocuser interface implementation
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName, int timeout, int maxRetry)
        -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    [[nodiscard]] auto isConnected() const -> bool override;

    // Movement control (delegated to MovementController)
    auto isMoving() const -> bool override;
    auto getSpeed() -> std::optional<double> override;
    auto setSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> int override;
    auto getSpeedRange() -> std::pair<int, int> override;
    auto getDirection() -> std::optional<FocusDirection> override;
    auto setDirection(FocusDirection direction) -> bool override;
    auto getMaxLimit() -> std::optional<int> override;
    auto setMaxLimit(int maxLimit) -> bool override;
    auto getMinLimit() -> std::optional<int> override;
    auto setMinLimit(int minLimit) -> bool override;
    auto isReversed() -> std::optional<bool> override;
    auto setReversed(bool reversed) -> bool override;
    auto moveSteps(int steps) -> bool override;
    auto moveToPosition(int position) -> bool override;
    auto getPosition() -> std::optional<int> override;
    auto moveForDuration(int durationMs) -> bool override;
    auto abortMove() -> bool override;
    auto syncPosition(int position) -> bool override;
    auto moveInward(int steps) -> bool override;
    auto moveOutward(int steps) -> bool override;

    // Backlash compensation
    auto getBacklash() -> int override;
    auto setBacklash(int backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Temperature management (delegated to TemperatureManager)
    auto getExternalTemperature() -> std::optional<double> override;
    auto getChipTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;
    auto getTemperatureCompensation() -> TemperatureCompensation override;
    auto setTemperatureCompensation(const TemperatureCompensation& comp)
        -> bool override;
    auto enableTemperatureCompensation(bool enable) -> bool override;

    // Auto-focus (basic implementation)
    auto startAutoFocus() -> bool override;
    auto stopAutoFocus() -> bool override;
    auto isAutoFocusing() -> bool override;
    auto getAutoFocusProgress() -> double override;

    // Preset management (delegated to PresetManager)
    auto savePreset(int slot, int position) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<int> override;
    auto deletePreset(int slot) -> bool override;

    // Statistics (delegated to StatisticsManager)
    auto getTotalSteps() -> uint64_t override;
    auto resetTotalSteps() -> bool override;
    auto getLastMoveSteps() -> int override;
    auto getLastMoveDuration() -> int override;

    // Component access for advanced usage
    PropertyManager& getPropertyManager() { return *propertyManager_; }
    MovementController& getMovementController() { return *movementController_; }
    TemperatureManager& getTemperatureManager() { return *temperatureManager_; }
    PresetManager& getPresetManager() { return *presetManager_; }
    StatisticsManager& getStatisticsManager() { return *statisticsManager_; }

protected:
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;

private:
    // Shared state
    std::unique_ptr<FocuserState> state_;

    // Component managers
    std::unique_ptr<PropertyManager> propertyManager_;
    std::unique_ptr<MovementController> movementController_;
    std::unique_ptr<TemperatureManager> temperatureManager_;
    std::unique_ptr<PresetManager> presetManager_;
    std::unique_ptr<StatisticsManager> statisticsManager_;

    // Component initialization
    bool initializeComponents();
    void cleanupComponents();

    // Device connection helpers
    void setupDeviceWatchers();
    void setupInitialConnection(const std::string& deviceName);
};

}  // namespace lithium::device::indi::focuser

#endif  // LITHIUM_INDI_FOCUSER_MODULAR_FOCUSER_HPP
