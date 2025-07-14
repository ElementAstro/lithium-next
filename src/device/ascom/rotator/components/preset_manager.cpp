#include "preset_manager.hpp"
#include "hardware_interface.hpp"
#include "position_manager.hpp"
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace lithium::device::ascom::rotator::components {

PresetManager::PresetManager(std::shared_ptr<HardwareInterface> hardware,
                           std::shared_ptr<PositionManager> position_manager)
    : hardware_(hardware), position_manager_(position_manager) {
    preset_directory_ = "./presets";
    initialize();
}

PresetManager::~PresetManager() {
    destroy();
}

auto PresetManager::initialize() -> bool {
    try {
        // Create preset directory if it doesn't exist
        std::filesystem::create_directories(preset_directory_);

        // Load existing presets
        loadPresetsFromFile();

        // Start auto-save thread if enabled
        if (auto_save_enabled_) {
            autosave_running_ = true;
            autosave_thread_ = std::make_unique<std::thread>(&PresetManager::autoSaveLoop, this);
        }

        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to initialize PresetManager: " + std::string(e.what()));
        return false;
    }
}

auto PresetManager::destroy() -> bool {
    try {
        // Stop auto-save thread
        autosave_running_ = false;
        if (autosave_thread_ && autosave_thread_->joinable()) {
            autosave_thread_->join();
        }

        // Save current presets
        savePresetsToFile();

        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to destroy PresetManager: " + std::string(e.what()));
        return false;
    }
}

auto PresetManager::savePreset(int slot, double angle, const std::string& name,
                             const std::string& description) -> bool {
    std::unique_lock<std::shared_mutex> lock(preset_mutex_);

    if (!validateSlot(slot)) {
        setLastError("Invalid slot number: " + std::to_string(slot));
        return false;
    }

    angle = normalizeAngle(angle);

    PresetInfo preset;
    preset.slot = slot;
    preset.name = name.empty() ? generatePresetName(slot, angle) : name;
    preset.angle = angle;
    preset.description = description;
    preset.created = std::chrono::system_clock::now();
    preset.last_used = preset.created;
    preset.use_count = 0;

    bool is_new = presets_.find(slot) == presets_.end();
    presets_[slot] = preset;

    if (auto_save_enabled_) {
        savePresetsToFile();
    }

    if (is_new) {
        notifyPresetCreated(slot, preset);
    } else {
        notifyPresetModified(slot, preset);
    }

    return true;
}

auto PresetManager::loadPreset(int slot) -> bool {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);

    auto it = presets_.find(slot);
    if (it == presets_.end()) {
        setLastError("Preset not found in slot: " + std::to_string(slot));
        return false;
    }

    // Update usage tracking
    lock.unlock();
    std::unique_lock<std::shared_mutex> write_lock(preset_mutex_);
    it->second.last_used = std::chrono::system_clock::now();
    it->second.use_count++;
    write_lock.unlock();

    // Move to preset position
    if (position_manager_) {
        bool success = position_manager_->moveToAngle(it->second.angle);
        if (success) {
            notifyPresetUsed(slot, it->second);
        }
        return success;
    }

    setLastError("Position manager not available");
    return false;
}

auto PresetManager::deletePreset(int slot) -> bool {
    std::unique_lock<std::shared_mutex> lock(preset_mutex_);

    auto it = presets_.find(slot);
    if (it == presets_.end()) {
        setLastError("Preset not found in slot: " + std::to_string(slot));
        return false;
    }

    presets_.erase(it);

    if (auto_save_enabled_) {
        savePresetsToFile();
    }

    notifyPresetDeleted(slot);
    return true;
}

auto PresetManager::hasPreset(int slot) -> bool {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);
    return presets_.find(slot) != presets_.end();
}

auto PresetManager::getPreset(int slot) -> std::optional<PresetInfo> {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);

    auto it = presets_.find(slot);
    if (it != presets_.end()) {
        return it->second;
    }

    return std::nullopt;
}

auto PresetManager::updatePreset(int slot, const PresetInfo& info) -> bool {
    std::unique_lock<std::shared_mutex> lock(preset_mutex_);

    if (!validateSlot(slot)) {
        setLastError("Invalid slot number: " + std::to_string(slot));
        return false;
    }

    if (!validatePresetData(info)) {
        setLastError("Invalid preset data");
        return false;
    }

    auto it = presets_.find(slot);
    if (it == presets_.end()) {
        setLastError("Preset not found in slot: " + std::to_string(slot));
        return false;
    }

    PresetInfo updated_info = info;
    updated_info.slot = slot;  // Ensure slot matches
    presets_[slot] = updated_info;

    if (auto_save_enabled_) {
        savePresetsToFile();
    }

    notifyPresetModified(slot, updated_info);
    return true;
}

