#ifndef LITHIUM_CLIENT_INDI_FILTERWHEEL_HPP
#define LITHIUM_CLIENT_INDI_FILTERWHEEL_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <optional>
#include <string_view>
#include <vector>

#include "device/template/filterwheel.hpp"

class INDIFilterwheel : public INDI::BaseClient, public AtomFilterWheel {
public:
    explicit INDIFilterwheel(std::string name);
    ~INDIFilterwheel() override = default;

    auto connect(const std::string &deviceName, int timeout,
                 int maxRetry) -> bool override;

    auto disconnect() -> bool override;

    auto scan() -> std::vector<std::string> override;

    virtual auto watchAdditionalProperty() -> bool;

    void setPropertyNumber(std::string_view propertyName, double value);

    auto getPosition()
        -> std::optional<std::tuple<double, double, double>> override;
    auto setPosition(int position) -> bool override;
    auto getSlotName() -> std::optional<std::string> override;
    auto setSlotName(std::string_view name) -> bool override;

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

    std::atomic_int currentSlot_;
    int maxSlot_;
    int minSlot_;
    std::string currentSlotName_;
    std::vector<std::string> slotNames_;
};

#endif
