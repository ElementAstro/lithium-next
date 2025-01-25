/*
 * focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomTelescope Simulator and Basic Definition

*************************************************/

#pragma once

#include <optional>
#include <string_view>
#include <tuple>
#include "device.hpp"

enum class ConnectionMode { SERIAL, TCP, NONE };
enum class T_BAUD_RATE {
    B9600,
    B19200,
    B38400,
    B57600,
    B115200,
    B230400,
    NONE
};
enum class TrackMode { SIDEREAL, SOLAR, LUNAR, CUSTOM, NONE };
enum class PierSide { EAST, WEST, NONE };
enum class ParkOptions { CURRENT, DEFAULT, WRITE_DATA, PURGE_DATA, NONE };
enum class SlewRate { GUIDE, CENTERING, FIND, MAX, NONE };
enum class MotionEW { WEST, EAST, NONE };
enum class MotionNS { NORTH, SOUTH, NONE };
enum class DomePolicy { IGNORED, LOCKED, NONE };

class AtomTelescope : public AtomDriver {
public:
    explicit AtomTelescope(std::string name) : AtomDriver(name) {}

    virtual auto getTelescopeInfo()
        -> std::optional<std::tuple<double, double, double, double>> = 0;
    virtual auto setTelescopeInfo(double aperture, double focalLength,
                                  double guiderAperture,
                                  double guiderFocalLength) -> bool = 0;
    virtual auto getPierSide() -> std::optional<PierSide> = 0;

    virtual auto getTrackRate() -> std::optional<TrackMode> = 0;
    virtual auto setTrackRate(TrackMode rate) -> bool = 0;

    virtual auto isTrackingEnabled() -> bool = 0;
    virtual auto enableTracking(bool enable) -> bool = 0;

    virtual auto abortMotion() -> bool = 0;
    virtual auto getStatus() -> std::optional<std::string> = 0;

    virtual auto setParkOption(ParkOptions option) -> bool = 0;
    virtual auto getParkPosition()
        -> std::optional<std::pair<double, double>> = 0;
    virtual auto setParkPosition(double ra, double dec) -> bool = 0;
    virtual auto isParked() -> bool = 0;
    virtual auto park(bool isParked) -> bool = 0;

    virtual auto initializeHome(std::string_view command) -> bool = 0;

    virtual auto getSlewRate() -> std::optional<double> = 0;
    virtual auto setSlewRate(double speed) -> bool = 0;
    virtual auto getTotalSlewRate() -> std::optional<double> = 0;

    virtual auto getMoveDirectionEW() -> std::optional<MotionEW> = 0;
    virtual auto setMoveDirectionEW(MotionEW direction) -> bool = 0;
    virtual auto getMoveDirectionNS() -> std::optional<MotionNS> = 0;
    virtual auto setMoveDirectionNS(MotionNS direction) -> bool = 0;

    virtual auto guideNS(int direction, int duration) -> bool = 0;
    virtual auto guideEW(int direction, int duration) -> bool = 0;

    virtual auto setActionAfterPositionSet(std::string_view action) -> bool = 0;

    virtual auto getRADECJ2000()
        -> std::optional<std::pair<double, double>> = 0;
    virtual auto setRADECJ2000(double raHours, double decDegrees) -> bool = 0;

    virtual auto getRADECJNow() -> std::optional<std::pair<double, double>> = 0;
    virtual auto setRADECJNow(double raHours, double decDegrees) -> bool = 0;

    virtual auto getTargetRADECJNow()
        -> std::optional<std::pair<double, double>> = 0;
    virtual auto setTargetRADECJNow(double raHours,
                                    double decDegrees) -> bool = 0;
    virtual auto slewToRADECJNow(double raHours, double decDegrees,
                                 bool enableTracking) -> bool = 0;

    virtual auto syncToRADECJNow(double raHours, double decDegrees) -> bool = 0;
    virtual auto getAZALT() -> std::optional<std::pair<double, double>> = 0;
    virtual auto setAZALT(double azDegrees, double altDegrees) -> bool = 0;
};
