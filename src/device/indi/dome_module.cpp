/*
 * dome_module.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: INDI Dome Module with Modular Architecture

*************************************************/

#include "dome/modular_dome.hpp"

#include <spdlog/spdlog.h>
#include <memory>

namespace lithium::device::indi {

/**
 * @brief Factory function to create a modular INDI dome instance
 * @param name Dome device name
 * @return Shared pointer to AtomDome instance
 */
std::shared_ptr<AtomDome> createINDIDome(const std::string& name) {
    try {
        auto dome = std::make_shared<ModularINDIDome>(name);
        spdlog::info("Created modular INDI dome: {}", name);
        return dome;
    } catch (const std::exception& ex) {
        spdlog::error("Failed to create INDI dome '{}': {}", name, ex.what());
        return nullptr;
    }
}

/**
 * @brief Get dome module information
 * @return Module information string
 */
std::string getDomeModuleInfo() {
    return "Lithium INDI Dome Module v2.0 - Modular Architecture\n"
           "Features:\n"
           "- Modular component architecture\n"
           "- Robust INDI property handling\n"
           "- Motion control with backlash compensation\n"
           "- Shutter control with safety interlocks\n"
           "- Weather monitoring integration\n"
           "- Performance profiling and analytics\n"
           "- Event-driven callback system\n"
           "- Thread-safe operations";
}

/**
 * @brief Check if INDI dome is available
 * @return true if available, false otherwise
 */
bool isINDIDomeAvailable() {
    // Check if INDI libraries are available and accessible
    try {
        auto test_dome = std::make_shared<ModularINDIDome>("test");
        return test_dome != nullptr;
    } catch (...) {
        return false;
    }
}

} // namespace lithium::device::indi

// C-style interface for dynamic loading
extern "C" {

/**
 * @brief C interface to create INDI dome
 */
void* create_indi_dome(const char* name) {
    if (!name) {
        return nullptr;
    }

    try {
        auto dome = lithium::device::indi::createINDIDome(std::string(name));
        if (dome) {
            // Return raw pointer that caller must manage
            return new std::shared_ptr<AtomDome>(dome);
        }
    } catch (...) {
        spdlog::error("Exception in create_indi_dome");
    }

    return nullptr;
}

/**
 * @brief C interface to destroy INDI dome
 */
void destroy_indi_dome(void* dome_ptr) {
    if (dome_ptr) {
        try {
            auto* shared_dome = static_cast<std::shared_ptr<AtomDome>*>(dome_ptr);
            delete shared_dome;
        } catch (...) {
            spdlog::error("Exception in destroy_indi_dome");
        }
    }
}

/**
 * @brief C interface to get module information
 */
const char* get_dome_module_info() {
    static std::string info = lithium::device::indi::getDomeModuleInfo();
    return info.c_str();
}

/**
 * @brief C interface to check availability
 */
int is_indi_dome_available() {
    return lithium::device::indi::isINDIDomeAvailable() ? 1 : 0;
}

} // extern "C"