auto PresetManager::getPresetAngle(int slot) -> std::optional<double> {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);

    auto it = presets_.find(slot);
    if (it != presets_.end()) {
        return it->second.angle;
    }

    return std::nullopt;
}

auto PresetManager::getPresetName(int slot) -> std::optional<std::string> {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);

    auto it = presets_.find(slot);
    if (it != presets_.end()) {
        return it->second.name;
    }

    return std::nullopt;
}

auto PresetManager::setPresetName(int slot, const std::string& name) -> bool {
    std::unique_lock<std::shared_mutex> lock(preset_mutex_);

    auto it = presets_.find(slot);
    if (it == presets_.end()) {
        setLastError("Preset not found in slot: " + std::to_string(slot));
        return false;
    }

    it->second.name = name;

    if (auto_save_enabled_) {
        savePresetsToFile();
    }

    notifyPresetModified(slot, it->second);
    return true;
}

auto PresetManager::setPresetDescription(int slot, const std::string& description) -> bool {
    std::unique_lock<std::shared_mutex> lock(preset_mutex_);

    auto it = presets_.find(slot);
    if (it == presets_.end()) {
        setLastError("Preset not found in slot: " + std::to_string(slot));
        return false;
    }

    it->second.description = description;

    if (auto_save_enabled_) {
        savePresetsToFile();
    }

    notifyPresetModified(slot, it->second);
    return true;
}

auto PresetManager::getAllPresets() -> std::vector<PresetInfo> {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);

    std::vector<PresetInfo> presets;
    presets.reserve(presets_.size());

    for (const auto& [slot, preset] : presets_) {
        presets.push_back(preset);
    }

    return presets;
}

auto PresetManager::getUsedSlots() -> std::vector<int> {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);

    std::vector<int> slots;
    slots.reserve(presets_.size());

    for (const auto& [slot, preset] : presets_) {
        slots.push_back(slot);
    }

    std::sort(slots.begin(), slots.end());
    return slots;
}

auto PresetManager::getFreeSlots() -> std::vector<int> {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);

    std::vector<int> free_slots;

    for (int slot = 1; slot <= max_presets_; ++slot) {
        if (presets_.find(slot) == presets_.end()) {
            free_slots.push_back(slot);
        }
    }

    return free_slots;
}

auto PresetManager::getNextFreeSlot() -> std::optional<int> {
    auto free_slots = getFreeSlots();
    if (!free_slots.empty()) {
        return free_slots.front();
    }
    return std::nullopt;
}

auto PresetManager::clearAllPresets() -> bool {
    std::unique_lock<std::shared_mutex> lock(preset_mutex_);

    presets_.clear();
    groups_.clear();

    if (auto_save_enabled_) {
        savePresetsToFile();
    }

    return true;
}

auto PresetManager::findPresetByName(const std::string& name) -> std::optional<int> {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);

    for (const auto& [slot, preset] : presets_) {
        if (preset.name == name) {
            return slot;
        }
    }

    return std::nullopt;
}

auto PresetManager::saveCurrentPosition(int slot, const std::string& name) -> bool {
    if (!position_manager_) {
        setLastError("Position manager not available");
        return false;
    }

    auto current_angle = position_manager_->getCurrentPosition();
    if (!current_angle.has_value()) {
        setLastError("Failed to get current position");
        return false;
    }

    return savePreset(slot, current_angle.value(), name);
}

auto PresetManager::moveToPreset(int slot) -> bool {
    return loadPreset(slot);
}

auto PresetManager::moveToPresetAsync(int slot) -> std::shared_ptr<std::future<bool>> {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = std::make_shared<std::future<bool>>(promise->get_future());

    std::thread([this, slot, promise]() {
        try {
            bool result = loadPreset(slot);
            promise->set_value(result);
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();

    return future;
}

auto PresetManager::getClosestPreset(double angle) -> std::optional<int> {
    std::shared_lock<std::shared_mutex> lock(preset_mutex_);

    if (presets_.empty()) {
        return std::nullopt;
    }

    angle = normalizeAngle(angle);
    int closest_slot = -1;
    double min_distance = std::numeric_limits<double>::max();

    for (const auto& [slot, preset] : presets_) {
        double distance = std::abs(preset.angle - angle);
        // Handle circular nature of angles
        distance = std::min(distance, 360.0 - distance);

        if (distance < min_distance) {
            min_distance = distance;
            closest_slot = slot;
        }
    }

    return closest_slot != -1 ? std::optional<int>(closest_slot) : std::nullopt;
}

// Helper methods implementation
auto PresetManager::validateSlot(int slot) -> bool {
    return slot >= 1 && slot <= max_presets_;
}

auto PresetManager::generatePresetName(int slot, double angle) -> std::string {
    return "Preset_" + std::to_string(slot) + "_" + std::to_string(static_cast<int>(angle)) + "deg";
}

auto PresetManager::normalizeAngle(double angle) -> double {
    while (angle < 0.0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    return angle;
}

auto PresetManager::setLastError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
}

auto PresetManager::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto PresetManager::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

auto PresetManager::validatePresetData(const PresetInfo& preset) -> bool {
    return !preset.name.empty() &&
           preset.angle >= 0.0 && preset.angle < 360.0 &&
           preset.slot >= 1 && preset.slot <= max_presets_;
}

auto PresetManager::notifyPresetCreated(int slot, const PresetInfo& info) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (preset_created_callback_) {
        preset_created_callback_(slot, info);
    }
}

auto PresetManager::notifyPresetDeleted(int slot) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (preset_deleted_callback_) {
        preset_deleted_callback_(slot);
    }
}

auto PresetManager::notifyPresetUsed(int slot, const PresetInfo& info) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (preset_used_callback_) {
        preset_used_callback_(slot, info);
    }
}

