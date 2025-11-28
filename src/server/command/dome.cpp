#include "dome.hpp"

#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>

#include "atom/log/loguru.hpp"
#include "device/template/dome.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

namespace {

// MockDome implementation for simulation
class MockDome : public AtomDome {
private:
    bool connected_ = false;
    double azimuth_ = 0.0;
    double target_azimuth_ = 0.0;
    ShutterStatus shutter_ = ShutterStatus::CLOSED;
    DomeState state_ = DomeState::IDLE;
    bool parked_ = true;
    bool slaved_ = false;
    std::mutex mutex_;

    // Simulation thread
    std::thread sim_thread_;
    bool running_ = true;

public:
    MockDome() : AtomDome("Mock Dome") {
        sim_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                updateSimulation();
            }
        });
    }

    ~MockDome() {
        running_ = false;
        if (sim_thread_.joinable()) {
            sim_thread_.join();
        }
    }

    void updateSimulation() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_) return;

        // Simulate azimuth movement
        if (state_ == DomeState::MOVING) {
            double diff = target_azimuth_ - azimuth_;
            // Handle wrap-around shortest path logic could be here, but simplified for mock
            if (std::abs(diff) < 1.0) {
                azimuth_ = target_azimuth_;
                state_ = DomeState::IDLE;
                if (parked_) {
                     // If we were parking, now we are parked
                }
            } else {
                double step = 2.0; // degrees per 100ms
                if (diff > 0) azimuth_ += step;
                else azimuth_ -= step;
                
                // Normalize
                if (azimuth_ >= 360.0) azimuth_ -= 360.0;
                if (azimuth_ < 0.0) azimuth_ += 360.0;
            }
        }

        // Simulate shutter movement
        if (shutter_ == ShutterStatus::OPENING) {
            // Simulate instantaneous open for now, or add timer
            shutter_ = ShutterStatus::OPEN;
        } else if (shutter_ == ShutterStatus::CLOSING) {
            shutter_ = ShutterStatus::CLOSED;
        }
    }

    bool connect(const std::string &connection_string) override {
        std::lock_guard<std::mutex> lock(mutex_);
        (void)connection_string;
        connected_ = true;
        return true;
    }

    bool disconnect() override {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = false;
        return true;
    }

    bool isConnected() const override {
        // Mutex is tricky in const, but for mock simple read is usually fine or make mutex mutable
        return connected_;
    }

    auto getAzimuth() -> std::optional<double> override {
        std::lock_guard<std::mutex> lock(mutex_);
        return azimuth_;
    }

    auto getAltitude() -> std::optional<double> override {
        return 90.0; // Fixed
    }

    auto setAzimuth(double azimuth) -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_) return false;
        if (parked_) parked_ = false; // Implicit unpark on move
        target_azimuth_ = azimuth;
        state_ = DomeState::MOVING;
        return true;
    }

    auto setAltitude(double altitude) -> bool override {
        (void)altitude;
        return false; // Not supported
    }

    auto getShutterStatus() -> std::optional<ShutterStatus> override {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutter_;
    }

    auto openShutter() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_) return false;
        if (shutter_ == ShutterStatus::OPEN) return true;
        shutter_ = ShutterStatus::OPENING;
        return true;
    }

    auto closeShutter() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_) return false;
        if (shutter_ == ShutterStatus::CLOSED) return true;
        shutter_ = ShutterStatus::CLOSING;
        return true;
    }

    auto isParked() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        return parked_;
    }

    auto park() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_) return false;
        target_azimuth_ = 0.0; // Assume Home/Park is 0
        state_ = DomeState::MOVING;
        parked_ = true;
        return true;
    }

    auto unpark() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_) return false;
        parked_ = false;
        return true;
    }

    auto findHome() -> bool override {
        return park(); // Use park as home for mock
    }

    auto getDomeState() -> std::optional<DomeState> override {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    auto canSlave() -> bool override { return true; }
    auto setSlaved(bool slaved) -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        slaved_ = slaved;
        return true;
    }
    auto isSlaved() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        return slaved_;
    }
};

// Singleton instance of MockDome
std::shared_ptr<MockDome> g_mockDome = std::make_shared<MockDome>();

} // namespace

auto listDomes() -> json {
    LOG_F(INFO, "listDomes: Listing all available domes");
    json response;
    response["status"] = "success";
    json list = json::array();
    json info;
    info["deviceId"] = "dom-001";
    info["name"] = g_mockDome->getName();
    info["connected"] = g_mockDome->isConnected();
    list.push_back(info);
    response["data"] = list;
    return response;
}

