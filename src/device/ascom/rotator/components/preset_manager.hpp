/*
 * preset_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Rotator Preset Manager Component

This component manages rotator position presets, providing
storage, retrieval, and management of named positions.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace lithium::device::ascom::rotator::components {

// Forward declarations
class HardwareInterface;
class PositionManager;

/**
 * @brief Preset information structure
 */
struct PresetInfo {
    int slot{0};
    std::string name;
    double angle{0.0};
    std::string description;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_used;
    int use_count{0};
    bool is_favorite{false};
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Preset group for organizing related presets
 */
struct PresetGroup {
    std::string name;
    std::string description;
    std::vector<int> preset_slots;
    bool is_active{true};
    std::chrono::system_clock::time_point created;
};

/**
 * @brief Preset import/export format
 */
struct PresetExportData {
    std::string version{"1.0"};
    std::chrono::system_clock::time_point export_time;
    std::string device_name;
    std::vector<PresetInfo> presets;
    std::vector<PresetGroup> groups;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Preset Manager for ASCOM Rotator
 *
 * This component provides comprehensive preset management including
 * storage, organization, import/export, and automated positioning.
 */
class PresetManager {
public:
    explicit PresetManager(std::shared_ptr<HardwareInterface> hardware,
                          std::shared_ptr<PositionManager> position_manager);
    ~PresetManager();

    // Lifecycle management
    auto initialize() -> bool;
    auto destroy() -> bool;

    // Basic preset operations
    auto savePreset(int slot, double angle, const std::string& name = "",
                   const std::string& description = "") -> bool;
    auto loadPreset(int slot) -> bool;
    auto deletePreset(int slot) -> bool;
    auto hasPreset(int slot) -> bool;
    auto getPreset(int slot) -> std::optional<PresetInfo>;
    auto updatePreset(int slot, const PresetInfo& info) -> bool;

    // Preset information
    auto getPresetAngle(int slot) -> std::optional<double>;
    auto getPresetName(int slot) -> std::optional<std::string>;
    auto setPresetName(int slot, const std::string& name) -> bool;
    auto setPresetDescription(int slot, const std::string& description) -> bool;
    auto getPresetMetadata(int slot, const std::string& key) -> std::optional<std::string>;
    auto setPresetMetadata(int slot, const std::string& key, const std::string& value) -> bool;

    // Preset management
    auto getAllPresets() -> std::vector<PresetInfo>;
    auto getUsedSlots() -> std::vector<int>;
    auto getFreeSlots() -> std::vector<int>;
    auto getNextFreeSlot() -> std::optional<int>;
    auto copyPreset(int from_slot, int to_slot) -> bool;
    auto swapPresets(int slot1, int slot2) -> bool;
    auto clearAllPresets() -> bool;

    // Search and filtering
    auto findPresetByName(const std::string& name) -> std::optional<int>;
    auto findPresetsByGroup(const std::string& group_name) -> std::vector<int>;
    auto findPresetsNearAngle(double angle, double tolerance = 1.0) -> std::vector<int>;
    auto searchPresets(const std::string& query) -> std::vector<int>;

    // Position operations
    auto saveCurrentPosition(int slot, const std::string& name = "") -> bool;
    auto moveToPreset(int slot) -> bool;
    auto moveToPresetAsync(int slot) -> std::shared_ptr<std::future<bool>>;
    auto getClosestPreset(double angle) -> std::optional<int>;
    auto snapToNearestPreset(double tolerance = 5.0) -> std::optional<int>;

    // Preset groups
    auto createGroup(const std::string& name, const std::string& description = "") -> bool;
    auto deleteGroup(const std::string& name) -> bool;
    auto addPresetToGroup(int slot, const std::string& group_name) -> bool;
    auto removePresetFromGroup(int slot, const std::string& group_name) -> bool;
    auto getGroups() -> std::vector<PresetGroup>;
    auto getGroup(const std::string& name) -> std::optional<PresetGroup>;
    auto renameGroup(const std::string& old_name, const std::string& new_name) -> bool;

    // Favorites and usage tracking
    auto setPresetFavorite(int slot, bool is_favorite) -> bool;
    auto isPresetFavorite(int slot) -> bool;
    auto getFavoritePresets() -> std::vector<int>;
    auto getMostUsedPresets(int count = 10) -> std::vector<int>;
    auto getRecentlyUsedPresets(int count = 5) -> std::vector<int>;
    auto updatePresetUsage(int slot) -> void;

