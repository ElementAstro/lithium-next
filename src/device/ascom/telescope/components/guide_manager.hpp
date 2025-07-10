/*
 * guide_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Guide Manager Component

*************************************************/

#pragma once

#include <memory>
#include <mutex>
#include <string>

namespace lithium::device::ascom::telescope::components {

class HardwareInterface;

/**
 * @brief Guide Manager for ASCOM Telescope
 */
class GuideManager {
public:
    explicit GuideManager(std::shared_ptr<HardwareInterface> hardware);
    ~GuideManager();

    // =========================================================================
    // Guide Operations
    // =========================================================================

    /**
     * @brief Send guide pulse
     * @param direction Guide direction (N, S, E, W)
     * @param duration Duration in milliseconds
     * @return true if pulse sent successfully
     */
    bool guidePulse(const std::string& direction, int duration);

    /**
     * @brief Send RA/DEC guide correction
     * @param ra_ms RA correction in milliseconds
     * @param dec_ms DEC correction in milliseconds
     * @return true if correction sent successfully
     */
    bool guideRADEC(double ra_ms, double dec_ms);

    /**
     * @brief Check if currently pulse guiding
     * @return true if pulse guiding active
     */
    bool isPulseGuiding() const;

    /**
     * @brief Get guide rates
     * @return Pair of RA, DEC guide rates in arcsec/sec
     */
    std::pair<double, double> getGuideRates() const;

    /**
     * @brief Set guide rates
     * @param ra_rate RA guide rate in arcsec/sec
     * @param dec_rate DEC guide rate in arcsec/sec
     * @return true if operation successful
     */
    bool setGuideRates(double ra_rate, double dec_rate);

    std::string getLastError() const;
    void clearError();

private:
    std::shared_ptr<HardwareInterface> hardware_;
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;

    void setLastError(const std::string& error) const;
    bool validateDirection(const std::string& direction) const;
};

} // namespace lithium::device::ascom::telescope::components
