/*
 * statistics_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_DEVICE_INDI_DOME_STATISTICS_MANAGER_HPP
#define LITHIUM_DEVICE_INDI_DOME_STATISTICS_MANAGER_HPP

#include "component_base.hpp"

namespace lithium::device::indi {

class StatisticsManager : public DomeComponentBase {
public:
    explicit StatisticsManager(std::shared_ptr<INDIDomeCore> core)
        : DomeComponentBase(std::move(core), "StatisticsManager") {}
    
    auto initialize() -> bool override { return true; }
    auto cleanup() -> bool override { return true; }
    void handlePropertyUpdate(const INDI::Property& property) override {}
};

} // namespace lithium::device::indi

#endif
