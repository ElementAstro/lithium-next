/*
 * dome.hpp
 *
 * AtomDome Simulator and Basic Definition
 */

#pragma once

#include <optional>
#include <string_view>
#include "device.hpp"

enum class ShutterStatus { OPEN, CLOSED, OPENING, CLOSING, ERROR, NONE };
enum class DomeState { IDLE, MOVING, PARKED, PARKING, UNPARKING, ERROR, NONE };

class AtomDome : public AtomDriver {
public:
    explicit AtomDome(std::string name) : AtomDriver(name) {}

    virtual auto getAzimuth() -> std::optional<double> = 0;
    virtual auto getAltitude()
        -> std::optional<double> = 0;  // Some domes have altitude control
    virtual auto setAzimuth(double azimuth) -> bool = 0;
    virtual auto setAltitude(double altitude) -> bool = 0;

    virtual auto getShutterStatus() -> std::optional<ShutterStatus> = 0;
    virtual auto openShutter() -> bool = 0;
    virtual auto closeShutter() -> bool = 0;

    virtual auto isParked() -> bool = 0;
    virtual auto park() -> bool = 0;
    virtual auto unpark() -> bool = 0;
    virtual auto findHome() -> bool = 0;

    virtual auto getDomeState() -> std::optional<DomeState> = 0;

    virtual auto canSlave() -> bool = 0;
    virtual auto setSlaved(bool slaved) -> bool = 0;
    virtual auto isSlaved() -> bool = 0;
};
