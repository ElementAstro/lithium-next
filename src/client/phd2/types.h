// phd2_types.h
#pragma once

#include <optional>
#include <string>
#include <variant>
#include "atom/type/json.hpp"

namespace phd2 {

// Using nlohmann/json for JSON handling
using json = nlohmann::json;

// PHD2 events
enum class EventType {
    Version,
    LockPositionSet,
    Calibrating,
    CalibrationComplete,
    StarSelected,
    StartGuiding,
    Paused,
    StartCalibration,
    AppState,
    CalibrationFailed,
    CalibrationDataFlipped,
    LockPositionShiftLimitReached,
    LoopingExposures,
    LoopingExposuresStopped,
    SettleBegin,
    Settling,
    SettleDone,
    StarLost,
    GuidingStopped,
    Resumed,
    GuideStep,
    GuidingDithered,
    LockPositionLost,
    Alert,
    GuideParamChange,
    ConfigurationChange,
    Generic  // For any unhandled event types
};

// PHD2 application states
enum class AppStateType {
    Stopped,
    Selected,
    Calibrating,
    Guiding,
    LostLock,
    Paused,
    Looping,
    Unknown
};

// Map string state to enum
inline AppStateType appStateFromString(const std::string& state) {
    if (state == "Stopped")
        return AppStateType::Stopped;
    if (state == "Selected")
        return AppStateType::Selected;
    if (state == "Calibrating")
        return AppStateType::Calibrating;
    if (state == "Guiding")
        return AppStateType::Guiding;
    if (state == "LostLock")
        return AppStateType::LostLock;
    if (state == "Paused")
        return AppStateType::Paused;
    if (state == "Looping")
        return AppStateType::Looping;
    return AppStateType::Unknown;
}

// Map enum to string
inline std::string appStateToString(AppStateType state) {
    switch (state) {
        case AppStateType::Stopped:
            return "Stopped";
        case AppStateType::Selected:
            return "Selected";
        case AppStateType::Calibrating:
            return "Calibrating";
        case AppStateType::Guiding:
            return "Guiding";
        case AppStateType::LostLock:
            return "LostLock";
        case AppStateType::Paused:
            return "Paused";
        case AppStateType::Looping:
            return "Looping";
        default:
            return "Unknown";
    }
}

// Map string event to enum
inline EventType eventTypeFromString(const std::string& event) {
    if (event == "Version")
        return EventType::Version;
    if (event == "LockPositionSet")
        return EventType::LockPositionSet;
    if (event == "Calibrating")
        return EventType::Calibrating;
    if (event == "CalibrationComplete")
        return EventType::CalibrationComplete;
    if (event == "StarSelected")
        return EventType::StarSelected;
    if (event == "StartGuiding")
        return EventType::StartGuiding;
    if (event == "Paused")
        return EventType::Paused;
    if (event == "StartCalibration")
        return EventType::StartCalibration;
    if (event == "AppState")
        return EventType::AppState;
    if (event == "CalibrationFailed")
        return EventType::CalibrationFailed;
    if (event == "CalibrationDataFlipped")
        return EventType::CalibrationDataFlipped;
    if (event == "LockPositionShiftLimitReached")
        return EventType::LockPositionShiftLimitReached;
    if (event == "LoopingExposures")
        return EventType::LoopingExposures;
    if (event == "LoopingExposuresStopped")
        return EventType::LoopingExposuresStopped;
    if (event == "SettleBegin")
        return EventType::SettleBegin;
    if (event == "Settling")
        return EventType::Settling;
    if (event == "SettleDone")
        return EventType::SettleDone;
    if (event == "StarLost")
        return EventType::StarLost;
    if (event == "GuidingStopped")
        return EventType::GuidingStopped;
    if (event == "Resumed")
        return EventType::Resumed;
    if (event == "GuideStep")
        return EventType::GuideStep;
    if (event == "GuidingDithered")
        return EventType::GuidingDithered;
    if (event == "LockPositionLost")
        return EventType::LockPositionLost;
    if (event == "Alert")
        return EventType::Alert;
    if (event == "GuideParamChange")
        return EventType::GuideParamChange;
    if (event == "ConfigurationChange")
        return EventType::ConfigurationChange;
    return EventType::Generic;
}

// RPC response structure
struct RpcResponse {
    bool success;
    json result;
    int errorCode{0};
    std::string errorMessage;
};

// Settle parameters for guide and dither commands
struct SettleParams {
    double pixels;  // Maximum distance for "settled" state
    int time;       // Minimum time to remain "settled"
    int timeout;    // Maximum time before giving up

