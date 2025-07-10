/*
 * tracking_manager.hpp
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "device/template/telescope.hpp"

namespace lithium::device::ascom::telescope::components {

class HardwareInterface;

class TrackingManager {
public:
    explicit TrackingManager(std::shared_ptr<HardwareInterface> hardware);
    ~TrackingManager();

    bool isTracking() const;
    bool setTracking(bool enable);
    std::optional<TrackMode> getTrackingRate() const;
    bool setTrackingRate(TrackMode rate);
    MotionRates getTrackingRates() const;
    bool setTrackingRates(const MotionRates& rates);

    std::string getLastError() const;
    void clearError();

private:
    std::shared_ptr<HardwareInterface> hardware_;
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;

    void setLastError(const std::string& error) const;
};

} // namespace lithium::device::ascom::telescope::components
