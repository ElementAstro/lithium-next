#include "indi_filterwheel_core.hpp"

namespace lithium::device::indi::filterwheel {

INDIFilterWheelCore::INDIFilterWheelCore(std::string name)
    : name_(std::move(name)) {
    // Initialize logger
    logger_ = spdlog::get("filterwheel");
    if (!logger_) {
        logger_ = spdlog::default_logger();
    }

    logger_->info("Creating INDI FilterWheel core: {}", name_);

    // Initialize default slot names
    slotNames_.resize(maxSlot_);
    for (int i = 0; i < maxSlot_; ++i) {
        slotNames_[i] = "Filter " + std::to_string(i + 1);
    }
}

void INDIFilterWheelCore::notifyPositionChange(int position, const std::string& filterName) {
    if (positionCallback_) {
        try {
            positionCallback_(position, filterName);
        } catch (const std::exception& e) {
            logger_->error("Error in position callback: {}", e.what());
        }
    }
}

void INDIFilterWheelCore::notifyMoveComplete(bool success, const std::string& message) {
    if (moveCompleteCallback_) {
        try {
            moveCompleteCallback_(success, message);
        } catch (const std::exception& e) {
            logger_->error("Error in move complete callback: {}", e.what());
        }
    }
}

void INDIFilterWheelCore::notifyTemperatureChange(double temperature) {
    if (temperatureCallback_) {
        try {
            temperatureCallback_(temperature);
        } catch (const std::exception& e) {
            logger_->error("Error in temperature callback: {}", e.what());
        }
    }
}

void INDIFilterWheelCore::notifyConnectionChange(bool connected) {
    if (connectionCallback_) {
        try {
            connectionCallback_(connected);
        } catch (const std::exception& e) {
            logger_->error("Error in connection callback: {}", e.what());
        }
    }
}

}  // namespace lithium::device::indi::filterwheel
