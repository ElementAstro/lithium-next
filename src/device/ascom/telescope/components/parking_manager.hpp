/*
 * parking_manager.hpp
 */

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "device/template/telescope.hpp"

namespace lithium::device::ascom::telescope::components {

class HardwareInterface;

class ParkingManager {
public:
    explicit ParkingManager(std::shared_ptr<HardwareInterface> hardware);
    ~ParkingManager();

    bool isParked() const;
    bool park();
    bool unpark();
    bool canPark() const;
    std::optional<EquatorialCoordinates> getParkPosition() const;
    bool setParkPosition(double ra, double dec);
    bool isAtPark() const;

    std::string getLastError() const;
    void clearError();

private:
    std::shared_ptr<HardwareInterface> hardware_;
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;

    void setLastError(const std::string& error) const;
};

} // namespace lithium::device::ascom::telescope::components
