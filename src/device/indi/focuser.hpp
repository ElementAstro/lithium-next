#ifndef LITHIUM_CLIENT_INDI_FOCUSER_HPP
#define LITHIUM_CLIENT_INDI_FOCUSER_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <optional>
#include <string_view>
#include <vector>

#include "device/template/focuser.hpp"

class INDIFocuser : public INDI::BaseClient, public AtomFocuser {
public:
    explicit INDIFocuser(std::string name);
    ~INDIFocuser() override = default;

    // 拷贝构造函数
    INDIFocuser(const INDIFocuser& other) = default;
    // 拷贝赋值运算符
    INDIFocuser& operator=(const INDIFocuser& other) = default;
    // 移动构造函数
    INDIFocuser(INDIFocuser&& other) noexcept = default;
    // 移动赋值运算符
    INDIFocuser& operator=(INDIFocuser&& other) noexcept = default;

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

protected:
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;

private:
    std::string name_;
    std::string deviceName_;

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

    std::atomic_bool backlashEnabled_;
    std::atomic_int backlashSteps_;

    std::atomic<double> temperature_;
    std::atomic<double> chipTemperature_;

    int delay_msec_;
};

#endif