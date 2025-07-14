/*
 * property_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Property Manager Component

This component manages camera properties, settings, and configuration
including gain, offset, binning, ROI, and other camera parameters.

*************************************************/

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <optional>
#include <vector>
#include <map>
#include <variant>

#include "device/template/camera_frame.hpp"
#include "device/template/camera.hpp"

namespace lithium::device::ascom::camera::components {

class HardwareInterface;

/**
 * @brief Property Manager for ASCOM Camera
 *
 * Manages camera properties, settings validation, and configuration
 * with support for property constraints and change notifications.
 */
class PropertyManager {
public:
    using PropertyValue = std::variant<int, double, bool, std::string>;

    struct PropertyInfo {
        std::string name;
        std::string description;
        PropertyValue currentValue;
        PropertyValue defaultValue;
        PropertyValue minValue;
        PropertyValue maxValue;
        bool isReadOnly = false;
        bool isAvailable = true;
        std::vector<PropertyValue> allowedValues; // For enumerated properties
    };

    struct FrameSettings {
        int startX = 0;
        int startY = 0;
        int width = 0;      // 0 = full frame
        int height = 0;     // 0 = full frame
        int binX = 1;
        int binY = 1;
        FrameType frameType = FrameType::FITS;
        UploadMode uploadMode = UploadMode::LOCAL;
    };

    struct ROI {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    struct ImageSettings {
        int gain = 0;
        int offset = 0;
        int iso = 0;
        double pixelSize = 0.0;
        int bitDepth = 16;
        bool isColor = false;
        BayerPattern bayerPattern = BayerPattern::MONO;
    };

    using PropertyChangeCallback = std::function<void(const std::string& name, const PropertyValue& oldValue, const PropertyValue& newValue)>;

public:
    explicit PropertyManager(std::shared_ptr<HardwareInterface> hardware);
    ~PropertyManager() = default;

    // Non-copyable and non-movable
    PropertyManager(const PropertyManager&) = delete;
    PropertyManager& operator=(const PropertyManager&) = delete;
    PropertyManager(PropertyManager&&) = delete;
    PropertyManager& operator=(PropertyManager&&) = delete;

    // =========================================================================
    // Property Management
    // =========================================================================

    /**
     * @brief Initialize property manager and load camera properties
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Refresh all properties from camera
     * @return true if refresh successful
     */
    bool refreshProperties();

    /**
     * @brief Get property information
     * @param name Property name
     * @return Optional property info
     */
    std::optional<PropertyInfo> getPropertyInfo(const std::string& name) const;

    /**
     * @brief Get property value
     * @param name Property name
     * @return Optional property value
     */
    std::optional<PropertyValue> getProperty(const std::string& name) const;

    /**
     * @brief Set property value
     * @param name Property name
     * @param value New value
     * @return true if set successfully
     */
    bool setProperty(const std::string& name, const PropertyValue& value);

    /**
     * @brief Get all available properties
     * @return Map of property name to info
     */
    std::map<std::string, PropertyInfo> getAllProperties() const;

    /**
     * @brief Check if property exists and is available
     * @param name Property name
     * @return true if property is available
     */
    bool isPropertyAvailable(const std::string& name) const;

    // =========================================================================
    // Gain and Offset Control
    // =========================================================================

    /**
     * @brief Set camera gain
     * @param gain Gain value
     * @return true if set successfully
     */
    bool setGain(int gain);

    /**
     * @brief Get camera gain
     * @return Current gain value
     */
    std::optional<int> getGain() const;

    /**
     * @brief Get gain range
     * @return Pair of min, max gain values
     */
    std::pair<int, int> getGainRange() const;

    /**
     * @brief Set camera offset
     * @param offset Offset value
     * @return true if set successfully
     */
    bool setOffset(int offset);

    /**
     * @brief Get camera offset
     * @return Current offset value
     */
    std::optional<int> getOffset() const;

    /**
     * @brief Get offset range
     * @return Pair of min, max offset values
     */
    std::pair<int, int> getOffsetRange() const;

    /**
     * @brief Set ISO value
     * @param iso ISO value
     * @return true if set successfully
     */
    bool setISO(int iso);

