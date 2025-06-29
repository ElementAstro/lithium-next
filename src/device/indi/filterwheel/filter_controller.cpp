#include "filter_controller.hpp"

namespace lithium::device::indi::filterwheel {

FilterController::FilterController(std::shared_ptr<INDIFilterWheelCore> core)
    : FilterWheelComponentBase(std::move(core)) {}

bool FilterController::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    core->getLogger()->info("Initializing FilterController");
    initialized_ = true;
    return true;
}

void FilterController::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("Shutting down FilterController");
    }
    initialized_ = false;
}

bool FilterController::setPosition(int position) {
    auto core = getCore();
    if (!core || !validateComponentReady()) {
        if (core) {
            core->getLogger()->error("FilterController not ready for position change");
        }
        return false;
    }

    if (!isValidPosition(position)) {
        core->getLogger()->error("Invalid filter position: {}", position);
        return false;
    }

    if (core->isMoving()) {
        core->getLogger()->warn("Filter wheel is already moving");
        return false;
    }

    core->getLogger()->info("Setting filter position to: {}", position);
    recordMoveStart();

    bool success = sendFilterChangeCommand(position);
    if (!success) {
        core->getLogger()->error("Failed to send filter change command");
        return false;
    }

    return true;
}

std::optional<int> FilterController::getPosition() const {
    auto core = getCore();
    if (!core) {
        return std::nullopt;
    }

    return core->getCurrentSlot();
}

bool FilterController::isMoving() const {
    auto core = getCore();
    if (!core) {
        return false;
    }

    return core->isMoving();
}

bool FilterController::abortMove() {
    auto core = getCore();
    if (!core || !validateComponentReady()) {
        return false;
    }

    core->getLogger()->info("Aborting filter wheel movement");

    // Try to send abort command if available
    auto& device = core->getDevice();
    INDI::PropertySwitch abortProp = device.getProperty("FILTER_ABORT");
    if (!abortProp.isValid()) {
        core->getLogger()->warn("No abort command available for this filter wheel");
        return false;
    }

    if (abortProp.count() > 0) {
        abortProp[0].setState(ISS_ON);
        core->getClient()->sendNewProperty(abortProp);
        return true;
    }

    core->getLogger()->warn("No abort command available for this filter wheel");
    return false;
}

int FilterController::getMaxPosition() const {
    auto core = getCore();
    if (!core) {
        return 0;
    }

    return core->getMaxSlot();
}

int FilterController::getMinPosition() const {
    auto core = getCore();
    if (!core) {
        return 1;
    }

    return core->getMinSlot();
}

std::vector<std::string> FilterController::getFilterNames() const {
    auto core = getCore();
    if (!core) {
        return {};
    }

    return core->getSlotNames();
}

std::optional<std::string> FilterController::getFilterName(int position) const {
    auto core = getCore();
    if (!core || !isValidPosition(position)) {
        return std::nullopt;
    }

    const auto& names = core->getSlotNames();
    if (position > 0 && position <= static_cast<int>(names.size())) {
        return names[position - 1];
    }

    return std::nullopt;
}

bool FilterController::setFilterName(int position, const std::string& name) {
    auto core = getCore();
    if (!core || !validateComponentReady()) {
        return false;
    }

    if (!isValidPosition(position)) {
        core->getLogger()->error("Invalid filter position for name change: {}", position);
        return false;
    }

    auto& device = core->getDevice();
    INDI::PropertyText nameProp = device.getProperty("FILTER_NAME");
    if (!nameProp.isValid()) {
        core->getLogger()->error("FILTER_NAME property not available");
        return false;
    }

    // Find the text widget for this position
    std::string widgetName = "FILTER_SLOT_NAME_" + std::to_string(position);
    for (int i = 0; i < nameProp.count(); ++i) {
        if (std::string(nameProp[i].getName()) == widgetName) {
            nameProp[i].setText(name.c_str());
            core->getClient()->sendNewProperty(nameProp);

            // Update local state
            auto names = core->getSlotNames();
            if (position > 0 && position <= static_cast<int>(names.size())) {
                names[position - 1] = name;
                core->setSlotNames(names);
            }

            core->getLogger()->info("Filter {} name set to: {}", position, name);
            return true;
        }
    }

    core->getLogger()->error("Could not find name widget for position {}", position);
    return false;
}

bool FilterController::isValidPosition(int position) const {
    auto core = getCore();
    if (!core) {
        return false;
    }

    return position >= core->getMinSlot() && position <= core->getMaxSlot();
}

std::chrono::milliseconds FilterController::getLastMoveDuration() const {
    return lastMoveDuration_;
}

bool FilterController::sendFilterChangeCommand(int position) {
    auto core = getCore();
    if (!core) {
        return false;
    }

    auto& device = core->getDevice();
    INDI::PropertyNumber slotProp = device.getProperty("FILTER_SLOT");
    if (!slotProp.isValid()) {
        core->getLogger()->error("FILTER_SLOT property not available");
        return false;
    }

    if (slotProp.count() > 0) {
        slotProp[0].setValue(position);
        core->getClient()->sendNewProperty(slotProp);
        core->setMoving(true);

        core->getLogger()->debug("Sent filter change command: position {}", position);
        return true;
    }

    core->getLogger()->error("FILTER_SLOT property has no elements");
    return false;
}

void FilterController::recordMoveStart() {
    moveStartTime_ = std::chrono::steady_clock::now();
}

void FilterController::recordMoveEnd() {
    auto now = std::chrono::steady_clock::now();
    lastMoveDuration_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - moveStartTime_);
}

}  // namespace lithium::device::indi::filterwheel