    json toJson() const {
        return json{{"pixels", pixels}, {"time", time}, {"timeout", timeout}};
    }
};

// Base class for all PHD2 events
struct EventBase {
    EventType type;
    double timestamp;
    std::string host;
    int instance;

    EventBase(EventType t, double ts, std::string h, int inst)
        : type(t), timestamp(ts), host(std::move(h)), instance(inst) {}

    virtual ~EventBase() = default;
};

// Generic event for unimplemented types
struct GenericEvent : EventBase {
    json data;

    GenericEvent(double ts, std::string h, int inst, json d)
        : EventBase(EventType::Generic, ts, std::move(h), inst),
          data(std::move(d)) {}
};

// Version event
struct VersionEvent : EventBase {
    std::string phdVersion;
    std::string phdSubver;
    int msgVersion;
    bool overlapSupport;

    VersionEvent(double ts, std::string h, int inst, std::string ver,
                 std::string subver, int msgVer, bool overlap)
        : EventBase(EventType::Version, ts, std::move(h), inst),
          phdVersion(std::move(ver)),
          phdSubver(std::move(subver)),
          msgVersion(msgVer),
          overlapSupport(overlap) {}
};

// LockPositionSet event
struct LockPositionSetEvent : EventBase {
    double x;
    double y;

    LockPositionSetEvent(double ts, std::string h, int inst, double xPos,
                         double yPos)
        : EventBase(EventType::LockPositionSet, ts, std::move(h), inst),
          x(xPos),
          y(yPos) {}
};

// Calibrating event
struct CalibratingEvent : EventBase {
    std::string mount;
    std::string dir;
    double dist;
    double dx;
    double dy;
    std::array<double, 2> pos;
    int step;
    std::string state;

    CalibratingEvent(double ts, std::string h, int inst, std::string m,
                     std::string d, double dist_, double dx_, double dy_,
                     std::array<double, 2> p, int s, std::string st)
        : EventBase(EventType::Calibrating, ts, std::move(h), inst),
          mount(std::move(m)),
          dir(std::move(d)),
          dist(dist_),
          dx(dx_),
          dy(dy_),
          pos(p),
          step(s),
          state(std::move(st)) {}
};

// CalibrationComplete event
struct CalibrationCompleteEvent : EventBase {
    std::string mount;

    CalibrationCompleteEvent(double ts, std::string h, int inst, std::string m)
        : EventBase(EventType::CalibrationComplete, ts, std::move(h), inst),
          mount(std::move(m)) {}
};

// StarSelected event
struct StarSelectedEvent : EventBase {
    double x;
    double y;

    StarSelectedEvent(double ts, std::string h, int inst, double xPos,
                      double yPos)
        : EventBase(EventType::StarSelected, ts, std::move(h), inst),
          x(xPos),
          y(yPos) {}
};

// AppState event
struct AppStateEvent : EventBase {
    AppStateType state;

    AppStateEvent(double ts, std::string h, int inst, AppStateType s)
        : EventBase(EventType::AppState, ts, std::move(h), inst), state(s) {}
};

// GuideStep event
struct GuideStepEvent : EventBase {
    int frame;
    double time;
    std::string mount;
    double dx;
    double dy;
    double raDistanceRaw;
    double decDistanceRaw;
    double raDistanceGuide;
    double decDistanceGuide;
    int raDuration;
    std::string raDirection;
    int decDuration;
    std::string decDirection;
    double starMass;
    double snr;
    double hfd;
    double avgDist;
    std::optional<bool> raLimited;
    std::optional<bool> decLimited;
    std::optional<int> errorCode;

