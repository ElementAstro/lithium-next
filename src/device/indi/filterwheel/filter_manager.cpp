/*
 * filter_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Filter management operations implementation

*************************************************/

#include "filter_manager.hpp"

INDIFilterwheelFilterManager::INDIFilterwheelFilterManager(std::string name)
    : INDIFilterwheelBase(name) {
}

auto INDIFilterwheelFilterManager::getSlotName(int slot) -> std::optional<std::string> {
    if (!validateSlotIndex(slot)) {
        logger_->error("Invalid slot index: {}", slot);
        return std::nullopt;
    }

    if (slot >= static_cast<int>(slotNames_.size())) {
        logger_->warn("Slot {} not yet populated with name", slot);
        return std::nullopt;
    }

    return slotNames_[slot];
}

auto INDIFilterwheelFilterManager::setSlotName(int slot, const std::string& name) -> bool {
    if (!validateSlotIndex(slot)) {
        logger_->error("Invalid slot index: {}", slot);
        return false;
    }

    INDI::PropertyText property = device_.getProperty("FILTER_NAME");
    if (!property.isValid()) {
        logger_->error("Unable to find FILTER_NAME property");
        return false;
    }

    if (slot >= static_cast<int>(property.size())) {
        logger_->error("Slot {} out of range for property", slot);
        return false;
    }

    logger_->info("Setting slot {} name to: {}", slot, name);

    property[slot].setText(name.c_str());
    sendNewProperty(property);

    // Update local cache
    if (slot < static_cast<int>(slotNames_.size())) {
        slotNames_[slot] = name;
    } else {
        // Expand the vector if necessary
        slotNames_.resize(slot + 1);
        slotNames_[slot] = name;
    }

    notifyFilterChange(slot, name);
    return true;
}

auto INDIFilterwheelFilterManager::getAllSlotNames() -> std::vector<std::string> {
    return slotNames_;
}

auto INDIFilterwheelFilterManager::getCurrentFilterName() -> std::string {
    int currentPos = currentSlot_.load();
    if (currentPos >= 0 && currentPos < static_cast<int>(slotNames_.size())) {
        return slotNames_[currentPos];
    }
    return "Unknown";
}

auto INDIFilterwheelFilterManager::getFilterInfo(int slot) -> std::optional<FilterInfo> {
    if (!validateSlotIndex(slot)) {
        logger_->error("Invalid slot index: {}", slot);
        return std::nullopt;
    }

    if (slot < MAX_FILTERS) {
        FilterInfo info = filters_[slot];

        // If we have a cached name but the filter info name is empty, use the cached name
        if (info.name.empty() && slot < static_cast<int>(slotNames_.size())) {
            info.name = slotNames_[slot];
        }

        // Provide default values if not set
        if (info.type.empty()) {
            info.type = "Unknown";
        }
        if (info.description.empty()) {
            info.description = "Filter at slot " + std::to_string(slot);
        }

        return info;
    }

    return std::nullopt;
}

auto INDIFilterwheelFilterManager::setFilterInfo(int slot, const FilterInfo& info) -> bool {
    if (!validateSlotIndex(slot)) {
        logger_->error("Invalid slot index: {}", slot);
        return false;
    }

    if (slot >= MAX_FILTERS) {
        logger_->error("Slot {} exceeds maximum filter slots", slot);
        return false;
    }

    logger_->info("Setting filter info for slot {}: name={}, type={}",
                  slot, info.name, info.type);

    // Store the filter info
    filters_[slot] = info;

    // Also update the slot name if it's different
    if (slot < static_cast<int>(slotNames_.size()) && slotNames_[slot] != info.name) {
        return setSlotName(slot, info.name);
    }

    return true;
}

auto INDIFilterwheelFilterManager::getAllFilterInfo() -> std::vector<FilterInfo> {
    std::vector<FilterInfo> infos;
    for (int i = 0; i < getFilterCount(); ++i) {
        auto info = getFilterInfo(i);
        if (info) {
            infos.push_back(*info);
        }
    }
    return infos;
}

auto INDIFilterwheelFilterManager::findFilterByName(const std::string& name) -> std::optional<int> {
    for (int i = 0; i < static_cast<int>(slotNames_.size()); ++i) {
        if (slotNames_[i] == name) {
            logger_->debug("Found filter '{}' at slot {}", name, i);
            return i;
        }
    }

    logger_->debug("Filter '{}' not found", name);
    return std::nullopt;
}

auto INDIFilterwheelFilterManager::findFilterByType(const std::string& type) -> std::vector<int> {
    std::vector<int> matches;

    for (int i = 0; i < MAX_FILTERS && i < static_cast<int>(slotNames_.size()); ++i) {
        if (filters_[i].type == type) {
            matches.push_back(i);
        }
    }

    logger_->debug("Found {} filters of type '{}'", matches.size(), type);
    return matches;
}

auto INDIFilterwheelFilterManager::selectFilterByName(const std::string& name) -> bool {
    auto slot = findFilterByName(name);
    if (slot) {
        logger_->info("Selecting filter '{}' at slot {}", name, *slot);
        // Note: This will need to call the control component's setPosition
        // For now, we'll implement a basic version
        currentSlot_ = *slot;
        return true;
    }

    logger_->error("Filter '{}' not found", name);
    return false;
}

auto INDIFilterwheelFilterManager::selectFilterByType(const std::string& type) -> bool {
    auto slots = findFilterByType(type);
    if (!slots.empty()) {
        int selectedSlot = slots[0]; // Select first match
        logger_->info("Selecting first filter of type '{}' at slot {}", type, selectedSlot);
        // Note: This will need to call the control component's setPosition
        // For now, we'll implement a basic version
        currentSlot_ = selectedSlot;
        return true;
    }

    logger_->error("No filter of type '{}' found", type);
    return false;
}

auto INDIFilterwheelFilterManager::validateSlotIndex(int slot) -> bool {
    return slot >= 0 && slot < filterwheel_capabilities_.maxFilters;
}

void INDIFilterwheelFilterManager::updateFilterCache() {
    logger_->debug("Updating filter cache");
    // This method can be called to refresh the local filter cache
    // Implementation depends on specific needs
}

void INDIFilterwheelFilterManager::notifyFilterChange(int slot, const std::string& name) {
    logger_->info("Filter change notification: slot {} -> '{}'", slot, name);

    // If this is the current slot, update the current name
    if (slot == currentSlot_.load()) {
        currentSlotName_ = name;
    }

    // Call position callback if set and this is the current position
    if (position_callback_ && slot == currentSlot_.load()) {
        position_callback_(slot, name);
    }
}
