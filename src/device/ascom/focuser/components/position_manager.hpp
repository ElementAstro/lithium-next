/*
 * position_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Focuser Position Manager Component

This component handles position tracking, preset management,
and position validation for ASCOM focuser devices.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace lithium::device::ascom::focuser::components {

// Forward declarations
class HardwareInterface;
class MovementController;

/**
 * @brief Position Manager for ASCOM Focuser
 *
 * This component manages focuser position tracking and presets:
 * - Position tracking and validation
 * - Preset position management
 * - Position history and statistics
 * - Auto-save functionality
 * - Position-based triggers
 */
class PositionManager {
public:
    // Position preset information
    struct PositionPreset {
        int position = 0;
        std::string name;
        std::string description;
        std::chrono::steady_clock::time_point created;
        std::chrono::steady_clock::time_point lastUsed;
        int useCount = 0;
        bool isProtected = false;
        double temperature = 0.0;  // Temperature when preset was created
    };

    // Position history entry
    struct PositionHistoryEntry {
        std::chrono::steady_clock::time_point timestamp;
        int position;
        std::string source;  // "manual", "preset", "auto", "compensation"
        std::string description;
        double temperature;
        int moveSteps;
        std::chrono::milliseconds moveDuration;
    };

    // Position statistics
    struct PositionStats {
        int currentPosition = 0;
        int minPosition = 0;
        int maxPosition = 65535;
        int totalMoves = 0;
        int averagePosition = 0;
        int mostUsedPosition = 0;
        std::chrono::steady_clock::time_point lastMoveTime;
        std::chrono::milliseconds totalMoveTime{0};
        std::chrono::milliseconds averageMoveTime{0};
    };

    // Position configuration
    struct PositionConfig {
        bool enableAutoSave = true;
        std::chrono::seconds autoSaveInterval{300};  // 5 minutes
        std::string autoSaveFile = "focuser_positions.json";
        int maxHistoryEntries = 500;
        int maxPresets = 20;
        bool enablePositionValidation = true;
        int positionTolerance = 5;  // Steps
        bool enablePositionTriggers = true;
    };

    // Position trigger for automated actions
    struct PositionTrigger {
        int position;
        int tolerance;
        std::function<void(int position)> callback;
        std::string description;
        bool enabled = true;
        int triggerCount = 0;
    };

    // Constructor and destructor
    explicit PositionManager(std::shared_ptr<HardwareInterface> hardware,
                            std::shared_ptr<MovementController> movement);
    ~PositionManager();

    // Non-copyable and non-movable
    PositionManager(const PositionManager&) = delete;
    PositionManager& operator=(const PositionManager&) = delete;
    PositionManager(PositionManager&&) = delete;
    PositionManager& operator=(PositionManager&&) = delete;

    // =========================================================================
    // Initialization and Configuration
    // =========================================================================

    /**
     * @brief Initialize the position manager
     */
    auto initialize() -> bool;

    /**
     * @brief Destroy the position manager
     */
    auto destroy() -> bool;

    /**
     * @brief Set position configuration
     */
    auto setPositionConfig(const PositionConfig& config) -> void;

    /**
     * @brief Get position configuration
     */
    auto getPositionConfig() const -> PositionConfig;

    // =========================================================================
    // Position Tracking
    // =========================================================================

    /**
     * @brief Get current position
     */
    auto getCurrentPosition() -> int;

    /**
     * @brief Update position tracking
     */
    auto updatePosition(int position, const std::string& source = "manual") -> void;

    /**
     * @brief Get position statistics
     */
    auto getPositionStats() -> PositionStats;

    /**
     * @brief Reset position statistics
     */
    auto resetPositionStats() -> void;

    /**
     * @brief Validate position
     */
    auto validatePosition(int position) -> bool;

    /**
     * @brief Get position tolerance
     */
    auto getPositionTolerance() -> int;

