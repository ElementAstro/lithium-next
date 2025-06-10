// client.h
#pragma once

#include "connection.h"
#include "event_handler.h"
#include "types.h"

#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace phd2 {

/**
 * @brief Main client class for interacting with PHD2
 */
class Client {
public:
    /**
     * @brief Construct a new Client object
     *
     * @param host The hostname where PHD2 is running
     * @param port The port number (default: 4400)
     * @param eventHandler Optional event handler to receive PHD2 events
     */
    explicit Client(std::string host = "localhost", int port = 4400,
                    std::shared_ptr<EventHandler> eventHandler = nullptr);

    /**
     * @brief Destroy the Client object and disconnect if connected
     */
    ~Client();

    /**
     * @brief Connect to PHD2
     *
     * @param timeoutMs Connection timeout in milliseconds
     * @return true if connection was successful
     */
    bool connect(int timeoutMs = 5000);

    /**
     * @brief Disconnect from PHD2
     */
    void disconnect();

    /**
     * @brief Check if currently connected to PHD2
     *
     * @return true if connected to PHD2
     */
    bool isConnected() const;

    /**
     * @brief Set the event handler for receiving PHD2 events
     *
     * @param handler The event handler
     */
    void setEventHandler(std::shared_ptr<EventHandler> handler);

    // ========== Camera Control Methods ==========

    /**
     * @brief Get the current camera exposure time
     *
     * @return int Exposure time in milliseconds
     */
    int getExposure();

    /**
     * @brief Set the camera exposure time
     *
     * @param exposureMs Exposure time in milliseconds
     */
    void setExposure(int exposureMs);

    /**
     * @brief Get available exposure durations
     *
     * @return std::vector<int> List of available exposure durations in
     * milliseconds
     */
    std::vector<int> getExposureDurations();

    /**
     * @brief Get subframe status
     *
     * @return bool True if subframes are being used
     */
    bool getUseSubframes();

    /**
     * @brief Capture a single frame
     *
     * @param exposureMs Optional exposure time in milliseconds
     * @param subframe Optional subframe region [x, y, width, height]
     */
    void captureSingleFrame(
        std::optional<int> exposureMs = std::nullopt,
        std::optional<std::array<int, 4>> subframe = std::nullopt);

    /**
     * @brief Get the camera frame size
     *
     * @return std::array<int, 2> Width and height in pixels
     */
    std::array<int, 2> getCameraFrameSize();

    /**
     * @brief Get the camera temperature
     *
     * @return double Temperature in degrees C
     */
    double getCcdTemperature();

    /**
     * @brief Get cooler status
     *
     * @return json Object with cooler status information
     */
    json getCoolerStatus();

    /**
     * @brief Save the current camera image to a file
     *
     * @return std::string Path to the saved FITS file
     */
    std::string saveImage();

    /**
     * @brief Get a star image
     *
     * @param size Optional size of the image in pixels (must be >= 15)
     * @return json Object with star image data
     */
    json getStarImage(std::optional<int> size = std::nullopt);

    // ========== Equipment Control Methods ==========

    /**
     * @brief Check if all equipment is connected
     *
     * @return bool True if all equipment is connected
     */
    bool getConnected();

    /**
     * @brief Connect or disconnect all equipment
     *
     * @param connect True to connect, false to disconnect
     */
    void setConnected(bool connect);

    /**
     * @brief Get current equipment information
     *
     * @return json Object with equipment information
     */
    json getCurrentEquipment();

    /**
     * @brief Get available equipment profiles
     *
     * @return json Array of profile objects
     */
    json getProfiles();

    /**
     * @brief Get current equipment profile
     *
     * @return json Object with profile information
     */
    json getProfile();

    /**
     * @brief Set equipment profile
     *
     * @param profileId Profile ID to set
     */
    void setProfile(int profileId);

    // ========== Guiding Control Methods ==========

    /**
     * @brief Start guiding
     *
     * @param settle Settlement parameters
     * @param recalibrate Whether to force recalibration
     * @param roi Optional region of interest for star selection
     * @return std::future<bool> A future that completes when settling is done
     */
    std::future<bool> startGuiding(
        const SettleParams& settle, bool recalibrate = false,
        const std::optional<std::array<int, 4>>& roi = std::nullopt);

    /**
     * @brief Stop guiding and capturing
     */
    void stopCapture();

    /**
     * @brief Start looping exposures
     */
    void loop();

    /**
     * @brief Perform a dither operation
     *
     * @param amount Dither amount in pixels
     * @param raOnly Whether to dither only in RA
     * @param settle Settlement parameters
     * @return std::future<bool> A future that completes when settling is done
     */
    std::future<bool> dither(double amount, bool raOnly,
                             const SettleParams& settle);

    /**
     * @brief Get the current PHD2 application state
     *
     * @return AppStateType The current state
     */
    AppStateType getAppState();

    /**
     * @brief Send a direct guide pulse command
     *
     * @param amount Pulse duration in milliseconds, or AO step count
     * @param direction Direction ("N"/"S"/"E"/"W"/"Up"/"Down"/"Left"/"Right")
     * @param which "Mount" or "AO" (default: "Mount")
     */
    void guidePulse(int amount, const std::string& direction,
                    const std::string& which = "Mount");

