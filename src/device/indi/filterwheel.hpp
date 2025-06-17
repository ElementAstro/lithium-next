#ifndef LITHIUM_CLIENT_INDI_FILTERWHEEL_HPP
#define LITHIUM_CLIENT_INDI_FILTERWHEEL_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <optional>
#include <string_view>
#include <vector>
#include <memory>

#include <spdlog/spdlog.h>

#include "device/template/filterwheel.hpp"

class INDIFilterwheel : public INDI::BaseClient, public AtomFilterWheel {
public:
    explicit INDIFilterwheel(std::string name);
    ~INDIFilterwheel() override = default;

    auto initialize() -> bool override;

    auto destroy() -> bool override;

    auto isConnected() const -> bool override;

    auto connect(const std::string &deviceName, int timeout,
                 int maxRetry) -> bool override;

    auto disconnect() -> bool override;

    auto scan() -> std::vector<std::string> override;

    virtual auto watchAdditionalProperty() -> bool;

    void setPropertyNumber(std::string_view propertyName, double value);

    auto getPositionDetails()
        -> std::optional<std::tuple<double, double, double>>;
    auto getPosition() -> std::optional<int> override;
    auto setPosition(int position) -> bool override;

    // Implementation of AtomFilterWheel interface  
    auto isMoving() const -> bool override;
    auto getFilterCount() -> int override;
    auto isValidPosition(int position) -> bool override;
    auto getSlotName(int slot) -> std::optional<std::string> override;
    auto setSlotName(int slot, const std::string& name) -> bool override;
    auto getAllSlotNames() -> std::vector<std::string> override;
    auto getCurrentFilterName() -> std::string override;
    auto getFilterInfo(int slot) -> std::optional<FilterInfo> override;
    auto setFilterInfo(int slot, const FilterInfo& info) -> bool override;
    auto getAllFilterInfo() -> std::vector<FilterInfo> override;
    auto findFilterByName(const std::string& name) -> std::optional<int> override;
    auto findFilterByType(const std::string& type) -> std::vector<int> override;
    auto selectFilterByName(const std::string& name) -> bool override;
    auto selectFilterByType(const std::string& type) -> bool override;
    auto abortMotion() -> bool override;
    auto homeFilterWheel() -> bool override;
    auto calibrateFilterWheel() -> bool override;
    auto getTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;
    auto getTotalMoves() -> uint64_t override;
    auto resetTotalMoves() -> bool override;
    auto getLastMoveTime() -> int override;
    auto saveFilterConfiguration(const std::string& name) -> bool override;
    auto loadFilterConfiguration(const std::string& name) -> bool override;
    auto deleteFilterConfiguration(const std::string& name) -> bool override;
    auto getAvailableConfigurations() -> std::vector<std::string> override;

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

    // Logger
    std::shared_ptr<spdlog::logger> logger_;
};

#endif
