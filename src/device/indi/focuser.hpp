#ifndef LITHIUM_CLIENT_INDI_FOCUSER_HPP
#define LITHIUM_CLIENT_INDI_FOCUSER_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <optional>
#include <string_view>
#include <vector>
#include <array>
#include <spdlog/spdlog.h>

#include "device/template/focuser.hpp"

class INDIFocuser : public INDI::BaseClient, public AtomFocuser {
public:
    explicit INDIFocuser(std::string name);
    ~INDIFocuser() override = default;

    // Non-copyable, non-movable due to atomic members
    INDIFocuser(const INDIFocuser& other) = delete;
    INDIFocuser& operator=(const INDIFocuser& other) = delete;
    INDIFocuser(INDIFocuser&& other) = delete;
    INDIFocuser& operator=(INDIFocuser&& other) = delete;

    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout,
                 int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto reconnect(int timeout, int maxRetry) -> bool;
    auto scan() -> std::vector<std::string> override;
    [[nodiscard]] auto isConnected() const -> bool override;

    virtual auto watchAdditionalProperty() -> bool;

    void setPropertyNumber(std::string_view propertyName, double value);

    auto getSpeed() -> std::optional<double> override;
    auto setSpeed(double speed) -> bool override;

    auto getDirection() -> std::optional<FocusDirection> override;
    auto setDirection(FocusDirection direction) -> bool override;

    auto getMaxLimit() -> std::optional<int> override;
    auto setMaxLimit(int maxLimit) -> bool override;

    auto isReversed() -> std::optional<bool> override;
    auto setReversed(bool reversed) -> bool override;

    auto moveSteps(int steps) -> bool override;
    auto moveToPosition(int position) -> bool override;
    auto getPosition() -> std::optional<int> override;
    auto moveForDuration(int durationMs) -> bool override;
    auto abortMove() -> bool override;
    auto syncPosition(int position) -> bool override;

    auto getExternalTemperature() -> std::optional<double> override;
    auto getChipTemperature() -> std::optional<double> override;

    // Additional methods from AtomFocuser that need implementation
    auto isMoving() const -> bool override;
    auto getMaxSpeed() -> int override;
    auto getSpeedRange() -> std::pair<int, int> override;
    auto getMinLimit() -> std::optional<int> override;
    auto setMinLimit(int minLimit) -> bool override;
    
    auto moveInward(int steps) -> bool override;
    auto moveOutward(int steps) -> bool override;
    
    auto getBacklash() -> int override;
    auto setBacklash(int backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;
    
    auto hasTemperatureSensor() -> bool override;
    auto getTemperatureCompensation() -> TemperatureCompensation override;
    auto setTemperatureCompensation(const TemperatureCompensation& comp) -> bool override;
    auto enableTemperatureCompensation(bool enable) -> bool override;
    
    auto startAutoFocus() -> bool override;
    auto stopAutoFocus() -> bool override;
    auto isAutoFocusing() -> bool override;
    auto getAutoFocusProgress() -> double override;
    
    auto savePreset(int slot, int position) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<int> override;
    auto deletePreset(int slot) -> bool override;
    
    auto getTotalSteps() -> uint64_t override;
    auto resetTotalSteps() -> bool override;
    auto getLastMoveSteps() -> int override;
    auto getLastMoveDuration() -> int override;

protected:
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;

private:
    std::string name_;
    std::string deviceName_;
    std::shared_ptr<spdlog::logger> logger_;

    std::string driverExec_;
    std::string driverVersion_;
    std::string driverInterface_;
    bool deviceAutoSearch_;
    bool devicePortScan_;

    std::atomic<double> currentPollingPeriod_;

    std::atomic_bool isDebug_;
    std::atomic_bool isConnected_;

    INDI::BaseDevice device_;

    std::string devicePort_;
    BAUD_RATE baudRate_;

    std::atomic_bool isFocuserMoving_;
    FocusMode focusMode_;
    FocusDirection focusDirection_;
    std::atomic<double> currentFocusSpeed_;
    std::atomic_bool isReverse_;
    std::atomic<double> focusTimer_;

    std::atomic_int realRelativePosition_;
    std::atomic_int realAbsolutePosition_;
    int maxPosition_;
    int minPosition_{0};

    std::atomic_bool backlashEnabled_;
    std::atomic_int backlashSteps_;

    std::atomic<double> temperature_;
    std::atomic<double> chipTemperature_;

    int delay_msec_;
    
    // Additional state for missing features
    std::atomic_bool isAutoFocusing_{false};
    std::atomic<double> autoFocusProgress_{0.0};
    std::atomic<uint64_t> totalSteps_{0};
    std::atomic_int lastMoveSteps_{0};
    std::atomic_int lastMoveDuration_{0};
    
    // Presets storage
    std::array<std::optional<int>, 10> presets_;
    
    // Temperature compensation state
    TemperatureCompensation tempCompensation_;
    std::atomic_bool tempCompensationEnabled_{false};
};

#endif
