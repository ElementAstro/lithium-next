#include "filterwheel.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"

#include "device/template/filterwheel.hpp"

#include "constant/constant.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

namespace {

struct FilterMeta {
    std::map<int, std::string> names;
    std::map<int, int> offsets;
};

std::mutex g_filterMetaMutex;
std::map<std::string, FilterMeta> g_filterMeta;

auto buildFilterList(const std::string &deviceId, int minSlot, int maxSlot)
    -> json {
    json filters = json::array();
    std::lock_guard<std::mutex> lock(g_filterMetaMutex);
    auto &meta = g_filterMeta[deviceId];
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

constexpr double DEFAULT_MOVE_TIME_SECONDS = 3.0;

}  // namespace

auto listFilterWheels() -> json {
    LOG_F(INFO, "listFilterWheels: Listing all available filter wheels");
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
            LOG_F(WARNING,
                  "listFilterWheels: Main filter wheel not available");
        }

        response["data"] = wheelList;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "listFilterWheels: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "listFilterWheels: Completed");
    return response;
}

auto getFilterWheelStatus(const std::string &deviceId) -> json {
    LOG_F(INFO, "getFilterWheelStatus: Getting status for filter wheel: %s",
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
            data["filters"] = buildFilterList(deviceId, minSlot, maxSlot);
        } else {
            data["position"] = nullptr;
            data["filters"] = json::array();
        }

        response["status"] = "success";
        response["data"] = data;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "getFilterWheelStatus: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "getFilterWheelStatus: Completed");
    return response;
}