    GuideStepEvent(double ts, std::string h, int inst, int f, double t,
                   std::string m, double dx_, double dy_, double raRaw,
                   double decRaw, double raGuide, double decGuide, int raDur,
                   std::string raDir, int decDur, std::string decDir,
                   double mass, double s, double hfd_, double avgDist_,
                   std::optional<bool> raLim = std::nullopt,
                   std::optional<bool> decLim = std::nullopt,
                   std::optional<int> errCode = std::nullopt)
        : EventBase(EventType::GuideStep, ts, std::move(h), inst),
          frame(f),
          time(t),
          mount(std::move(m)),
          dx(dx_),
          dy(dy_),
          raDistanceRaw(raRaw),
          decDistanceRaw(decRaw),
          raDistanceGuide(raGuide),
          decDistanceGuide(decGuide),
          raDuration(raDur),
          raDirection(std::move(raDir)),
          decDuration(decDur),
          decDirection(std::move(decDir)),
          starMass(mass),
          snr(s),
          hfd(hfd_),
          avgDist(avgDist_),
          raLimited(raLim),
          decLimited(decLim),
          errorCode(errCode) {}
};

// SettleBegin event
struct SettleBeginEvent : EventBase {
    SettleBeginEvent(double ts, std::string h, int inst)
        : EventBase(EventType::SettleBegin, ts, std::move(h), inst) {}
};

// Settling event
struct SettlingEvent : EventBase {
    double distance;
    double time;
    double settleTime;
    bool starLocked;

    SettlingEvent(double ts, std::string h, int inst, double dist, double t,
                  double sTime, bool locked)
        : EventBase(EventType::Settling, ts, std::move(h), inst),
          distance(dist),
          time(t),
          settleTime(sTime),
          starLocked(locked) {}
};

// SettleDone event
struct SettleDoneEvent : EventBase {
    int status;
    std::string error;
    int totalFrames;
    int droppedFrames;

    SettleDoneEvent(double ts, std::string h, int inst, int s, std::string e,
                    int tf, int df)
        : EventBase(EventType::SettleDone, ts, std::move(h), inst),
          status(s),
          error(std::move(e)),
          totalFrames(tf),
          droppedFrames(df) {}
};

// StarLost event
struct StarLostEvent : EventBase {
    int frame;
    double time;
    double starMass;
    double snr;
    double avgDist;
    int errorCode;
    std::string status;

    StarLostEvent(double ts, std::string h, int inst, int f, double t,
                  double mass, double s, double avgDist_, int errCode,
                  std::string status_)
        : EventBase(EventType::StarLost, ts, std::move(h), inst),
          frame(f),
          time(t),
          starMass(mass),
          snr(s),
          avgDist(avgDist_),
          errorCode(errCode),
          status(std::move(status_)) {}
};

// GuidingDithered event
struct GuidingDitheredEvent : EventBase {
    double dx;
    double dy;

    GuidingDitheredEvent(double ts, std::string h, int inst, double dx_,
                         double dy_)
        : EventBase(EventType::GuidingDithered, ts, std::move(h), inst),
          dx(dx_),
          dy(dy_) {}
};

// Alert event
struct AlertEvent : EventBase {
    std::string message;
    std::string type;

    AlertEvent(double ts, std::string h, int inst, std::string msg,
               std::string t)
        : EventBase(EventType::Alert, ts, std::move(h), inst),
          message(std::move(msg)),
          type(std::move(t)) {}
};

// Complete the remaining event types as needed
// ...

// Event variant type for polymorphic event handling
using Event =
    std::variant<VersionEvent, LockPositionSetEvent, CalibratingEvent,
                 CalibrationCompleteEvent, StarSelectedEvent, AppStateEvent,
                 GuideStepEvent, SettleBeginEvent, SettlingEvent,
                 SettleDoneEvent, StarLostEvent, GuidingDitheredEvent,
                 AlertEvent,
                 GenericEvent  // For handling any unimplemented event types
                 >;

// Helper to get the event type from a variant
inline EventType getEventType(const Event& event) {
    return std::visit(
        [](const auto& e) -> EventType {
            return static_cast<const EventBase&>(e)
                .type;  // Explicitly access base class type
        },
        event);
}

}  // namespace phd2
