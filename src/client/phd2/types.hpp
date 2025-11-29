/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: PHD2 types and event definitions with modern C++ features

*************************************************/

#pragma once

#include <array>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "atom/type/json.hpp"

namespace phd2 {

using json = nlohmann::json;

// ============================================================================
// Enums with string conversion using constexpr
// ============================================================================

/**
 * @brief PHD2 event types
 */
enum class EventType : uint8_t {
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
    Generic
};

/**
 * @brief PHD2 application states
 */
enum class AppStateType : uint8_t {
    Stopped,
    Selected,
    Calibrating,
    Guiding,
    LostLock,
    Paused,
    Looping,
    Unknown
};

// ============================================================================
// Compile-time string conversion maps
// ============================================================================

namespace detail {

inline constexpr std::array<std::pair<std::string_view, AppStateType>, 7>
    kAppStateMap = {{
        {"Stopped", AppStateType::Stopped},
        {"Selected", AppStateType::Selected},
        {"Calibrating", AppStateType::Calibrating},
        {"Guiding", AppStateType::Guiding},
        {"LostLock", AppStateType::LostLock},
        {"Paused", AppStateType::Paused},
        {"Looping", AppStateType::Looping},
    }};

inline constexpr std::array<std::pair<std::string_view, EventType>, 26>
    kEventTypeMap = {{
        {"Version", EventType::Version},
        {"LockPositionSet", EventType::LockPositionSet},
        {"Calibrating", EventType::Calibrating},
        {"CalibrationComplete", EventType::CalibrationComplete},
        {"StarSelected", EventType::StarSelected},
        {"StartGuiding", EventType::StartGuiding},
        {"Paused", EventType::Paused},
        {"StartCalibration", EventType::StartCalibration},
        {"AppState", EventType::AppState},
        {"CalibrationFailed", EventType::CalibrationFailed},
        {"CalibrationDataFlipped", EventType::CalibrationDataFlipped},
        {"LockPositionShiftLimitReached",
         EventType::LockPositionShiftLimitReached},
        {"LoopingExposures", EventType::LoopingExposures},
        {"LoopingExposuresStopped", EventType::LoopingExposuresStopped},
        {"SettleBegin", EventType::SettleBegin},
        {"Settling", EventType::Settling},
        {"SettleDone", EventType::SettleDone},
        {"StarLost", EventType::StarLost},
        {"GuidingStopped", EventType::GuidingStopped},
        {"Resumed", EventType::Resumed},
        {"GuideStep", EventType::GuideStep},
        {"GuidingDithered", EventType::GuidingDithered},
        {"LockPositionLost", EventType::LockPositionLost},
        {"Alert", EventType::Alert},
        {"GuideParamChange", EventType::GuideParamChange},
        {"ConfigurationChange", EventType::ConfigurationChange},
    }};

}  // namespace detail

/**
 * @brief Convert string to AppStateType
 */
[[nodiscard]] constexpr auto appStateFromString(std::string_view state) noexcept
    -> AppStateType {
    for (const auto& [str, type] : detail::kAppStateMap) {
        if (str == state)
            return type;
    }
    return AppStateType::Unknown;
}

/**
 * @brief Convert AppStateType to string
 */
[[nodiscard]] constexpr auto appStateToString(AppStateType state) noexcept
    -> std::string_view {
    for (const auto& [str, type] : detail::kAppStateMap) {
        if (type == state)
            return str;
    }
    return "Unknown";
}

/**
 * @brief Convert string to EventType
 */
[[nodiscard]] constexpr auto eventTypeFromString(
    std::string_view event) noexcept -> EventType {
    for (const auto& [str, type] : detail::kEventTypeMap) {
        if (str == event)
            return type;
    }
    return EventType::Generic;
}

// ============================================================================
// RPC Response
// ============================================================================

/**
 * @brief RPC response structure with structured bindings support
 */
struct RpcResponse {
    bool success{false};
    json result{};
    int errorCode{0};
    std::string errorMessage{};

    [[nodiscard]] explicit operator bool() const noexcept { return success; }

    [[nodiscard]] auto hasError() const noexcept -> bool {
        return !success || errorCode != 0;
    }
};

// ============================================================================
// Settle Parameters
// ============================================================================

/**
 * @brief Settle parameters for guide and dither commands
 */
struct SettleParams {
    double pixels{1.5};    // Maximum distance for "settled" state
    double time{10.0};     // Minimum time to remain "settled"
    double timeout{60.0};  // Maximum time before giving up

    [[nodiscard]] auto toJson() const -> json {
        return json{{"pixels", pixels}, {"time", time}, {"timeout", timeout}};
    }

