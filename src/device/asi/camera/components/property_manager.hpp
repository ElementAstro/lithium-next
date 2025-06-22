/*
 * property_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Property Manager Component

This component manages all camera properties, settings, and controls
including gain, offset, ROI, binning, and advanced camera features.

*************************************************/

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include <vector>
#include <functional>
#include <optional>

#include "../asi_camera_sdk_stub.hpp"

namespace lithium::device::asi::camera::components {

class HardwareInterface;

/**
 * @brief Property Manager for ASI Camera
 * 
 * Manages camera properties, controls, and settings with validation,
 * caching, and change notification capabilities.
 */
class PropertyManager {
public:
    struct PropertyInfo {
        std::string name;
        std::string description;
        ASI_CONTROL_TYPE controlType;
        long minValue = 0;
        long maxValue = 0;
        long defaultValue = 0;
        long currentValue = 0;
        bool isAuto = false;
        bool isAutoSupported = false;
        bool isWritable = false;
        bool isAvailable = false;
    };

    struct ROI {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool isValid() const { return width > 0 && height > 0; }
    };

    struct BinningMode {
        int binX = 1;
        int binY = 1;
        std::string description;
        bool operator==(const BinningMode& other) const {
            return binX == other.binX && binY == other.binY;
        }
    };

    struct ImageFormat {
        ASI_IMG_TYPE type;
        std::string name;
        std::string description;
        int bytesPerPixel;
        bool isColor;
    };

    using PropertyChangeCallback = std::function<void(ASI_CONTROL_TYPE, long, bool)>;
    using ROIChangeCallback = std::function<void(const ROI&)>;
    using BinningChangeCallback = std::function<void(const BinningMode&)>;

public:
    explicit PropertyManager(std::shared_ptr<HardwareInterface> hardware);
    ~PropertyManager();

    // Non-copyable and non-movable
    PropertyManager(const PropertyManager&) = delete;
    PropertyManager& operator=(const PropertyManager&) = delete;
    PropertyManager(PropertyManager&&) = delete;
    PropertyManager& operator=(PropertyManager&&) = delete;

    // Initialization and Discovery
    bool initialize();
    bool refresh();
    bool isInitialized() const { return initialized_; }
    
    // Property Information
    std::vector<PropertyInfo> getAllProperties() const;
    std::optional<PropertyInfo> getProperty(ASI_CONTROL_TYPE controlType) const;
    bool hasProperty(ASI_CONTROL_TYPE controlType) const;
    std::vector<ASI_CONTROL_TYPE> getAvailableProperties() const;
    
    // Property Control
    bool setProperty(ASI_CONTROL_TYPE controlType, long value, bool isAuto = false);
    bool getProperty(ASI_CONTROL_TYPE controlType, long& value, bool& isAuto) const;
    bool setPropertyAuto(ASI_CONTROL_TYPE controlType, bool enable);
    bool resetProperty(ASI_CONTROL_TYPE controlType);
    
    // Common Properties (convenience methods)
    bool setGain(int gain);
    int getGain() const;
    std::pair<int, int> getGainRange() const;
    bool setAutoGain(bool enable);
    bool isAutoGainEnabled() const;
    
    bool setExposure(long exposureUs);
    long getExposure() const;
    std::pair<long, long> getExposureRange() const;
    bool setAutoExposure(bool enable);
    bool isAutoExposureEnabled() const;
    
    bool setOffset(int offset);
    int getOffset() const;
    std::pair<int, int> getOffsetRange() const;
    
    bool setGamma(int gamma);
    int getGamma() const;
    std::pair<int, int> getGammaRange() const;
    
    bool setWhiteBalance(int wbR, int wbB);
    std::pair<int, int> getWhiteBalance() const;
    bool setAutoWhiteBalance(bool enable);
    bool isAutoWhiteBalanceEnabled() const;
    
    bool setUSBBandwidth(int bandwidth);
    int getUSBBandwidth() const;
    std::pair<int, int> getUSBBandwidthRange() const;
    
    bool setHighSpeedMode(bool enable);
    bool isHighSpeedModeEnabled() const;
    
    bool setHardwareBinning(bool enable);
    bool isHardwareBinningEnabled() const;
    
    // ROI Management
    bool setROI(const ROI& roi);
    bool setROI(int x, int y, int width, int height);
    ROI getROI() const;
    ROI getMaxROI() const;
    bool validateROI(const ROI& roi) const;
    bool resetROI();
    
