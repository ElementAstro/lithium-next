/*
 * component_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "component_base.hpp"
#include "core/indi_dome_core.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::indi {

auto DomeComponentBase::isOurProperty(const INDI::Property& property) const -> bool {
    if (!property.isValid()) {
        return false;
    }
    
    auto core = getCore();
    if (!core) {
        return false;
    }
    
    return property.getDeviceName() == core->getDeviceName();
}

void DomeComponentBase::logInfo(const std::string& message) const {
    spdlog::info("[{}] {}", component_name_, message);
}

void DomeComponentBase::logWarning(const std::string& message) const {
    spdlog::warn("[{}] {}", component_name_, message);
}

void DomeComponentBase::logError(const std::string& message) const {
    spdlog::error("[{}] {}", component_name_, message);
}

} // namespace lithium::device::indi