    // Import/Export
    auto exportPresets(const std::string& filename) -> bool;
    auto importPresets(const std::string& filename, bool merge = true) -> bool;
    auto exportPresetsToString() -> std::string;
    auto importPresetsFromString(const std::string& data, bool merge = true) -> bool;
    auto backupPresets(const std::string& backup_name = "") -> bool;
    auto restorePresets(const std::string& backup_name) -> bool;

    // Configuration
    auto setMaxPresets(int max_presets) -> bool;
    auto getMaxPresets() -> int;
    auto setAutoSaveEnabled(bool enabled) -> bool;
    auto isAutoSaveEnabled() -> bool;
    auto setPresetDirectory(const std::string& directory) -> bool;
    auto getPresetDirectory() -> std::string;

    // Validation and verification
    auto validatePreset(int slot) -> bool;
    auto validateAllPresets() -> std::vector<int>; // Returns invalid slots
    auto repairPreset(int slot) -> bool;
    auto optimizePresetStorage() -> bool;

    // Event callbacks
    auto setPresetCreatedCallback(std::function<void(int, const PresetInfo&)> callback) -> void;
    auto setPresetDeletedCallback(std::function<void(int)> callback) -> void;
    auto setPresetUsedCallback(std::function<void(int, const PresetInfo&)> callback) -> void;
    auto setPresetModifiedCallback(std::function<void(int, const PresetInfo&)> callback) -> void;

    // Statistics
    auto getPresetStatistics() -> std::unordered_map<std::string, int>;
    auto getUsageStatistics() -> std::unordered_map<int, int>; // slot -> use count
    auto getTotalPresets() -> int;
    auto getAverageUsage() -> double;

    // Error handling
    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

private:
    // Hardware and position interfaces
    std::shared_ptr<HardwareInterface> hardware_;
    std::shared_ptr<PositionManager> position_manager_;

    // Preset storage
    std::unordered_map<int, PresetInfo> presets_;
    std::unordered_map<std::string, PresetGroup> groups_;
    int max_presets_{100};
    mutable std::shared_mutex preset_mutex_;

    // Configuration
    std::string preset_directory_;
    bool auto_save_enabled_{true};
    bool auto_backup_enabled_{true};
    int backup_interval_hours_{24};

    // Event callbacks
    std::function<void(int, const PresetInfo&)> preset_created_callback_;
    std::function<void(int)> preset_deleted_callback_;
    std::function<void(int, const PresetInfo&)> preset_used_callback_;
    std::function<void(int, const PresetInfo&)> preset_modified_callback_;
    mutable std::mutex callback_mutex_;

    // Auto-save and backup
    std::unique_ptr<std::thread> autosave_thread_;
    std::atomic<bool> autosave_running_{false};
    std::chrono::system_clock::time_point last_save_;
    std::chrono::system_clock::time_point last_backup_;

    // Error handling
    std::string last_error_;
    mutable std::mutex error_mutex_;

    // Helper methods
    auto loadPresetsFromFile() -> bool;
    auto savePresetsToFile() -> bool;
    auto validateSlot(int slot) -> bool;
    auto generatePresetName(int slot, double angle) -> std::string;
    auto normalizeAngle(double angle) -> double;
    auto setLastError(const std::string& error) -> void;
    auto notifyPresetCreated(int slot, const PresetInfo& info) -> void;
    auto notifyPresetDeleted(int slot) -> void;
    auto notifyPresetUsed(int slot, const PresetInfo& info) -> void;
    auto notifyPresetModified(int slot, const PresetInfo& info) -> void;
    auto autoSaveLoop() -> void;
    auto createBackupFilename(const std::string& backup_name = "") -> std::string;
    auto serializePresets() -> std::string;
    auto deserializePresets(const std::string& data) -> PresetExportData;
    auto mergePresets(const PresetExportData& import_data) -> bool;
    auto replacePresets(const PresetExportData& import_data) -> bool;
    auto getUniqueSlotForImport(int preferred_slot) -> int;
    auto validatePresetData(const PresetInfo& preset) -> bool;
    auto cleanupInvalidPresets() -> int;
};

} // namespace lithium::device::ascom::rotator::components
