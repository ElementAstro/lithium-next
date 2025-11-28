#ifndef LITHIUM_SERVER_COMMAND_GUIDER_HPP
#define LITHIUM_SERVER_COMMAND_GUIDER_HPP

#include "atom/memory/shared.hpp"
#include "config/configor.hpp"

#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

#include "constant/constant.hpp"
#include "utils/macro.hpp"

namespace lithium::middleware {
namespace internal {
inline uint8_t MSB(uint16_t i) { return static_cast<uint8_t>((i >> 8) & 0xFF); }

inline uint8_t LSB(uint16_t i) { return static_cast<uint8_t>(i & 0xFF); }

struct PHD2Command {
    uint16_t command;
    uint32_t data[256];
    bool busy;
};

class PHD2Controller {
private:
    std::unique_ptr<atom::connection::SharedMemory<PHD2Command>> shm_;
    static constexpr auto TIMEOUT = std::chrono::milliseconds(500);
    pid_t phd2_pid_;

public:
    PHD2Controller() : shm_(nullptr) {
        LOG_F(INFO, "Initializing PHD2 controller");
    }

    bool initialize() {
        try {
            LOG_F(INFO, "Terminating existing PHD2 instances");
            system("pkill phd2");

            LOG_F(DEBUG, "Creating shared memory for PHD2 communication");
            shm_ =
                std::make_unique<atom::connection::SharedMemory<PHD2Command>>(
                    "phd2_control");

            LOG_F(INFO, "Starting PHD2 application");
            phd2_pid_ = fork();
            if (phd2_pid_ == 0) {
                execl("/usr/bin/phd2", "phd2", nullptr);
                exit(1);
            }
            return phd2_pid_ > 0;
        } catch (const atom::connection::SharedMemoryException& e) {
            LOG_F(ERROR, "Failed to initialize PHD2: {}", e.what());
            return false;
        }
    }

    bool executeCommand(uint16_t cmd,
                        const std::vector<uint32_t>& params = {}) {
        try {
            PHD2Command command{};
            command.command = cmd;
            command.busy = true;

            if (!params.empty()) {
                std::memcpy(command.data, params.data(),
                            std::min(params.size() * sizeof(uint32_t),
                                     sizeof(command.data)));
            }

            LOG_F(DEBUG, "Executing PHD2 command: 0x{:04x}", cmd);
            shm_->write(command, TIMEOUT);

            auto result = shm_->read(TIMEOUT);
            return !result.busy;
        } catch (const atom::connection::SharedMemoryException& e) {
            LOG_F(ERROR, "Command execution failed: {}", e.what());
            return false;
        }
    }

    // PHD2 Specific Commands
    bool getVersion(std::string& version) {
        if (!executeCommand(0x01)) {
            LOG_F(ERROR, "Failed to get PHD2 version");
            return false;
        }

        auto result = shm_->read(TIMEOUT);
        version = std::string(reinterpret_cast<char*>(result.data));
        LOG_F(INFO, "PHD2 version: {}", version);
        return true;
    }

    bool startLooping() {
        LOG_F(INFO, "Starting PHD2 looping");
        return executeCommand(0x03);
    }

    bool stopLooping() {
        LOG_F(INFO, "Stopping PHD2 looping");
        return executeCommand(0x04);
    }

    bool autoFindStar() {
        LOG_F(INFO, "Auto finding star");
        return executeCommand(0x05);
    }

    bool startGuiding() {
        LOG_F(INFO, "Starting guiding");
        return executeCommand(0x06);
    }

    bool setExposureTime(unsigned int expTime) {
        LOG_F(INFO, "Setting exposure time: {} ms", expTime);
        return executeCommand(0x0b, {expTime});
    }

    bool setFocalLength(int focalLength) {
        LOG_F(INFO, "Setting focal length: {} mm", focalLength);
        return executeCommand(0x10, {static_cast<uint32_t>(focalLength)});
    }
};
}  // namespace internal
inline void guiderCanvasClick(int canvasWidth, int canvasHeight, int x, int y) {
    LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager,
                             Constants::CONFIG_MANAGER);
    int currentWidth = configManager->get("/quarcs/phd/canvas/width").value();
    int currentHeight = configManager->get("/quarcs/phd/canvas/height").value();
    if (currentWidth != 0 && currentHeight != 0) {
        x = x * currentWidth / canvasWidth;
        y = y * currentHeight / canvasHeight;
    }
    // internal::callPHDStarClick(x, y);
}

inline void guiderFocalLength(int focalLength) {
    LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager,
                             Constants::CONFIG_MANAGER);
    configManager->set("/quarcs/phd/focalLength", focalLength);
}

/**
 * @brief Connect to PHD2 (start process if needed).
 */
auto connectGuider() -> nlohmann::json;

/**
 * @brief Disconnect from PHD2 (stop process).
 */
auto disconnectGuider() -> nlohmann::json;

/**
 * @brief Start guiding.
 */
auto startGuiding() -> nlohmann::json;

/**
 * @brief Stop guiding.
 */
auto stopGuiding() -> nlohmann::json;

/**
 * @brief Dither guiding.
 */
auto ditherGuider(double pixels) -> nlohmann::json;

/**
 * @brief Get guider status.
 */
auto getGuiderStatus() -> nlohmann::json;

/**
 * @brief Get guider statistics.
 */
auto getGuiderStats() -> nlohmann::json;

/**
 * @brief Update guider settings.
 */
auto setGuiderSettings(const nlohmann::json& settings) -> nlohmann::json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_COMMAND_GUIDER_HPP