    // Binning Management
    bool setBinning(const BinningMode& binning);
    bool setBinning(int binX, int binY);
    BinningMode getBinning() const;
    std::vector<BinningMode> getSupportedBinning() const;
    bool validateBinning(const BinningMode& binning) const;
    
    // Image Format Management
    bool setImageFormat(ASI_IMG_TYPE format);
    ASI_IMG_TYPE getImageFormat() const;
    std::vector<ImageFormat> getSupportedImageFormats() const;
    ImageFormat getImageFormatInfo(ASI_IMG_TYPE format) const;
    
    // Camera Mode Management
    bool setCameraMode(ASI_CAMERA_MODE mode);
    ASI_CAMERA_MODE getCameraMode() const;
    std::vector<ASI_CAMERA_MODE> getSupportedCameraModes() const;
    
    // Flip Control
    bool setFlipMode(ASI_FLIP_STATUS flip);
    ASI_FLIP_STATUS getFlipMode() const;
    
    // Advanced Settings
    bool setAntiDewHeater(bool enable);
    bool isAntiDewHeaterEnabled() const;
    
    bool setFan(bool enable);
    bool isFanEnabled() const;
    
    bool setPatternAdjust(bool enable);
    bool isPatternAdjustEnabled() const;
    
    // Presets and Profiles
    bool savePreset(const std::string& name);
    bool loadPreset(const std::string& name);
    std::vector<std::string> getAvailablePresets() const;
    bool deletePreset(const std::string& name);
    
    // Callbacks
    void setPropertyChangeCallback(PropertyChangeCallback callback);
    void setROIChangeCallback(ROIChangeCallback callback);
    void setBinningChangeCallback(BinningChangeCallback callback);
    
    // Validation and Constraints
    bool validatePropertyValue(ASI_CONTROL_TYPE controlType, long value) const;
    long clampPropertyValue(ASI_CONTROL_TYPE controlType, long value) const;
    
    // Batch Operations
    bool setMultipleProperties(const std::map<ASI_CONTROL_TYPE, std::pair<long, bool>>& properties);
    std::map<ASI_CONTROL_TYPE, std::pair<long, bool>> getAllPropertyValues() const;

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;
    
    // State management
    std::atomic<bool> initialized_{false};
    mutable std::mutex propertiesMutex_;
    
    // Property storage
    std::map<ASI_CONTROL_TYPE, PropertyInfo> properties_;
    
    // Current settings
    ROI currentROI_;
    BinningMode currentBinning_;
    ASI_IMG_TYPE currentImageFormat_ = ASI_IMG_RAW16;
    ASI_CAMERA_MODE currentCameraMode_ = ASI_MODE_NORMAL;
    ASI_FLIP_STATUS currentFlipMode_ = ASI_FLIP_NONE;
    
    // Callbacks
    PropertyChangeCallback propertyChangeCallback_;
    ROIChangeCallback roiChangeCallback_;
    BinningChangeCallback binningChangeCallback_;
    std::mutex callbackMutex_;
    
    // Presets storage
    std::map<std::string, std::map<ASI_CONTROL_TYPE, std::pair<long, bool>>> presets_;
    mutable std::mutex presetsMutex_;
    
    // Helper methods
    bool loadPropertyCapabilities();
    bool loadCurrentPropertyValues();
    PropertyInfo createPropertyInfo(const ASI_CONTROL_CAPS& caps) const;
    void updatePropertyValue(ASI_CONTROL_TYPE controlType, long value, bool isAuto);
    void notifyPropertyChange(ASI_CONTROL_TYPE controlType, long value, bool isAuto);
    void notifyROIChange(const ROI& roi);
    void notifyBinningChange(const BinningMode& binning);
    
    // Validation helpers
    bool isValidROI(const ROI& roi) const;
    bool isValidBinning(const BinningMode& binning) const;
    BinningMode normalizeBinning(const BinningMode& binning) const;
    
    // Format conversion helpers
    std::string controlTypeToString(ASI_CONTROL_TYPE controlType) const;
    std::string cameraModeToString(ASI_CAMERA_MODE mode) const;
    std::string flipStatusToString(ASI_FLIP_STATUS flip) const;
    std::string imageTypeToString(ASI_IMG_TYPE type) const;
    
    // Preset management
    bool savePresetToFile(const std::string& name, const std::map<ASI_CONTROL_TYPE, std::pair<long, bool>>& preset);
    bool loadPresetFromFile(const std::string& name, std::map<ASI_CONTROL_TYPE, std::pair<long, bool>>& preset);
    std::string getPresetFilename(const std::string& name) const;
};

} // namespace lithium::device::asi::camera::components
