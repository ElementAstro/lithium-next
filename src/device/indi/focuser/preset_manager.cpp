#include "preset_manager.hpp"

#include <algorithm>
#include <cmath>

namespace lithium::device::indi::focuser {

bool PresetManager::initialize(FocuserState& state) {
    state_ = &state;
    state_->logger_->info("{}: Initializing preset manager",
                          getComponentName());
    return true;
}

void PresetManager::cleanup() {
    if (state_) {
        state_->logger_->info("{}: Cleaning up preset manager",
                              getComponentName());
    }
    state_ = nullptr;
}

bool PresetManager::savePreset(int slot, int position) {
    if (!state_ || !isValidSlot(slot)) {
        if (state_) {
            state_->logger_->error("Invalid preset slot: {}", slot);
        }
        return false;
    }
    state_->presets_[slot] = position;
    state_->logger_->info("Saved preset {} with position {}", slot, position);
    return true;
}

bool PresetManager::loadPreset(int slot) {
    if (!state_ || !isValidSlot(slot)) {
        if (state_) {
            state_->logger_->error("Invalid preset slot: {}", slot);
        }
        return false;
    }
    if (!state_->presets_[slot]) {
        state_->logger_->error("Preset slot {} is empty", slot);
        return false;
    }
    int position = *state_->presets_[slot];
    state_->logger_->info("Loading preset {} with position {}", slot, position);
    // Note: Actual movement would be handled by MovementController
    // This just provides the position to move to
    return true;
}

std::optional<int> PresetManager::getPreset(int slot) const {
    if (!state_ || !isValidSlot(slot)) {
        return std::nullopt;
    }
    return state_->presets_[slot];
}

bool PresetManager::deletePreset(int slot) {
    if (!state_ || !isValidSlot(slot)) {
        if (state_) {
            state_->logger_->error("Invalid preset slot: {}", slot);
        }
        return false;
    }
    state_->presets_[slot].reset();
    state_->logger_->info("Deleted preset {}", slot);
    return true;
}

std::vector<int> PresetManager::getUsedSlots() const {
    std::vector<int> usedSlots;
    if (!state_) {
        return usedSlots;
    }
    for (int i = 0; i < static_cast<int>(state_->presets_.size()); ++i) {
        if (state_->presets_[i]) {
            usedSlots.push_back(i);
        }
    }
    return usedSlots;
}

int PresetManager::getAvailableSlots() const {
    if (!state_) {
        return 0;
    }
    return static_cast<int>(std::ranges::count_if(
        state_->presets_, [](const auto& preset) { return !preset; }));
}

bool PresetManager::hasPreset(int slot) const {
    return state_ && isValidSlot(slot) && state_->presets_[slot];
}

bool PresetManager::saveCurrentPosition(int slot) {
    if (!state_) {
        return false;
    }
    int currentPosition = state_->currentPosition_.load();
    return savePreset(slot, currentPosition);
}

std::optional<int> PresetManager::findNearestPreset(int position,
                                                    int tolerance) const {
    if (!state_) {
        return std::nullopt;
    }
    int nearestSlot = -1;
    int minDistance = tolerance + 1;
    for (int i = 0; i < static_cast<int>(state_->presets_.size()); ++i) {
        if (state_->presets_[i]) {
            int distance = std::abs(*state_->presets_[i] - position);
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
    return state_ && slot >= 0 &&
           slot < static_cast<int>(state_->presets_.size());
}

}  // namespace lithium::device::indi::focuser
