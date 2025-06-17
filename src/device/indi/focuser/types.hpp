#ifndef LITHIUM_INDI_FOCUSER_TYPES_HPP
#define LITHIUM_INDI_FOCUSER_TYPES_HPP

#include <libindi/basedevice.h>
#include <spdlog/spdlog.h>
#include <array>
#include <atomic>
#include <optional>
#include <string>

#include "device/template/focuser.hpp"

namespace lithium::device::indi::focuser {

/**
 * @brief Shared state structure for INDI Focuser components.
 *
 * This structure holds all relevant state information for an INDI-based focuser
 * device, including connection status, device information, focus parameters,
 * temperature, statistics, and references to the underlying INDI device and
 * logger.
 *
 * All members are designed to be thread-safe where necessary, using atomic
 * types for values that may be updated from multiple threads.
 */
struct FocuserState {
    /**
     * @brief Indicates if the focuser device is currently connected.
     */
    std::atomic_bool isConnected_{false};

    /**
     * @brief Indicates if debug mode is enabled for the focuser.
     */
    std::atomic_bool isDebug_{false};

    /**
     * @brief Indicates if the focuser is currently moving.
     */
    std::atomic_bool isFocuserMoving_{false};

    /**
     * @brief Name of the focuser device.
     */
    std::string deviceName_;

    /**
     * @brief Path to the focuser driver executable.
     */
    std::string driverExec_;

    /**
     * @brief Version string of the focuser driver.
     */
    std::string driverVersion_;

    /**
     * @brief Interface type of the focuser driver.
     */
    std::string driverInterface_;

    /**
     * @brief Current polling period in milliseconds.
     */
    std::atomic<double> currentPollingPeriod_{1000.0};

    /**
     * @brief Whether the device auto-search is enabled.
     */
    bool deviceAutoSearch_{false};

    /**
     * @brief Whether the device port scan is enabled.
     */
    bool devicePortScan_{false};

    /**
     * @brief Serial port name for the focuser device.
     */
    std::string devicePort_;

    /**
     * @brief Baud rate for serial communication.
     */
    BAUD_RATE baudRate_{BAUD_RATE::B9600};

    /**
     * @brief Current focus mode (e.g., ALL, RELATIVE, ABSOLUTE).
     */
    FocusMode focusMode_{FocusMode::ALL};

    /**
     * @brief Current focus direction (IN or OUT).
     */
    FocusDirection focusDirection_{FocusDirection::IN};

    /**
     * @brief Current focus speed (percentage or device-specific units).
     */
    std::atomic<double> currentFocusSpeed_{50.0};

    /**
     * @brief Indicates if the focuser direction is reversed.
     */
    std::atomic_bool isReverse_{false};

    /**
     * @brief Timer value for focus operations (milliseconds).
     */
    std::atomic<double> focusTimer_{0.0};

    /**
     * @brief Last known relative position of the focuser.
     */
    std::atomic_int realRelativePosition_{0};

    /**
     * @brief Last known absolute position of the focuser.
     */
    std::atomic_int realAbsolutePosition_{0};

    /**
     * @brief Current position of the focuser.
     */
    std::atomic_int currentPosition_{0};

    /**
     * @brief Target position for the focuser to move to.
     */
    std::atomic_int targetPosition_{0};

    /**
     * @brief Maximum allowed focuser position.
     */
    int maxPosition_{65535};

    /**
     * @brief Minimum allowed focuser position.
     */
    int minPosition_{0};

    /**
     * @brief Indicates if backlash compensation is enabled.
     */
    std::atomic_bool backlashEnabled_{false};

    /**
     * @brief Number of steps for backlash compensation.
     */
    std::atomic_int backlashSteps_{0};

    /**
     * @brief Current temperature reported by the focuser (Celsius).
     */
    std::atomic<double> temperature_{20.0};

    /**
     * @brief Chip temperature, if available (Celsius).
     */
    std::atomic<double> chipTemperature_{20.0};

    /**
     * @brief Delay in milliseconds for certain operations.
     */
    int delay_msec_{0};

    /**
     * @brief Indicates if auto-focus is currently running.
     */
    std::atomic_bool isAutoFocusing_{false};

    /**
     * @brief Progress of the current auto-focus operation (0.0 - 100.0).
     */
    std::atomic<double> autoFocusProgress_{0.0};

    /**
     * @brief Total number of steps moved by the focuser.
     */
    std::atomic<uint64_t> totalSteps_{0};

    /**
     * @brief Number of steps moved in the last move operation.
     */
    std::atomic_int lastMoveSteps_{0};

    /**
     * @brief Duration of the last move operation (milliseconds).
     */
    std::atomic_int lastMoveDuration_{0};

    /**
     * @brief Preset positions for the focuser (up to 10).
     */
    std::array<std::optional<int>, 10> presets_;

    /**
     * @brief Temperature compensation settings.
     */
    TemperatureCompensation tempCompensation_;

    /**
     * @brief Indicates if temperature compensation is enabled.
     */
    std::atomic_bool tempCompensationEnabled_{false};

    /**
     * @brief Reference to the underlying INDI device.
     */
    INDI::BaseDevice device_;

    /**
     * @brief Shared pointer to the logger instance for this focuser.
     */
    std::shared_ptr<spdlog::logger> logger_;
};

/**
 * @brief Base interface for focuser components.
 *
 * All focuser components should inherit from this interface to ensure
 * consistent initialization, cleanup, and logging.
 */
class IFocuserComponent {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~IFocuserComponent() = default;

    /**
     * @brief Initialize the component.
     * @param state Shared focuser state.
     * @return true if initialization was successful, false otherwise.
     */
    virtual bool initialize(FocuserState& state) = 0;

    /**
     * @brief Cleanup the component and release any resources.
     */
    virtual void cleanup() = 0;

    /**
     * @brief Get the component's name for logging and identification.
     * @return Name of the component.
     */
    virtual std::string getComponentName() const = 0;
};

}  // namespace lithium::device::indi::focuser

#endif  // LITHIUM_INDI_FOCUSER_TYPES_HPP