    static auto fromJson(const json& j) -> SettleParams {
        return SettleParams{.pixels = j.value("pixels", 1.5),
                            .time = j.value("time", 10.0),
                            .timeout = j.value("timeout", 60.0)};
    }
};

// ============================================================================
// Event Structures using aggregate initialization
// ============================================================================

/**
 * @brief Base event data common to all events
 */
struct EventBase {
    EventType type{EventType::Generic};
    double timestamp{0.0};
    std::string host{};
    int instance{0};
};

/**
 * @brief Generic event for unimplemented types
 */
struct GenericEvent : EventBase {
    json data{};
};

/**
 * @brief Version event
 */
struct VersionEvent : EventBase {
    std::string phdVersion{};
    std::string phdSubver{};
    int msgVersion{0};
    bool overlapSupport{false};
};

/**
 * @brief Lock position set event
 */
struct LockPositionSetEvent : EventBase {
    double x{0.0};
    double y{0.0};
};

/**
 * @brief Calibrating event
 */
struct CalibratingEvent : EventBase {
    std::string mount{};
    std::string dir{};
    double dist{0.0};
    double dx{0.0};
    double dy{0.0};
    std::array<double, 2> pos{};
    int step{0};
    std::string state{};
};

/**
 * @brief Calibration complete event
 */
struct CalibrationCompleteEvent : EventBase {
    std::string mount{};
};

/**
 * @brief Star selected event
 */
struct StarSelectedEvent : EventBase {
    double x{0.0};
    double y{0.0};
};

/**
 * @brief App state change event
 */
struct AppStateEvent : EventBase {
    std::string state{};
};

/**
 * @brief Start guiding event
 */
struct StartGuidingEvent : EventBase {};

/**
 * @brief Guiding stopped event
 */
struct GuidingStoppedEvent : EventBase {};

/**
 * @brief Paused event
 */
struct PausedEvent : EventBase {};

/**
 * @brief Resumed event
 */
struct ResumedEvent : EventBase {};

/**
 * @brief Guide step event with full telemetry
 */
struct GuideStepEvent : EventBase {
    int frame{0};
    double time{0.0};
    std::string mount{};
    double dx{0.0};
    double dy{0.0};
    double raDistanceRaw{0.0};
    double decDistanceRaw{0.0};
    double raDistanceGuide{0.0};
    double decDistanceGuide{0.0};
    int raDuration{0};
    std::string raDirection{};
    int decDuration{0};
    std::string decDirection{};
    double starMass{0.0};
    double snr{0.0};
    double hfd{0.0};
    double avgDist{0.0};
    std::optional<bool> raLimited{};
    std::optional<bool> decLimited{};
    std::optional<int> errorCode{};
};

/**
 * @brief Settle begin event
 */
struct SettleBeginEvent : EventBase {};

/**
 * @brief Settling progress event
 */
struct SettlingEvent : EventBase {
    double distance{0.0};
    double time{0.0};
    double settleTime{0.0};
    bool starLocked{false};
};

/**
 * @brief Settle done event
 */
struct SettleDoneEvent : EventBase {
    int status{0};
    std::string error{};
    int totalFrames{0};
    int droppedFrames{0};

    [[nodiscard]] auto isSuccess() const noexcept -> bool {
        return status == 0;
    }
};

/**
 * @brief Star lost event
 */
struct StarLostEvent : EventBase {
    int frame{0};
    double time{0.0};
    double starMass{0.0};
    double snr{0.0};
    double avgDist{0.0};
    int errorCode{0};
    std::string status{};
};

/**
 * @brief Guiding dithered event
 */
struct GuidingDitheredEvent : EventBase {
    double dx{0.0};
    double dy{0.0};
};

/**
 * @brief Alert event
 */
struct AlertEvent : EventBase {
    std::string message{};
    std::string alertType{};
};

/**
 * @brief Calibration failed event
 */
struct CalibrationFailedEvent : EventBase {
    std::string reason{};
};

// ============================================================================
// Event Variant
// ============================================================================

/**
 * @brief Event variant type for polymorphic event handling
 */
using Event =
    std::variant<VersionEvent, LockPositionSetEvent, CalibratingEvent,
                 CalibrationCompleteEvent, CalibrationFailedEvent,
                 StarSelectedEvent, AppStateEvent, StartGuidingEvent,
                 GuidingStoppedEvent, PausedEvent, ResumedEvent, GuideStepEvent,
                 SettleBeginEvent, SettlingEvent, SettleDoneEvent,
                 StarLostEvent, GuidingDitheredEvent, AlertEvent, GenericEvent>;

/**
 * @brief Get the event type from a variant
 */
[[nodiscard]] inline auto getEventType(const Event& event) noexcept
    -> EventType {
    return std::visit([](const auto& e) { return e.type; }, event);
}

/**
 * @brief Get the timestamp from a variant
 */
[[nodiscard]] inline auto getEventTimestamp(const Event& event) noexcept
    -> double {
    return std::visit([](const auto& e) { return e.timestamp; }, event);
}

// ============================================================================
// Concepts for type constraints
// ============================================================================

/**
 * @brief Concept for PHD2 event types
 */
template <typename T>
concept Phd2Event = std::derived_from<T, EventBase> || requires(T t) {
    { t.type } -> std::convertible_to<EventType>;
    { t.timestamp } -> std::convertible_to<double>;
};

}  // namespace phd2