    /**
     * @brief Set position tolerance
     */
    auto setPositionTolerance(int tolerance) -> void;

    // =========================================================================
    // Preset Management
    // =========================================================================

    /**
     * @brief Save position to preset slot
     */
    auto savePreset(int slot, int position, const std::string& name = "",
                   const std::string& description = "") -> bool;

    /**
     * @brief Load position from preset slot
     */
    auto loadPreset(int slot) -> bool;

    /**
     * @brief Get preset position
     */
    auto getPreset(int slot) -> std::optional<PositionPreset>;

    /**
     * @brief Get all presets
     */
    auto getAllPresets() -> std::unordered_map<int, PositionPreset>;

    /**
     * @brief Delete preset
     */
    auto deletePreset(int slot) -> bool;

    /**
     * @brief Check if preset exists
     */
    auto hasPreset(int slot) -> bool;

    /**
     * @brief Get preset count
     */
    auto getPresetCount() -> int;

    /**
     * @brief Clear all presets
     */
    auto clearAllPresets() -> bool;

    /**
     * @brief Get available preset slots
     */
    auto getAvailablePresetSlots() -> std::vector<int>;

    /**
     * @brief Find preset by name
     */
    auto findPresetByName(const std::string& name) -> std::optional<int>;

    /**
     * @brief Rename preset
     */
    auto renamePreset(int slot, const std::string& newName) -> bool;

    /**
     * @brief Protect/unprotect preset
     */
    auto setPresetProtection(int slot, bool protected_) -> bool;

    // =========================================================================
    // Position History
    // =========================================================================

    /**
     * @brief Get position history
     */
    auto getPositionHistory() -> std::vector<PositionHistoryEntry>;

    /**
     * @brief Get position history for specified duration
     */
    auto getPositionHistory(std::chrono::seconds duration) -> std::vector<PositionHistoryEntry>;

    /**
     * @brief Clear position history
     */
    auto clearPositionHistory() -> void;

    /**
     * @brief Add position history entry
     */
    auto addPositionHistoryEntry(int position, const std::string& source,
                                const std::string& description = "") -> void;

    /**
     * @brief Get position usage statistics
     */
    auto getPositionUsageStats() -> std::unordered_map<int, int>;

    /**
     * @brief Get most frequently used positions
     */
    auto getMostUsedPositions(int count = 10) -> std::vector<std::pair<int, int>>;

    // =========================================================================
    // Position Triggers
    // =========================================================================

    /**
     * @brief Add position trigger
     */
    auto addPositionTrigger(int position, int tolerance,
                           std::function<void(int)> callback,
                           const std::string& description = "") -> int;

    /**
     * @brief Remove position trigger
     */
    auto removePositionTrigger(int triggerId) -> bool;

    /**
     * @brief Enable/disable position trigger
     */
    auto setPositionTriggerEnabled(int triggerId, bool enabled) -> bool;

    /**
     * @brief Get position triggers
     */
    auto getPositionTriggers() -> std::vector<PositionTrigger>;

    /**
     * @brief Clear all position triggers
     */
    auto clearPositionTriggers() -> void;

    /**
     * @brief Check and fire position triggers
     */
    auto checkPositionTriggers(int position) -> void;

    // =========================================================================
    // Auto-Save and Persistence
    // =========================================================================

    /**
     * @brief Enable/disable auto-save
     */
    auto enableAutoSave(bool enable) -> void;

    /**
     * @brief Save presets to file
     */
    auto savePresetsToFile(const std::string& filename) -> bool;

    /**
     * @brief Load presets from file
     */
    auto loadPresetsFromFile(const std::string& filename) -> bool;

    /**
     * @brief Auto-save presets
     */
    auto autoSavePresets() -> bool;

    /**
     * @brief Export presets to JSON
     */
    auto exportPresetsToJson() -> std::string;

    /**
     * @brief Import presets from JSON
     */
    auto importPresetsFromJson(const std::string& json) -> bool;