auto connectFilterWheel(const std::string &deviceId, bool connected) -> json {
    LOG_F(INFO, "connectFilterWheel: %s filter wheel: %s",
          connected ? "Connecting" : "Disconnecting", deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomFilterWheel> wheel;
        GET_OR_CREATE_PTR(wheel, AtomFilterWheel, Constants::MAIN_FILTERWHEEL)

        bool success = connected ? wheel->connect("") : wheel->disconnect();
        if (success) {
            response["status"] = "success";
            response["message"] = connected
                                        ? "Filter wheel connection process "
                                          "initiated."
                                        : "Filter wheel disconnection process "
                                          "initiated.";
        } else {
            response["status"] = "error";
            response["error"] = {
                {"code", "connection_failed"},
                {"message", "Connection operation failed."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "connectFilterWheel: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "connectFilterWheel: Completed");
    return response;
}

auto setFilterPosition(const std::string &deviceId, const json &requestBody)
    -> json {
    LOG_F(INFO, "setFilterPosition: Moving filter wheel: %s",
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

        if (!requestBody.contains("position") ||
            !requestBody["position"].is_number_integer()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_filter_position"},
                {"message",
                 "Request must contain integer 'position' field"}};
            return response;
        }

        int position = requestBody["position"].get<int>();
        if (position < 1) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_filter_position"},
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
                std::lock_guard<std::mutex> lock(g_filterMetaMutex);
                auto &meta = g_filterMeta[deviceId];
                auto it = meta.names.find(position);
                if (it != meta.names.end()) {
                    targetName = it->second;
                } else {
                    targetName = "";
                }
            }

            data["targetFilterName"] = targetName;
            data["estimatedTime"] = DEFAULT_MOVE_TIME_SECONDS;

            response["data"] = data;
        } else {
            response["status"] = "error";
            response["error"] = {
                {"code", "move_failed"},
                {"message", "Filter wheel move command failed."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "setFilterPosition: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "setFilterPosition: Completed");
    return response;
}

auto setFilterByName(const std::string &deviceId, const json &requestBody)
    -> json {
    LOG_F(INFO, "setFilterByName: Moving filter wheel by name: %s",
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

        if (!requestBody.contains("filterName") ||
            !requestBody["filterName"].is_string()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_field_value"},
                {"message",
                 "Request must contain string 'filterName' field"}};
            return response;
        }

        std::string filterName = requestBody["filterName"].get<std::string>();

        int targetSlot = -1;
        {
            std::lock_guard<std::mutex> lock(g_filterMetaMutex);
            auto &meta = g_filterMeta[deviceId];
            for (const auto &entry : meta.names) {
                if (entry.second == filterName) {
                    targetSlot = entry.first;
                    break;
                }
            }
        }

        if (targetSlot < 0) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_filter_name"},
                {"message", "Filter name not found"}};
            return response;
        }

        json innerRequest;
        innerRequest["position"] = targetSlot;
        return setFilterPosition(deviceId, innerRequest);
    } catch (const std::exception &e) {
        LOG_F(ERROR, "setFilterByName: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "setFilterByName: Completed");
    return response;
}

auto getFilterWheelCapabilities(const std::string &deviceId) -> json {
    LOG_F(INFO,
          "getFilterWheelCapabilities: Getting capabilities for filter "
          "wheel: %s",
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
        data["moveTime"] = DEFAULT_MOVE_TIME_SECONDS;

        json positionNames = json::array();
        if (maxSlot >= minSlot) {
            std::lock_guard<std::mutex> lock(g_filterMetaMutex);
            auto &meta = g_filterMeta[deviceId];
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
    } catch (const std::exception &e) {
        LOG_F(ERROR,
              "getFilterWheelCapabilities: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "getFilterWheelCapabilities: Completed");
    return response;
}

auto configureFilterNames(const std::string &deviceId,
                          const json &requestBody) -> json {
    LOG_F(INFO, "configureFilterNames: Configuring filter names for: %s",
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
            std::lock_guard<std::mutex> lock(g_filterMetaMutex);
            auto &meta = g_filterMeta[deviceId];
            meta.names.clear();

            for (const auto &item : requestBody["filters"]) {
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
    } catch (const std::exception &e) {
        LOG_F(ERROR, "configureFilterNames: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "configureFilterNames: Completed");
    return response;
}

auto getFilterOffsets(const std::string &deviceId) -> json {
    LOG_F(INFO, "getFilterOffsets: Getting filter offsets for: %s",
          deviceId.c_str());
    json response;

    try {
        json offsets = json::array();

        {
            std::lock_guard<std::mutex> lock(g_filterMetaMutex);
            auto &meta = g_filterMeta[deviceId];
            for (const auto &entry : meta.offsets) {
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
    } catch (const std::exception &e) {
        LOG_F(ERROR, "getFilterOffsets: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "getFilterOffsets: Completed");
    return response;
}

auto setFilterOffsets(const std::string &deviceId, const json &requestBody)
    -> json {
    LOG_F(INFO, "setFilterOffsets: Setting filter offsets for: %s",
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
            std::lock_guard<std::mutex> lock(g_filterMetaMutex);
            auto &meta = g_filterMeta[deviceId];
            meta.offsets.clear();

            for (const auto &item : requestBody["offsets"]) {
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
    } catch (const std::exception &e) {
        LOG_F(ERROR, "setFilterOffsets: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "setFilterOffsets: Completed");
    return response;
}

auto haltFilterWheel(const std::string &deviceId) -> json {
    LOG_F(INFO, "haltFilterWheel: Halting filter wheel: %s",
          deviceId.c_str());
    json response;

    try {
        (void)deviceId;
        response["status"] = "error";
        response["error"] = {
            {"code", "feature_not_supported"},
            {"message", "Halting the filter wheel is not supported."}};
    } catch (const std::exception &e) {
        LOG_F(ERROR, "haltFilterWheel: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "haltFilterWheel: Completed");
    return response;
}

auto calibrateFilterWheel(const std::string &deviceId) -> json {
    LOG_F(INFO, "calibrateFilterWheel: Calibrating filter wheel: %s",
          deviceId.c_str());
    json response;

    try {
        (void)deviceId;
        response["status"] = "error";
        response["error"] = {
            {"code", "feature_not_supported"},
            {"message",
             "Filter wheel calibration is not implemented."}};
    } catch (const std::exception &e) {
        LOG_F(ERROR, "calibrateFilterWheel: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                              {"message", e.what()}};
    }

    LOG_F(INFO, "calibrateFilterWheel: Completed");
    return response;
}

}  // namespace lithium::middleware
