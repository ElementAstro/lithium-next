/*
 * configuration.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FilterWheel configuration management

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_FILTERWHEEL_CONFIGURATION_HPP
#define LITHIUM_DEVICE_INDI_FILTERWHEEL_CONFIGURATION_HPP

#include "base.hpp"
#include <filesystem>

// Forward declaration to avoid including the full header
namespace nlohmann {
    class json;
}

class INDIFilterwheelConfiguration : public virtual INDIFilterwheelBase {
public:
    explicit INDIFilterwheelConfiguration(std::string name);
    ~INDIFilterwheelConfiguration() override = default;

    // Configuration presets
    auto saveFilterConfiguration(const std::string& name) -> bool override;
    auto loadFilterConfiguration(const std::string& name) -> bool override;
    auto deleteFilterConfiguration(const std::string& name) -> bool override;
    auto getAvailableConfigurations() -> std::vector<std::string> override;

    // Configuration management
    auto exportConfiguration(const std::string& filename) -> bool;
    auto importConfiguration(const std::string& filename) -> bool;
    auto getConfigurationDetails(const std::string& name) -> std::optional<std::string>;

protected:
    std::filesystem::path getConfigurationPath() const;
    std::filesystem::path getConfigurationFile(const std::string& name) const;
    auto serializeCurrentConfiguration() -> std::string;
    auto deserializeConfiguration(const std::string& configStr) -> bool;
    
private:
    std::filesystem::path configBasePath_;
};

#endif // LITHIUM_DEVICE_INDI_FILTERWHEEL_CONFIGURATION_HPP
