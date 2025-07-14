/*
 * profiler.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_DEVICE_INDI_DOME_PROFILER_HPP
#define LITHIUM_DEVICE_INDI_DOME_PROFILER_HPP

#include "component_base.hpp"

namespace lithium::device::indi {

class DomeProfiler : public DomeComponentBase {
public:
    explicit DomeProfiler(std::shared_ptr<INDIDomeCore> core)
        : DomeComponentBase(std::move(core), "DomeProfiler") {}

    auto initialize() -> bool override { return true; }
    auto cleanup() -> bool override { return true; }
    void handlePropertyUpdate(const INDI::Property& property) override {}
};

} // namespace lithium::device::indi

#endif
