/*
 * filterwheel_service.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "filterwheel_service.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"

#include "constant/constant.hpp"
#include "device/template/filterwheel.hpp"

namespace lithium::device {

using json = nlohmann::json;

class FilterWheelService::Impl {
public:
    struct FilterMeta {
        std::map<int, std::string> names;
        std::map<int, int> offsets;
    };

    std::mutex filterMetaMutex;
    std::map<std::string, FilterMeta> filterMeta;

    static constexpr double DEFAULT_MOVE_TIME_SECONDS = 3.0;

    auto buildFilterList(const std::string& deviceId, int minSlot, int maxSlot)
        -> json {
        json filters = json::array();
        std::lock_guard<std::mutex> lock(filterMetaMutex);
        auto& meta = filterMeta[deviceId];
        for (int slot = minSlot; slot <= maxSlot; ++slot) {
            json f;
            f["slot"] = slot;
            auto it = meta.names.find(slot);
            if (it != meta.names.end()) {
                f["name"] = it->second;
            } else {
                f["name"] = "";
            }
            filters.push_back(f);
        }
        return filters;
    }
};

FilterWheelService::FilterWheelService()
    : TypedDeviceService("FilterWheelService", "FilterWheel",
                         Constants::MAIN_FILTERWHEEL),
      impl_(std::make_unique<Impl>()) {}

FilterWheelService::~FilterWheelService() = default;

auto FilterWheelService::list() -> json {
    LOG_INFO( "FilterWheelService::list: Listing all available filter wheels");
    json response;
    response["status"] = "success";

    try {
        json wheelList = json::array();

        std::shared_ptr<AtomFilterWheel> wheel;
        try {
            GET_OR_CREATE_PTR(wheel, AtomFilterWheel,
                              Constants::MAIN_FILTERWHEEL)
            json info;
            info["deviceId"] = "fw-001";
            info["name"] = wheel->getName();
            info["isConnected"] = wheel->isConnected();
            wheelList.push_back(info);
        } catch (...) {
            LOG_WARN(
                  "FilterWheelService::list: Main filter wheel not available");
        }

        response["data"] = wheelList;
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::list: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::list: Completed");
    return response;
}

auto FilterWheelService::getStatus(const std::string& deviceId) -> json {
    LOG_INFO(
          "FilterWheelService::getStatus: Getting status for filter wheel: %s",
          deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFilterWheel> wheel;
        GET_OR_CREATE_PTR(wheel, AtomFilterWheel, Constants::MAIN_FILTERWHEEL)

        if (!wheel->isConnected()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "device_not_connected"},
                {"message", "Filter wheel is not connected"}};
            return response;
        }

        json data;
        data["isConnected"] = wheel->isConnected();
        data["isMoving"] = false;

        int currentSlot = 0;
        int minSlot = 1;
        int maxSlot = 0;

        if (auto pos = wheel->getPosition()) {
            double current;
            double minVal;
            double maxVal;
            std::tie(current, minVal, maxVal) = *pos;
            currentSlot = static_cast<int>(current);
            minSlot = static_cast<int>(minVal);
            maxSlot = static_cast<int>(maxVal);
            if (minSlot < 1) {
                minSlot = 1;
            }
            if (maxSlot < minSlot) {
                maxSlot = minSlot;
            }
            data["position"] = currentSlot;
            data["filters"] = impl_->buildFilterList(deviceId, minSlot, maxSlot);
        } else {
            data["position"] = nullptr;
            data["filters"] = json::array();
        }

        response["status"] = "success";
        response["data"] = data;
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::getStatus: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::getStatus: Completed");
    return response;
}

auto FilterWheelService::connect(const std::string& deviceId,
                                 bool connected) -> json {
    LOG_INFO( "FilterWheelService::connect: %s filter wheel: %s",
          connected ? "Connecting" : "Disconnecting", deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFilterWheel> wheel;
        GET_OR_CREATE_PTR(wheel, AtomFilterWheel, Constants::MAIN_FILTERWHEEL)

        bool success = connected ? wheel->connect("") : wheel->disconnect();
        if (success) {
            response["status"] = "success";
            response["message"] =
                connected ? "Filter wheel connection process initiated."
                          : "Filter wheel disconnection process initiated.";
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "connection_failed"},
                                 {"message", "Connection operation failed."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::connect: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::connect: Completed");
    return response;
}

auto FilterWheelService::setPosition(const std::string& deviceId,
                                     const json& requestBody) -> json {
    LOG_INFO( "FilterWheelService::setPosition: Moving filter wheel: %s",
          deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFilterWheel> wheel;
        GET_OR_CREATE_PTR(wheel, AtomFilterWheel, Constants::MAIN_FILTERWHEEL)

        if (!wheel->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Filter wheel is not connected"}};
            return response;
        }

        if (!requestBody.contains("position") ||
            !requestBody["position"].is_number_integer()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_filter_position"},
                {"message", "Request must contain integer 'position' field"}};
            return response;
        }

        int position = requestBody["position"].get<int>();
        if (position < 1) {
            response["status"] = "error";
            response["error"] = {{"code", "invalid_filter_position"},
                                 {"message", "Position must be >= 1"}};
            return response;
        }

        if (auto pos = wheel->getPosition()) {
            double current;
            double minVal;
            double maxVal;
            std::tie(current, minVal, maxVal) = *pos;
            int minSlot = static_cast<int>(minVal);
            int maxSlot = static_cast<int>(maxVal);
            if (minSlot < 1) {
                minSlot = 1;
            }
            if (maxSlot < minSlot) {
                maxSlot = minSlot;
            }
            if (position < minSlot || position > maxSlot) {
                response["status"] = "error";
                response["error"] = {
                    {"code", "invalid_filter_position"},
                    {"message", "Position is out of valid range"}};
                return response;
            }
        }

        bool success = wheel->setPosition(position);
        if (success) {
            response["status"] = "success";
            response["message"] = "Filter wheel move initiated.";

            json data;
            data["targetPosition"] = position;

            std::string targetName;
            {
                std::lock_guard<std::mutex> lock(impl_->filterMetaMutex);
                auto& meta = impl_->filterMeta[deviceId];
                auto it = meta.names.find(position);
                if (it != meta.names.end()) {
                    targetName = it->second;
                } else {
                    targetName = "";
                }
            }

            data["targetFilterName"] = targetName;
            data["estimatedTime"] = Impl::DEFAULT_MOVE_TIME_SECONDS;

            response["data"] = data;
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "move_failed"},
                                 {"message", "Filter wheel move command failed."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::setPosition: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::setPosition: Completed");
    return response;
}

auto FilterWheelService::setByName(const std::string& deviceId,
                                   const json& requestBody) -> json {
    LOG_INFO( "FilterWheelService::setByName: Moving filter wheel by name: %s",
          deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFilterWheel> wheel;
        GET_OR_CREATE_PTR(wheel, AtomFilterWheel, Constants::MAIN_FILTERWHEEL)

        if (!wheel->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Filter wheel is not connected"}};
            return response;
        }

        if (!requestBody.contains("filterName") ||
            !requestBody["filterName"].is_string()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_field_value"},
                {"message", "Request must contain string 'filterName' field"}};
            return response;
        }

        std::string filterName = requestBody["filterName"].get<std::string>();

        int targetSlot = -1;
        {
            std::lock_guard<std::mutex> lock(impl_->filterMetaMutex);
            auto& meta = impl_->filterMeta[deviceId];
            for (const auto& entry : meta.names) {
                if (entry.second == filterName) {
                    targetSlot = entry.first;
                    break;
                }
            }
        }

        if (targetSlot < 0) {
            response["status"] = "error";
            response["error"] = {{"code", "invalid_filter_name"},
                                 {"message", "Filter name not found"}};
            return response;
        }

        json innerRequest;
        innerRequest["position"] = targetSlot;
        return setPosition(deviceId, innerRequest);
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::setByName: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::setByName: Completed");
    return response;
}

auto FilterWheelService::getCapabilities(const std::string& deviceId) -> json {
    LOG_INFO(
          "FilterWheelService::getCapabilities: Getting capabilities for: %s",
          deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFilterWheel> wheel;
        GET_OR_CREATE_PTR(wheel, AtomFilterWheel, Constants::MAIN_FILTERWHEEL)

        json data;

        int minSlot = 1;
        int maxSlot = 0;

        if (auto pos = wheel->getPosition()) {
            double current;
            double minVal;
            double maxVal;
            std::tie(current, minVal, maxVal) = *pos;
            minSlot = static_cast<int>(minVal);
            maxSlot = static_cast<int>(maxVal);
            if (minSlot < 1) {
                minSlot = 1;
            }
            if (maxSlot < minSlot) {
                maxSlot = minSlot;
            }
            data["numPositions"] = maxSlot - minSlot + 1;
        } else {
            data["numPositions"] = 0;
        }

        data["canSetNames"] = true;
        data["canSetOffsets"] = true;
        data["supportsHalting"] = false;
        data["moveTime"] = Impl::DEFAULT_MOVE_TIME_SECONDS;

        json positionNames = json::array();
        if (maxSlot >= minSlot) {
            std::lock_guard<std::mutex> lock(impl_->filterMetaMutex);
            auto& meta = impl_->filterMeta[deviceId];
            for (int slot = minSlot; slot <= maxSlot; ++slot) {
                auto it = meta.names.find(slot);
                if (it != meta.names.end()) {
                    positionNames.push_back(it->second);
                } else {
                    positionNames.push_back("");
                }
            }
        }
        data["positionNames"] = positionNames;

        response["status"] = "success";
        response["data"] = data;
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::getCapabilities: Exception: %s",
              e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::getCapabilities: Completed");
    return response;
}

auto FilterWheelService::configureNames(const std::string& deviceId,
                                        const json& requestBody) -> json {
    LOG_INFO( "FilterWheelService::configureNames: Configuring filter names for: %s",
          deviceId.c_str());
    json response;

    try {
        if (!requestBody.contains("filters") ||
            !requestBody["filters"].is_array()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_field_value"},
                {"message", "Request must contain 'filters' array"}};
            return response;
        }

        {
            std::lock_guard<std::mutex> lock(impl_->filterMetaMutex);
            auto& meta = impl_->filterMeta[deviceId];
            meta.names.clear();

            for (const auto& item : requestBody["filters"]) {
                if (!item.contains("slot") ||
                    !item["slot"].is_number_integer() ||
                    !item.contains("name") || !item["name"].is_string()) {
                    continue;
                }
                int slot = item["slot"].get<int>();
                std::string name = item["name"].get<std::string>();
                meta.names[slot] = name;
            }
        }

        response["status"] = "success";
        response["message"] = "Filter names updated.";
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::configureNames: Exception: %s",
              e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::configureNames: Completed");
    return response;
}

auto FilterWheelService::getOffsets(const std::string& deviceId) -> json {
    LOG_INFO( "FilterWheelService::getOffsets: Getting filter offsets for: %s",
          deviceId.c_str());
    json response;

    try {
        json offsets = json::array();

        {
            std::lock_guard<std::mutex> lock(impl_->filterMetaMutex);
            auto& meta = impl_->filterMeta[deviceId];
            for (const auto& entry : meta.offsets) {
                int slot = entry.first;
                int offset = entry.second;

                json item;
                item["slot"] = slot;
                auto itName = meta.names.find(slot);
                if (itName != meta.names.end()) {
                    item["name"] = itName->second;
                } else {
                    item["name"] = "";
                }
                item["offset"] = offset;
                offsets.push_back(item);
            }
        }

        json data;
        data["offsets"] = offsets;

        response["status"] = "success";
        response["data"] = data;
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::getOffsets: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::getOffsets: Completed");
    return response;
}

auto FilterWheelService::setOffsets(const std::string& deviceId,
                                    const json& requestBody) -> json {
    LOG_INFO( "FilterWheelService::setOffsets: Setting filter offsets for: %s",
          deviceId.c_str());
    json response;

    try {
        if (!requestBody.contains("offsets") ||
            !requestBody["offsets"].is_array()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_field_value"},
                {"message", "Request must contain 'offsets' array"}};
            return response;
        }

        {
            std::lock_guard<std::mutex> lock(impl_->filterMetaMutex);
            auto& meta = impl_->filterMeta[deviceId];
            meta.offsets.clear();

            for (const auto& item : requestBody["offsets"]) {
                if (!item.contains("slot") ||
                    !item["slot"].is_number_integer() ||
                    !item.contains("offset") ||
                    !item["offset"].is_number_integer()) {
                    continue;
                }
                int slot = item["slot"].get<int>();
                int offset = item["offset"].get<int>();
                meta.offsets[slot] = offset;
            }
        }

        response["status"] = "success";
        response["message"] = "Filter offsets updated.";
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::setOffsets: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::setOffsets: Completed");
    return response;
}

auto FilterWheelService::halt(const std::string& deviceId) -> json {
    LOG_INFO( "FilterWheelService::halt: Halting filter wheel: %s",
          deviceId.c_str());
    json response;

    try {
        (void)deviceId;
        response["status"] = "error";
        response["error"] = {
            {"code", "feature_not_supported"},
            {"message", "Halting the filter wheel is not supported."}};
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::halt: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::halt: Completed");
    return response;
}

auto FilterWheelService::calibrate(const std::string& deviceId) -> json {
    LOG_INFO( "FilterWheelService::calibrate: Calibrating filter wheel: %s",
          deviceId.c_str());
    json response;

    try {
        (void)deviceId;
        response["status"] = "error";
        response["error"] = {
            {"code", "feature_not_supported"},
            {"message", "Filter wheel calibration is not implemented."}};
    } catch (const std::exception& e) {
        LOG_ERROR( "FilterWheelService::calibrate: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }

    LOG_INFO( "FilterWheelService::calibrate: Completed");
    return response;
}

// ========== INDI-specific operations ==========

auto FilterWheelService::getINDIProperties(const std::string& deviceId) -> json {
    return withConnectedDevice(deviceId, "getINDIProperties",
                               [this, &deviceId](auto wheel) -> json {
        json data;
        data["driverName"] = "INDI Filter Wheel";
        data["driverVersion"] = "1.0";

        json properties = json::object();

        // Current position
        if (auto pos = wheel->getPosition()) {
            double current;
            double minVal;
            double maxVal;
            std::tie(current, minVal, maxVal) = *pos;

            properties["FILTER_SLOT"] = {
                {"value", static_cast<int>(current)},
                {"min", static_cast<int>(minVal)},
                {"max", static_cast<int>(maxVal)},
                {"type", "number"}
            };
        }

        // Filter names
        {
            std::lock_guard<std::mutex> lock(impl_->filterMetaMutex);
            auto& meta = impl_->filterMeta[deviceId];
            json names = json::array();
            for (const auto& [slot, name] : meta.names) {
                names.push_back({{"slot", slot}, {"name", name}});
            }
            properties["FILTER_NAME"] = {
                {"value", names},
                {"type", "text"}
            };
        }

        data["properties"] = properties;
        return makeSuccessResponse(data);
    });
}

auto FilterWheelService::setINDIProperty(const std::string& deviceId,
                                         const std::string& propertyName,
                                         const json& value) -> json {
    return withConnectedDevice(deviceId, "setINDIProperty",
                               [&](auto wheel) -> json {
        bool success = false;

        if (propertyName == "FILTER_SLOT" && value.is_number_integer()) {
            success = wheel->setPosition(value.get<int>());
        } else if (propertyName == "FILTER_NAME" && value.is_object()) {
            if (value.contains("slot") && value.contains("name")) {
                int slot = value["slot"].get<int>();
                std::string name = value["name"].get<std::string>();

                std::lock_guard<std::mutex> lock(impl_->filterMetaMutex);
                impl_->filterMeta[deviceId].names[slot] = name;
                success = true;
            }
        } else {
            return makeErrorResponse(ErrorCode::INVALID_FIELD_VALUE,
                                     "Unknown or invalid property: " + propertyName);
        }

        if (success) {
            return makeSuccessResponse("Property " + propertyName + " updated");
        }
        return makeErrorResponse(ErrorCode::OPERATION_FAILED,
                                 "Failed to set property " + propertyName);
    });
}

}  // namespace lithium::device