    /**
     * @brief Get ISO value
     * @return Current ISO value
     */
    std::optional<int> getISO() const;

    /**
     * @brief Get available ISO values
     * @return Vector of available ISO values
     */
    std::vector<int> getISOList() const;

    // =========================================================================
    // Frame and Resolution Settings
    // =========================================================================

    /**
     * @brief Set frame settings
     * @param settings Frame configuration
     * @return true if set successfully
     */
    bool setFrameSettings(const FrameSettings& settings);

    /**
     * @brief Get current frame settings
     * @return Current frame settings
     */
    FrameSettings getFrameSettings() const;

    /**
     * @brief Set resolution and ROI
     * @param x Starting X coordinate
     * @param y Starting Y coordinate
     * @param width Frame width
     * @param height Frame height
     * @return true if set successfully
     */
    bool setResolution(int x, int y, int width, int height);

    /**
     * @brief Get current resolution
     * @return Optional resolution structure
     */
    std::optional<AtomCameraFrame::Resolution> getResolution() const;

    /**
     * @brief Get maximum resolution
     * @return Maximum camera resolution
     */
    AtomCameraFrame::Resolution getMaxResolution() const;

    /**
     * @brief Set binning
     * @param binX Horizontal binning
     * @param binY Vertical binning
     * @return true if set successfully
     */
    bool setBinning(int binX, int binY);

    /**
     * @brief Set binning using Binning struct
     * @param binning Binning parameters
     * @return true if set successfully
     */
    bool setBinning(const AtomCameraFrame::Binning& binning);

    /**
     * @brief Get current binning
     * @return Optional binning structure
     */
    std::optional<AtomCameraFrame::Binning> getBinning() const;

    /**
     * @brief Get maximum binning
     * @return Maximum binning values
     */
    AtomCameraFrame::Binning getMaxBinning() const;

    /**
     * @brief Set ROI (Region of Interest)
     * @param roi ROI parameters
     * @return true if set successfully
     */
    bool setROI(const ROI& roi);

    /**
     * @brief Get current ROI
     * @return Current ROI settings
     */
    ROI getROI() const;

    /**
     * @brief Get maximum ROI
     * @return Maximum ROI dimensions
     */
    ROI getMaxROI() const;

    /**
     * @brief Set frame type
     * @param type Frame type
     * @return true if set successfully
     */
    bool setFrameType(FrameType type);

    /**
     * @brief Get current frame type
     * @return Current frame type
     */
    FrameType getFrameType() const;

    /**
     * @brief Set upload mode
     * @param mode Upload mode
     * @return true if set successfully
     */
    bool setUploadMode(UploadMode mode);

    /**
     * @brief Get current upload mode
     * @return Current upload mode
     */
    UploadMode getUploadMode() const;

    // =========================================================================
    // Image and Sensor Properties
    // =========================================================================

    /**
     * @brief Get image settings
     * @return Current image settings
     */
    ImageSettings getImageSettings() const;

    /**
     * @brief Get pixel size
     * @return Pixel size in micrometers
     */
    double getPixelSize() const;

    /**
     * @brief Get pixel size X
     * @return Pixel size X in micrometers
     */
    double getPixelSizeX() const;

    /**
     * @brief Get pixel size Y
     * @return Pixel size Y in micrometers
     */
    double getPixelSizeY() const;

    /**
     * @brief Get bit depth
     * @return Image bit depth
     */
    int getBitDepth() const;

    /**
     * @brief Check if camera is color
     * @return true if color camera
     */
    bool isColor() const;

    /**
     * @brief Get Bayer pattern
     * @return Current Bayer pattern
     */
    BayerPattern getBayerPattern() const;

    /**
     * @brief Set Bayer pattern
     * @param pattern Bayer pattern
     * @return true if set successfully
     */
    bool setBayerPattern(BayerPattern pattern);

    // =========================================================================
    // Advanced Properties
    // =========================================================================

    /**
     * @brief Check if camera has shutter
     * @return true if shutter available
     */
    bool hasShutter() const;

    /**
     * @brief Control shutter
     * @param open True to open shutter
     * @return true if operation successful
     */
    bool setShutter(bool open);

