/*
 * filter_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Filter management operations

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_FILTERWHEEL_FILTER_MANAGER_HPP
#define LITHIUM_DEVICE_INDI_FILTERWHEEL_FILTER_MANAGER_HPP

#include "base.hpp"

class INDIFilterwheelFilterManager : public virtual INDIFilterwheelBase {
public:
    explicit INDIFilterwheelFilterManager(std::string name);
    ~INDIFilterwheelFilterManager() override = default;

    // Filter names and information
    auto getSlotName(int slot) -> std::optional<std::string> override;
    auto setSlotName(int slot, const std::string& name) -> bool override;
    auto getAllSlotNames() -> std::vector<std::string> override;
    auto getCurrentFilterName() -> std::string override;

    // Enhanced filter management
    auto getFilterInfo(int slot) -> std::optional<FilterInfo> override;
    auto setFilterInfo(int slot, const FilterInfo& info) -> bool override;
    auto getAllFilterInfo() -> std::vector<FilterInfo> override;

    // Filter search and selection
    auto findFilterByName(const std::string& name) -> std::optional<int> override;
    auto findFilterByType(const std::string& type) -> std::vector<int> override;
    auto selectFilterByName(const std::string& name) -> bool override;
    auto selectFilterByType(const std::string& type) -> bool override;

protected:
    // Helper methods
    auto validateSlotIndex(int slot) -> bool;
    void updateFilterCache();
    void notifyFilterChange(int slot, const std::string& name);
};

#endif // LITHIUM_DEVICE_INDI_FILTERWHEEL_FILTER_MANAGER_HPP