auto PresetManager::notifyPresetModified(int slot, const PresetInfo& info) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (preset_modified_callback_) {
        preset_modified_callback_(slot, info);
    }
}

auto PresetManager::loadPresetsFromFile() -> bool {
    try {
        std::string filename = preset_directory_ + "/presets.csv";
        std::ifstream file(filename);

        if (!file.is_open()) {
            return true; // No file exists yet, start with empty presets
        }

        std::unique_lock<std::shared_mutex> lock(preset_mutex_);
        presets_.clear();

        std::string line;
        bool first_line = true;

        while (std::getline(file, line)) {
            // Skip header line
            if (first_line) {
                first_line = false;
                if (line.find("slot,name,angle") != std::string::npos) {
                    continue;
                }
                // If no header, process this line as data
            }

            if (line.empty() || line[0] == '#') {
                continue; // Skip empty lines and comments
            }

            std::istringstream iss(line);
            std::string slot_str, name, angle_str, description, use_count_str, favorite_str;
            std::string created_str, last_used_str;

            if (std::getline(iss, slot_str, ',') &&
                std::getline(iss, name, ',') &&
                std::getline(iss, angle_str, ',') &&
                std::getline(iss, description, ',') &&
                std::getline(iss, use_count_str, ',') &&
                std::getline(iss, favorite_str, ',') &&
                std::getline(iss, created_str, ',') &&
                std::getline(iss, last_used_str)) {

                try {
                    PresetInfo preset;
                    preset.slot = std::stoi(slot_str);
                    preset.name = name;
                    preset.angle = std::stod(angle_str);
                    preset.description = description;
                    preset.use_count = std::stoi(use_count_str);
                    preset.is_favorite = (favorite_str == "1" || favorite_str == "true");

                    // Parse timestamps (simplified - just use current time for now)
                    preset.created = std::chrono::system_clock::now();
                    preset.last_used = std::chrono::system_clock::now();

                    if (validatePresetData(preset)) {
                        presets_[preset.slot] = preset;
                    }
                } catch (const std::exception&) {
                    // Skip invalid lines
                    continue;
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to load presets: " + std::string(e.what()));
        return false;
    }
}

auto PresetManager::savePresetsToFile() -> bool {
    try {
        std::filesystem::create_directories(preset_directory_);

        std::string filename = preset_directory_ + "/presets.csv";
        std::ofstream file(filename);

        if (!file.is_open()) {
            setLastError("Failed to open preset file for writing: " + filename);
            return false;
        }

        // Write header
        file << "slot,name,angle,description,use_count,is_favorite,created,last_used\n";

        {
            std::shared_lock<std::shared_mutex> lock(preset_mutex_);

            for (const auto& [slot, preset] : presets_) {
                file << preset.slot << ","
                     << preset.name << ","
                     << std::fixed << std::setprecision(6) << preset.angle << ","
                     << preset.description << ","
                     << preset.use_count << ","
                     << (preset.is_favorite ? "1" : "0") << ","
                     << std::chrono::duration_cast<std::chrono::seconds>(
                            preset.created.time_since_epoch()).count() << ","
                     << std::chrono::duration_cast<std::chrono::seconds>(
                            preset.last_used.time_since_epoch()).count() << "\n";
            }
        }

        last_save_ = std::chrono::system_clock::now();
        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to save presets: " + std::string(e.what()));
        return false;
    }
}

auto PresetManager::autoSaveLoop() -> void {
    while (autosave_running_) {
        std::this_thread::sleep_for(std::chrono::minutes(5)); // Auto-save every 5 minutes

        if (autosave_running_ && auto_save_enabled_) {
            savePresetsToFile();
        }
    }
}