    /**
     * @brief Get shutter status
     * @return true if shutter is open
     */
    bool getShutterStatus() const;

    /**
     * @brief Check if camera has fan
     * @return true if fan available
     */
    bool hasFan() const;

    /**
     * @brief Set fan speed
     * @param speed Fan speed (0-100%)
     * @return true if set successfully
     */
    bool setFanSpeed(int speed);

    /**
     * @brief Get fan speed
     * @return Current fan speed
     */
    int getFanSpeed() const;

    /**
     * @brief Get frame info structure
     * @return Current frame information
     */
    std::shared_ptr<AtomCameraFrame> getFrameInfo() const;

    // =========================================================================
    // Property Validation and Constraints
    // =========================================================================

    /**
     * @brief Validate property value
     * @param name Property name
     * @param value Value to validate
     * @return true if value is valid
     */
    bool validateProperty(const std::string& name, const PropertyValue& value) const;

    /**
     * @brief Get property constraints
     * @param name Property name
     * @return String describing constraints
     */
    std::string getPropertyConstraints(const std::string& name) const;

    /**
     * @brief Reset property to default value
     * @param name Property name
     * @return true if reset successful
     */
    bool resetProperty(const std::string& name);

    /**
     * @brief Reset all properties to defaults
     * @return true if reset successful
     */
    bool resetAllProperties();

    // =========================================================================
    // Callbacks and Notifications
    // =========================================================================

    /**
     * @brief Set property change callback
     * @param callback Callback function
     */
    void setPropertyChangeCallback(PropertyChangeCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        propertyChangeCallback_ = std::move(callback);
    }

    /**
     * @brief Enable/disable property change notifications
     * @param enable True to enable notifications
     */
    void setNotificationsEnabled(bool enable) { notificationsEnabled_ = enable; }

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // Property storage
    mutable std::mutex propertiesMutex_;
    std::map<std::string, PropertyInfo> properties_;

    // Current settings cache
    mutable std::mutex settingsMutex_;
    FrameSettings currentFrameSettings_;
    ImageSettings currentImageSettings_;

    // Callbacks
    mutable std::mutex callbackMutex_;
    PropertyChangeCallback propertyChangeCallback_;
    std::atomic<bool> notificationsEnabled_{true};

    // Helper methods
    void loadCameraProperties();
    void loadProperty(const std::string& name);
    bool updatePropertyFromCamera(const std::string& name);
    bool applyPropertyToCamera(const std::string& name, const PropertyValue& value);
    void notifyPropertyChange(const std::string& name, const PropertyValue& oldValue, const PropertyValue& newValue);

    // Property type helpers
    template<typename T>
    std::optional<T> getTypedProperty(const std::string& name) const;

    template<typename T>
    bool setTypedProperty(const std::string& name, const T& value);

    // Validation helpers
    bool isValueInRange(const PropertyValue& value, const PropertyValue& min, const PropertyValue& max) const;
    bool isValueInAllowedList(const PropertyValue& value, const std::vector<PropertyValue>& allowedValues) const;

    // Property name constants
    static const std::string PROPERTY_GAIN;
    static const std::string PROPERTY_OFFSET;
    static const std::string PROPERTY_ISO;
    static const std::string PROPERTY_BINX;
    static const std::string PROPERTY_BINY;
    static const std::string PROPERTY_STARTX;
    static const std::string PROPERTY_STARTY;
    static const std::string PROPERTY_NUMX;
    static const std::string PROPERTY_NUMY;
    static const std::string PROPERTY_FRAME_TYPE;
    static const std::string PROPERTY_UPLOAD_MODE;
    static const std::string PROPERTY_PIXEL_SIZE_X;
    static const std::string PROPERTY_PIXEL_SIZE_Y;
    static const std::string PROPERTY_BIT_DEPTH;
    static const std::string PROPERTY_IS_COLOR;
    static const std::string PROPERTY_BAYER_PATTERN;
    static const std::string PROPERTY_HAS_SHUTTER;
    static const std::string PROPERTY_SHUTTER_OPEN;
    static const std::string PROPERTY_HAS_FAN;
    static const std::string PROPERTY_FAN_SPEED;
};

} // namespace lithium::device::ascom::camera::components
