/*
 * driver_collection.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_CLIENT_INDI_MANAGER_DRIVER_COLLECTION_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_DRIVER_COLLECTION_HPP

#include "device_container.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json_fwd.hpp"

namespace tinyxml2 {
class XMLElement;
}

namespace lithium::client::indi_manager {

using json = nlohmann::json;

/**
 * @class DriverCollection
 * @brief A class to manage and parse INDI driver collections.
 */
class DriverCollection {
public:
    auto parseDrivers(const std::string& path) -> bool;
    auto parseDevice(tinyxml2::XMLElement* device, const char* family)
        -> std::shared_ptr<DeviceContainer>;
    auto collectXMLFiles(const std::string& path) -> bool;
    auto parseCustomDrivers(const json& drivers) -> bool;
    void clearCustomDrivers();

    auto getByLabel(const std::string& label) -> std::shared_ptr<DeviceContainer>;
    auto getByName(const std::string& name) -> std::shared_ptr<DeviceContainer>;
    auto getByBinary(const std::string& binary) -> std::shared_ptr<DeviceContainer>;
    auto getFamilies() -> std::unordered_map<std::string, std::vector<std::string>>;

private:
    std::string path_;
    std::vector<std::string> files_;
    std::vector<std::shared_ptr<DeviceContainer>> drivers_;
};

// Backward compatibility alias
using INDIDriverCollection = DriverCollection;

}  // namespace lithium::client::indi_manager

#endif  // LITHIUM_CLIENT_INDI_MANAGER_DRIVER_COLLECTION_HPP
