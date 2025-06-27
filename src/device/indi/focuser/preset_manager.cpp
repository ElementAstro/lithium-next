#include "preset_manager.hpp"
#include <cmath>

namespace lithium::device::indi::focuser {

// Static preset storage - could be moved to persistent storage later
static std::array<std::optional<int>, 10> presets_; // 10 preset slots

PresetManager::PresetManager(std::shared_ptr<INDIFocuserCore> core)
    : FocuserComponentBase(std::move(core)) {
}

bool PresetManager::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }
    
    core->getLogger()->info("{}: Initializing preset manager", getComponentName());
    return true;
}

void PresetManager::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("{}: Shutting down preset manager", getComponentName());
    }
}

bool PresetManager::savePreset(int slot, int position) {
    if (!isValidSlot(slot)) {
        auto core = getCore();
        if (core) {
            core->getLogger()->error("Invalid preset slot: {}", slot);
        }
        return false;
    }
    
    presets_[slot] = position;
    
    auto core = getCore();
    if (core) {
        core->getLogger()->info("Saved preset {} with position {}", slot, position);
    }
    return true;
}

bool PresetManager::loadPreset(int slot) {
    auto core = getCore();
    if (!core || !isValidSlot(slot)) {
        if (core) {
            core->getLogger()->error("Invalid preset slot: {}", slot);
        }
        return false;
    }
    
    if (!presets_[slot]) {
        core->getLogger()->error("Preset slot {} is empty", slot);
        return false;
    }
    
    int position = *presets_[slot];
    core->getLogger()->info("Loading preset {} with position {}", slot, position);
    
    // Send position command via INDI
    if (core->getDevice().isValid() && core->getClient()) {
        INDI::PropertyNumber absProp = core->getDevice().getProperty("ABS_FOCUS_POSITION");
        if (absProp.isValid()) {
            absProp[0].value = position;
            core->getClient()->sendNewProperty(absProp);
            core->getLogger()->info("Moving to preset position {} via INDI", position);
            return true;
        } else {
            core->getLogger()->error("ABS_FOCUS_POSITION property not available");
            return false;
        }
    } else {
        core->getLogger()->error("Device or client not available");
        return false;
    }
}

std::optional<int> PresetManager::getPreset(int slot) const {
    if (!isValidSlot(slot)) {
        return std::nullopt;
    }
    return presets_[slot];
}

bool PresetManager::deletePreset(int slot) {
    if (!isValidSlot(slot)) {
        auto core = getCore();
        if (core) {
            core->getLogger()->error("Invalid preset slot: {}", slot);
        }
        return false;
    }
    
    if (!presets_[slot]) {
        auto core = getCore();
        if (core) {
            core->getLogger()->warn("Preset slot {} is already empty", slot);
        }
        return true; // Already empty, consider it success
    }
    
    presets_[slot] = std::nullopt;
    
    auto core = getCore();
    if (core) {
        core->getLogger()->info("Deleted preset {}", slot);
    }
    return true;
}

std::vector<int> PresetManager::getUsedSlots() const {
    std::vector<int> usedSlots;
    for (int i = 0; i < static_cast<int>(presets_.size()); ++i) {
        if (presets_[i]) {
            usedSlots.push_back(i);
        }
    }
    return usedSlots;
}

int PresetManager::getAvailableSlots() const {
    int available = 0;
    for (const auto& preset : presets_) {
        if (!preset) {
            ++available;
        }
    }
    return available;
}

bool PresetManager::hasPreset(int slot) const {
    if (!isValidSlot(slot)) {
        return false;
    }
    return presets_[slot].has_value();
}

bool PresetManager::saveCurrentPosition(int slot) {
    auto core = getCore();
    if (!core || !isValidSlot(slot)) {
        if (core) {
            core->getLogger()->error("Invalid preset slot: {}", slot);
        }
        return false;
    }
    
    int currentPosition = core->getCurrentPosition();
    return savePreset(slot, currentPosition);
}

std::optional<int> PresetManager::findNearestPreset(int position, int tolerance) const {
    int nearestSlot = -1;
    int minDistance = tolerance + 1; // Start with distance larger than tolerance
    
    for (int i = 0; i < static_cast<int>(presets_.size()); ++i) {
        if (presets_[i]) {
            int distance = std::abs(*presets_[i] - position);
            if (distance <= tolerance && distance < minDistance) {
                minDistance = distance;
                nearestSlot = i;
            }
        }
    }
    
    if (nearestSlot >= 0) {
        return nearestSlot;
    }
    return std::nullopt;
}

bool PresetManager::isValidSlot(int slot) const {
    return slot >= 0 && slot < static_cast<int>(presets_.size());
}

}  // namespace lithium::device::indi::focuser