auto getDomeStatus(const std::string &deviceId) -> json {
    // (void)deviceId; // Assume single device for now
    LOG_F(INFO, "getDomeStatus: %s", deviceId.c_str());
    json response;

    if (deviceId != "dom-001") {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_found"}, {"message", "Device not found"}};
         return response;
    }

    if (!g_mockDome->isConnected()) {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_connected"}, {"message", "Dome not connected"}};
         return response;
    }

    json data;
    data["connected"] = true;
    auto az = g_mockDome->getAzimuth();
    data["azimuth"] = az.value_or(0.0);
    data["altitude"] = g_mockDome->getAltitude().value_or(90.0);
    
    auto shutter = g_mockDome->getShutterStatus();
    std::string shutterStr = "Unknown";
    if (shutter) {
        switch(*shutter) {
            case ShutterStatus::OPEN: shutterStr = "Open"; break;
            case ShutterStatus::CLOSED: shutterStr = "Closed"; break;
            case ShutterStatus::OPENING: shutterStr = "Opening"; break;
            case ShutterStatus::CLOSING: shutterStr = "Closing"; break;
            case ShutterStatus::ERROR: shutterStr = "Error"; break;
            default: break;
        }
    }
    data["shutterStatus"] = shutterStr;
    
    auto state = g_mockDome->getDomeState();
    std::string stateStr = "Idle";
    if (state) {
        switch(*state) {
            case DomeState::MOVING: stateStr = "Moving"; break;
            case DomeState::PARKED: stateStr = "Parked"; break;
            case DomeState::PARKING: stateStr = "Parking"; break;
            case DomeState::UNPARKING: stateStr = "Unparking"; break;
            case DomeState::ERROR: stateStr = "Error"; break;
            default: break;
        }
    }
    data["status"] = stateStr;
    data["slaved"] = g_mockDome->isSlaved();
    data["parked"] = g_mockDome->isParked();

    response["status"] = "success";
    response["data"] = data;
    return response;
}

auto connectDome(const std::string &deviceId, bool connected) -> json {
    LOG_F(INFO, "connectDome: %s %s", deviceId.c_str(), connected ? "connect" : "disconnect");
    json response;
    if (deviceId != "dom-001") {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_found"}, {"message", "Device not found"}};
         return response;
    }

    bool success = connected ? g_mockDome->connect("") : g_mockDome->disconnect();
    if (success) {
        response["status"] = "success";
        response["message"] = connected ? "Dome connected" : "Dome disconnected";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "connection_failed"}, {"message", "Failed"}};
    }
    return response;
}

auto slewDome(const std::string &deviceId, double azimuth) -> json {
    LOG_F(INFO, "slewDome: %s to %f", deviceId.c_str(), azimuth);
    json response;
    if (deviceId != "dom-001") {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_found"}, {"message", "Device not found"}};
         return response;
    }

    if (!g_mockDome->isConnected()) {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_connected"}, {"message", "Not connected"}};
         return response;
    }

    if (g_mockDome->setAzimuth(azimuth)) {
        response["status"] = "success";
        response["message"] = "Slewing initiated";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "slew_failed"}, {"message", "Slew failed"}};
    }
    return response;
}

auto shutterControl(const std::string &deviceId, bool open) -> json {
    LOG_F(INFO, "shutterControl: %s %s", deviceId.c_str(), open ? "open" : "close");
    json response;
    if (deviceId != "dom-001") {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_found"}, {"message", "Device not found"}};
         return response;
    }

    if (!g_mockDome->isConnected()) {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_connected"}, {"message", "Not connected"}};
         return response;
    }

    bool success = open ? g_mockDome->openShutter() : g_mockDome->closeShutter();
    if (success) {
        response["status"] = "success";
        response["message"] = open ? "Opening shutter" : "Closing shutter";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "shutter_failed"}, {"message", "Shutter op failed"}};
    }
    return response;
}

auto parkDome(const std::string &deviceId) -> json {
    LOG_F(INFO, "parkDome: %s", deviceId.c_str());
    json response;
    if (deviceId != "dom-001") {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_found"}, {"message", "Device not found"}};
         return response;
    }
    
    if (!g_mockDome->isConnected()) {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_connected"}, {"message", "Not connected"}};
         return response;
    }

    if (g_mockDome->park()) {
        response["status"] = "success";
        response["message"] = "Parking initiated";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "park_failed"}, {"message", "Park failed"}};
    }
    return response;
}

auto unparkDome(const std::string &deviceId) -> json {
    LOG_F(INFO, "unparkDome: %s", deviceId.c_str());
    json response;
    if (deviceId != "dom-001") {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_found"}, {"message", "Device not found"}};
         return response;
    }

    if (!g_mockDome->isConnected()) {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_connected"}, {"message", "Not connected"}};
         return response;
    }

    if (g_mockDome->unpark()) {
        response["status"] = "success";
        response["message"] = "Unparked";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "unpark_failed"}, {"message", "Unpark failed"}};
    }
    return response;
}

auto homeDome(const std::string &deviceId) -> json {
    LOG_F(INFO, "homeDome: %s", deviceId.c_str());
    json response;
    if (deviceId != "dom-001") {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_found"}, {"message", "Device not found"}};
         return response;
    }

    if (!g_mockDome->isConnected()) {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_connected"}, {"message", "Not connected"}};
         return response;
    }

    if (g_mockDome->findHome()) {
        response["status"] = "success";
        response["message"] = "Homing initiated";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "home_failed"}, {"message", "Home failed"}};
    }
    return response;
}

auto stopDome(const std::string &deviceId) -> json {
     LOG_F(INFO, "stopDome: %s", deviceId.c_str());
     // Mock stop logic: just set state to idle if moving
     json response;
     if (deviceId != "dom-001") {
         response["status"] = "error";
         response["error"] = {{"code", "device_not_found"}, {"message", "Device not found"}};
         return response;
    }
    // Ideally implement abort in AtomDome
    response["status"] = "success";
    response["message"] = "Stopped";
    return response;
}

auto getDomeCapabilities(const std::string &deviceId) -> json {
    (void)deviceId;
    json response;
    response["status"] = "success";
    
    json caps;
    caps["canPark"] = true;
    caps["canFindHome"] = true;
    caps["canSlaved"] = true;
    caps["hasShutter"] = true;
    caps["canAzimuth"] = true;
    caps["canAltitude"] = false;
    
    response["data"] = caps;
    return response;
}

}  // namespace lithium::middleware
