/*
 * alignment_manager.hpp
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "device/template/telescope.hpp"

namespace lithium::device::ascom::telescope::components {

class HardwareInterface;

class AlignmentManager {
public:
    explicit AlignmentManager(std::shared_ptr<HardwareInterface> hardware);
    ~AlignmentManager();

    AlignmentMode getAlignmentMode() const;
    bool setAlignmentMode(AlignmentMode mode);
    bool addAlignmentPoint(const EquatorialCoordinates& measured,
                          const EquatorialCoordinates& target);
    bool clearAlignment();
    int getAlignmentPointCount() const;

    std::string getLastError() const;
    void clearError();

private:
    std::shared_ptr<HardwareInterface> hardware_;
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;

    void setLastError(const std::string& error) const;
};

} // namespace lithium::device::ascom::telescope::components
