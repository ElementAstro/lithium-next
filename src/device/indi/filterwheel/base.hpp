/*
 * base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Base INDI FilterWheel class definition

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_FILTERWHEEL_BASE_HPP
#define LITHIUM_DEVICE_INDI_FILTERWHEEL_BASE_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <optional>
#include <string_view>
#include <vector>
#include <memory>

#include <spdlog/spdlog.h>

#include "device/template/filterwheel.hpp"

class INDIFilterwheelBase : public INDI::BaseClient, public AtomFilterWheel {
public:
    explicit INDIFilterwheelBase(std::string name);
    ~INDIFilterwheelBase() override = default;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto isConnected() const -> bool override;

    // Connection management
    auto connect(const std::string &deviceName, int timeout = 3000,
                 int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;

    // INDI specific
    virtual auto watchAdditionalProperty() -> bool;
    void setPropertyNumber(std::string_view propertyName, double value);

protected:
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;

    // Device state
    std::string name_;
    std::string deviceName_;
    std::string driverExec_;
    std::string driverVersion_;
    std::string driverInterface_;

    std::atomic<bool> deviceAutoSearch_{false};
    std::atomic<bool> devicePortScan_{false};
    std::atomic<double> currentPollingPeriod_{1000.0};
    std::atomic<bool> isDebug_{false};
    std::atomic<bool> isConnected_{false};

    INDI::BaseDevice device_;

    // Filter state
    std::atomic<int> currentSlot_{0};
    int maxSlot_{8};
    int minSlot_{1};
    std::string currentSlotName_;
    std::vector<std::string> slotNames_;

    // Logger
    std::shared_ptr<spdlog::logger> logger_;

    // Helper methods
    virtual void setupPropertyWatchers();
    virtual void handleConnectionProperty(const INDI::PropertySwitch &property);
    virtual void handleDriverInfoProperty(const INDI::PropertyText &property);
    virtual void handleDebugProperty(const INDI::PropertySwitch &property);
    virtual void handlePollingProperty(const INDI::PropertyNumber &property);
    virtual void handleFilterSlotProperty(const INDI::PropertyNumber &property);
    virtual void handleFilterNameProperty(const INDI::PropertyText &property);
};

#endif // LITHIUM_DEVICE_INDI_FILTERWHEEL_BASE_HPP