    // =========================================================================
    // Callbacks and Events
    // =========================================================================

    using PositionChangeCallback = std::function<void(int oldPosition, int newPosition)>;
    using PresetCallback = std::function<void(int slot, const PositionPreset& preset)>;
    using PositionTriggerCallback = std::function<void(int position, const std::string& description)>;

    /**
     * @brief Set position change callback
     */
    auto setPositionChangeCallback(PositionChangeCallback callback) -> void;

    /**
     * @brief Set preset callback
     */
    auto setPresetCallback(PresetCallback callback) -> void;

    /**
     * @brief Set position trigger callback
     */
    auto setPositionTriggerCallback(PositionTriggerCallback callback) -> void;

    // =========================================================================
    // Advanced Features
    // =========================================================================

    /**
     * @brief Get position recommendations based on history
     */
    auto getPositionRecommendations(int count = 5) -> std::vector<int>;

    /**
     * @brief Find optimal position between two positions
     */
    auto findOptimalPosition(int startPos, int endPos) -> int;

    /**
     * @brief Get position difference
     */
    auto getPositionDifference(int pos1, int pos2) -> int;

    /**
     * @brief Check if position is close to any preset
     */
    auto findNearbyPreset(int position, int tolerance) -> std::optional<int>;

    /**
     * @brief Get position accuracy statistics
     */
    auto getPositionAccuracy() -> double;

private:
    // Component references
    std::shared_ptr<HardwareInterface> hardware_;
    std::shared_ptr<MovementController> movement_;

    // Configuration
    PositionConfig config_;

    // Position tracking
    std::atomic<int> current_position_{0};
    std::atomic<int> last_position_{0};

    // Presets storage
    std::unordered_map<int, PositionPreset> presets_;
    static constexpr int MAX_PRESET_SLOTS = 20;

    // Position history
    std::vector<PositionHistoryEntry> position_history_;

    // Position triggers
    std::vector<PositionTrigger> position_triggers_;
    int next_trigger_id_{0};

    // Statistics
    PositionStats stats_;

    // Threading and synchronization
    std::thread auto_save_thread_;
    std::atomic<bool> auto_save_active_{false};
    mutable std::mutex presets_mutex_;
    mutable std::mutex history_mutex_;
    mutable std::mutex stats_mutex_;
    mutable std::mutex config_mutex_;
    mutable std::mutex triggers_mutex_;

    // Callbacks
    PositionChangeCallback position_change_callback_;
    PresetCallback preset_callback_;
    PositionTriggerCallback position_trigger_callback_;

    // Private methods
    auto autoSaveLoop() -> void;
    auto startAutoSave() -> void;
    auto stopAutoSave() -> void;

    auto updatePositionStats(int position) -> void;
    auto addPositionToHistory(int position, const std::string& source,
                             const std::string& description) -> void;
    auto cleanupOldHistory() -> void;

    auto validatePresetSlot(int slot) -> bool;
    auto generatePresetName(int slot) -> std::string;
    auto updatePresetUsage(int slot) -> void;

    auto notifyPositionChange(int oldPosition, int newPosition) -> void;
    auto notifyPresetAction(int slot, const PositionPreset& preset) -> void;
    auto notifyPositionTrigger(int position, const std::string& description) -> void;

    // Utility methods
    auto getCurrentTemperature() -> double;
    auto formatPosition(int position) -> std::string;
    auto isValidPresetSlot(int slot) -> bool;
    auto findEmptyPresetSlot() -> std::optional<int>;

    // JSON serialization helpers
    auto presetToJson(const PositionPreset& preset) -> std::string;
    auto presetFromJson(const std::string& json) -> std::optional<PositionPreset>;
    auto historyEntryToJson(const PositionHistoryEntry& entry) -> std::string;
    auto historyEntryFromJson(const std::string& json) -> std::optional<PositionHistoryEntry>;
};

} // namespace lithium::device::ascom::focuser::components
