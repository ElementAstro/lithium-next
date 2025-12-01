/*
 * phd2_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: PHD2 guider client implementation

*************************************************/

#ifndef LITHIUM_CLIENT_PHD2_CLIENT_HPP
#define LITHIUM_CLIENT_PHD2_CLIENT_HPP

#include "../common/guider_client.hpp"
#include "connection.hpp"
#include "event_handler.hpp"
#include "types.hpp"

#include <memory>
#include <mutex>

namespace lithium::client {

/**
 * @brief PHD2-specific configuration
 */
struct PHD2Config {
    std::string host{"localhost"};
    int port{4400};
    int reconnectAttempts{3};
    int reconnectDelayMs{1000};
};

/**
 * @brief PHD2 guider client
 *
 * Provides guiding control through PHD2's JSON-RPC interface
 */
class PHD2Client : public GuiderClient, public phd2::EventHandler {
public:
    /**
     * @brief Construct a new PHD2Client
     * @param name Instance name
     */
    explicit PHD2Client(std::string name = "phd2");

    /**
     * @brief Destructor
     */
    ~PHD2Client() override;

    // ==================== Lifecycle ====================

    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& target, int timeout = 5000,
                 int maxRetry = 3) override;
    bool disconnect() override;
    bool isConnected() const override;
    std::vector<std::string> scan() override;

    // ==================== Guiding Control ====================

    std::future<bool> startGuiding(const SettleParams& settle,
                                   bool recalibrate = false) override;
    void stopGuiding() override;
    void pause(bool full = false) override;
    void resume() override;
    std::future<bool> dither(const DitherParams& params) override;
    void loop() override;

    // ==================== Calibration ====================

    bool isCalibrated() const override;
    void clearCalibration() override;
    void flipCalibration() override;
    CalibrationData getCalibrationData() const override;

    // ==================== Star Selection ====================

    GuideStar findStar(
        const std::optional<std::array<int, 4>>& roi = std::nullopt) override;
    void setLockPosition(double x, double y, bool exact = true) override;
    std::optional<std::array<double, 2>> getLockPosition() const override;

    // ==================== Camera Control ====================

    int getExposure() const override;
    void setExposure(int exposureMs) override;
    std::vector<int> getExposureDurations() const override;

    // ==================== Status ====================

    GuiderState getGuiderState() const override;
    GuideStats getGuideStats() const override;
    GuideStar getCurrentStar() const override;
    double getPixelScale() const override;

    // ==================== PHD2-Specific ====================

    /**
     * @brief Configure PHD2 connection
     * @param config PHD2 configuration
     */
    void configurePHD2(const PHD2Config& config);

    /**
     * @brief Get PHD2 configuration
     * @return Current configuration
     */
    const PHD2Config& getPHD2Config() const { return phd2Config_; }

    /**
     * @brief Get PHD2 application state
     * @return Application state string
     */
    std::string getAppState() const;

    /**
     * @brief Get current equipment profile
     * @return Profile information as JSON
     */
    phd2::json getProfile() const;

    /**
     * @brief Set equipment profile
     * @param profileId Profile ID
     */
    void setProfile(int profileId);

    /**
     * @brief Get available profiles
     * @return Array of profile information
     */
    phd2::json getProfiles() const;

    /**
     * @brief Send guide pulse
     * @param amount Pulse duration in ms
     * @param direction Direction (N/S/E/W)
     * @param which "Mount" or "AO"
     */
    void guidePulse(int amount, const std::string& direction,
                    const std::string& which = "Mount");

    /**
     * @brief Get Dec guide mode
     * @return Guide mode string
     */
    std::string getDecGuideMode() const;

    /**
     * @brief Set Dec guide mode
     * @param mode "Off", "Auto", "North", or "South"
     */
    void setDecGuideMode(const std::string& mode);

    /**
     * @brief Save current image
     * @return Path to saved file
     */
    std::string saveImage();

    /**
     * @brief Get camera frame size
     * @return [width, height]
     */
    std::array<int, 2> getCameraFrameSize() const;

    /**
     * @brief Get CCD temperature
     * @return Temperature in Celsius
     */
    double getCcdTemperature() const;