    /**
     * @brief Check if PHD2 is paused
     *
     * @return bool True if paused
     */
    bool getPaused();

    /**
     * @brief Pause or unpause PHD2
     *
     * @param pause True to pause, false to unpause
     * @param full If true and pausing, also pause looping exposures
     */
    void setPaused(bool pause, bool full = false);

    /**
     * @brief Check if guide output is enabled
     *
     * @return bool True if guide output is enabled
     */
    bool getGuideOutputEnabled();

    /**
     * @brief Enable or disable guide output
     *
     * @param enable True to enable, false to disable
     */
    void setGuideOutputEnabled(bool enable);

    /**
     * @brief Get variable delay settings
     *
     * @return json Variable delay settings
     */
    json getVariableDelaySettings();

    /**
     * @brief Set variable delay settings
     *
     * @param settings Variable delay settings object
     */
    void setVariableDelaySettings(const json& settings);

    // ========== Calibration Methods ==========

    /**
     * @brief Check if PHD2 is currently calibrated
     *
     * @return bool true if calibrated
     */
    bool isCalibrated();

    /**
     * @brief Clear the current calibration
     *
     * @param which "mount", "ao", or "both" (default: "both")
     */
    void clearCalibration(const std::string& which = "both");

    /**
     * @brief Flip the calibration data
     */
    void flipCalibration();

    /**
     * @brief Get detailed calibration data
     *
     * @param which "Mount" or "AO" (default: "Mount")
     * @return json Calibration data
     */
    json getCalibrationData(const std::string& which = "Mount");

    // ========== Guide Algorithm Methods ==========

    /**
     * @brief Set Dec guide mode
     *
     * @param mode "Off", "Auto", "North", or "South"
     */
    void setDecGuideMode(const std::string& mode);

    /**
     * @brief Get Dec guide mode
     *
     * @return std::string Current Dec guide mode
     */
    std::string getDecGuideMode();

    /**
     * @brief Set guide algorithm parameter
     *
     * @param axis "ra", "dec", "x", or "y"
     * @param name Parameter name
     * @param value Parameter value
     */
    void setAlgoParam(const std::string& axis, const std::string& name,
                      double value);

    /**
     * @brief Get guide algorithm parameter
     *
     * @param axis "ra", "dec", "x", or "y"
     * @param name Parameter name
     * @return double Parameter value
     */
    double getAlgoParam(const std::string& axis, const std::string& name);

    /**
     * @brief Get all available algorithm parameter names for an axis
     *
     * @param axis "ra", "dec", "x", or "y"
     * @return std::vector<std::string> Parameter names
     */
    std::vector<std::string> getAlgoParamNames(const std::string& axis);

    // ========== Star Selection Methods ==========

    /**
     * @brief Find a star automatically
     *
     * @param roi Optional region of interest [x, y, width, height]
     * @return std::array<double, 2> Star position [x, y]
     */
    std::array<double, 2> findStar(
        const std::optional<std::array<int, 4>>& roi = std::nullopt);

    /**
     * @brief Get lock position if set
     *
     * @return std::optional<std::array<double, 2>> Lock position [x, y] or
     * nullopt if not set
     */
    std::optional<std::array<double, 2>> getLockPosition();

    /**
     * @brief Set the lock position
     *
     * @param x X coordinate
     * @param y Y coordinate
     * @param exact Whether to use exact position or find nearest star (default:
     * true)
     */
    void setLockPosition(double x, double y, bool exact = true);

    /**
     * @brief Get search region size
     *
     * @return int Search region radius
     */
    int getSearchRegion();

    /**
     * @brief Get pixel scale
     *
     * @return double Pixel scale in arc-seconds per pixel
     */
    double getPixelScale();

    // ========== Lock Shift Methods ==========

    /**
     * @brief Check if lock shift is enabled
     *
     * @return bool True if lock shift is enabled
     */
    bool getLockShiftEnabled();

    /**
     * @brief Enable or disable lock shift
     *
     * @param enable True to enable, false to disable
     */
    void setLockShiftEnabled(bool enable);

    /**
     * @brief Get lock shift parameters
     *
     * @return json Lock shift parameters
     */
    json getLockShiftParams();

    /**
     * @brief Set lock shift parameters
     *
     * @param params Lock shift parameters
     */
    void setLockShiftParams(const json& params);

    // ========== Advanced Methods ==========

    /**
     * @brief Shutdown PHD2
     */
    void shutdown();

private:
    std::unique_ptr<Connection> connection_;
    std::shared_ptr<EventHandler> eventHandler_;
    std::string host_;
    int port_;

    // Promise management for async operations
    std::mutex promiseMutex_;
    std::promise<bool> settlePromise_;
    bool settleInProgress_{false};

    // Internal event handler that manages promises for async operations
    class InternalEventHandler;
    std::shared_ptr<InternalEventHandler> internalHandler_;

    void handleSettleDone(bool success);
};

}  // namespace phd2