    /**
     * @brief Get cooler status
     * @return Cooler status JSON
     */
    phd2::json getCoolerStatus() const;

    /**
     * @brief Get star image data
     * @param size Image size (>= 15)
     * @return Star image data
     */
    phd2::json getStarImage(int size = 15) const;

    /**
     * @brief Get current equipment info
     * @return Equipment information
     */
    phd2::json getCurrentEquipment() const;

    /**
     * @brief Check if equipment is connected
     * @return true if connected
     */
    bool getConnected() const;

    /**
     * @brief Connect/disconnect equipment
     * @param connect true to connect
     */
    void setConnected(bool connect);

    /**
     * @brief Get guide algorithm parameter names
     * @param axis "ra", "dec", "x", or "y"
     * @return Parameter names
     */
    std::vector<std::string> getAlgoParamNames(const std::string& axis) const;

    /**
     * @brief Get guide algorithm parameter
     * @param axis Axis name
     * @param name Parameter name
     * @return Parameter value
     */
    double getAlgoParam(const std::string& axis, const std::string& name) const;

    /**
     * @brief Set guide algorithm parameter
     * @param axis Axis name
     * @param name Parameter name
     * @param value Parameter value
     */
    void setAlgoParam(const std::string& axis, const std::string& name,
                      double value);

    /**
     * @brief Check if guide output is enabled
     * @return true if enabled
     */
    bool getGuideOutputEnabled() const;

    /**
     * @brief Enable/disable guide output
     * @param enable true to enable
     */
    void setGuideOutputEnabled(bool enable);

    /**
     * @brief Get lock shift enabled status
     * @return true if enabled
     */
    bool getLockShiftEnabled() const;

    /**
     * @brief Enable/disable lock shift
     * @param enable true to enable
     */
    void setLockShiftEnabled(bool enable);

    /**
     * @brief Get lock shift parameters
     * @return Lock shift params
     */
    phd2::json getLockShiftParams() const;

    /**
     * @brief Set lock shift parameters
     * @param params Lock shift params
     */
    void setLockShiftParams(const phd2::json& params);

    /**
     * @brief Get variable delay settings
     * @return Variable delay settings
     */
    phd2::json getVariableDelaySettings() const;

    /**
     * @brief Set variable delay settings
     * @param settings Variable delay settings
     */
    void setVariableDelaySettings(const phd2::json& settings);

    /**
     * @brief Check if settling
     * @return true if settling
     */
    bool getSettling() const;

    /**
     * @brief Capture single frame
     * @param exposureMs Exposure time
     * @param subframe Optional subframe
     */
    void captureSingleFrame(
        std::optional<int> exposureMs = std::nullopt,
        std::optional<std::array<int, 4>> subframe = std::nullopt);

    /**
     * @brief Get search region size
     * @return Search region radius
     */
    int getSearchRegion() const;

    /**
     * @brief Get camera binning
     * @return Binning value
     */
    int getCameraBinning() const;

    /**
     * @brief Export config settings
     * @return Exported filename
     */
    std::string exportConfigSettings() const;

    /**
     * @brief Shutdown PHD2
     */
    void shutdown();

protected:
    // EventHandler interface
    void onEvent(const phd2::Event& event) override;
    void onConnectionError(std::string_view error) override;

private:
    /**
     * @brief Handle settle done event
     * @param success Whether settle was successful
     */
    void handleSettleDone(bool success);

    /**
     * @brief Update guider state from PHD2 app state
     * @param appState PHD2 application state
     */
    void updateGuiderState(const std::string& appState);

    /**
     * @brief Parse PHD2 event to update internal state
     * @param event PHD2 event
     */
    void processEvent(const phd2::Event& event);

    PHD2Config phd2Config_;
    std::unique_ptr<phd2::Connection> connection_;
    std::shared_ptr<phd2::EventHandler> internalHandler_;

    mutable std::mutex stateMutex_;
    GuideStar currentStar_;
    GuideStats guideStats_;
    CalibrationData calibrationData_;

    // Async operation support
    std::mutex promiseMutex_;
    std::promise<bool> settlePromise_;
    std::atomic<bool> settleInProgress_{false};
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_PHD2_CLIENT_HPP
