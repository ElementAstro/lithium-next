#include "hardware_controller.hpp"
#include "../core/indi_camera_core.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::indi::camera {

HardwareController::HardwareController(std::shared_ptr<INDICameraCore> core)
    : ComponentBase(core) {
    spdlog::debug("Creating hardware controller");
    initializeDefaults();
}

auto HardwareController::initialize() -> bool {
    spdlog::debug("Initializing hardware controller");
    initializeDefaults();
    return true;
}

auto HardwareController::destroy() -> bool {
    spdlog::debug("Destroying hardware controller");
    return true;
}

auto HardwareController::getComponentName() const -> std::string {
    return "HardwareController";
}

auto HardwareController::handleProperty(INDI::Property property) -> bool {
    if (!property.isValid()) {
        return false;
    }

    std::string propertyName = property.getName();

    if (propertyName == "CCD_GAIN") {
        handleGainProperty(property);
        return true;
    } else if (propertyName == "CCD_OFFSET") {
        handleOffsetProperty(property);
        return true;
    } else if (propertyName == "CCD_FRAME") {
        handleFrameProperty(property);
        return true;
    } else if (propertyName == "CCD_BINNING") {
        handleBinningProperty(property);
        return true;
    } else if (propertyName == "CCD_INFO") {
        handleInfoProperty(property);
        return true;
    } else if (propertyName == "CCD_FRAME_TYPE") {
        handleFrameTypeProperty(property);
        return true;
    } else if (propertyName == "CCD_SHUTTER") {
        handleShutterProperty(property);
        return true;
    } else if (propertyName == "CCD_FAN") {
        handleFanProperty(property);
        return true;
    }

    return false;
}

// Gain control
auto HardwareController::setGain(int gain) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertyNumber ccdGain = device.getProperty("CCD_GAIN");
        if (!ccdGain.isValid()) {
            spdlog::error("CCD_GAIN property not found");
            return false;
        }

        int minGain = static_cast<int>(minGain_.load());
        int maxGain = static_cast<int>(maxGain_.load());

        if (gain < minGain || gain > maxGain) {
            spdlog::error("Gain {} out of range [{}, {}]", gain, minGain, maxGain);
            return false;
        }

        spdlog::info("Setting gain to {}...", gain);
        ccdGain[0].setValue(gain);
        getCore()->sendNewProperty(ccdGain);
        currentGain_.store(gain);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set gain: {}", e.what());
        return false;
    }
}

auto HardwareController::getGain() -> std::optional<int> {
    if (!getCore()->isConnected()) {
        return std::nullopt;
    }
    return currentGain_.load();
}

auto HardwareController::getGainRange() -> std::pair<int, int> {
    return {minGain_.load(), maxGain_.load()};
}

// Offset control
auto HardwareController::setOffset(int offset) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertyNumber ccdOffset = device.getProperty("CCD_OFFSET");
        if (!ccdOffset.isValid()) {
            spdlog::error("CCD_OFFSET property not found");
            return false;
        }

        int minOffset = minOffset_.load();
        int maxOffset = maxOffset_.load();

        if (offset < minOffset || offset > maxOffset) {
            spdlog::error("Offset {} out of range [{}, {}]", offset, minOffset, maxOffset);
            return false;
        }

        spdlog::info("Setting offset to {}...", offset);
        ccdOffset[0].setValue(offset);
        getCore()->sendNewProperty(ccdOffset);
        currentOffset_.store(offset);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set offset: {}", e.what());
        return false;
    }
}

auto HardwareController::getOffset() -> std::optional<int> {
    if (!getCore()->isConnected()) {
        return std::nullopt;
    }
    return currentOffset_.load();
}

auto HardwareController::getOffsetRange() -> std::pair<int, int> {
    return {minOffset_.load(), maxOffset_.load()};
}

// ISO control
auto HardwareController::setISO(int iso) -> bool {
    // INDI typically doesn't support ISO settings directly
    spdlog::warn("ISO setting not supported in INDI cameras");
    return false;
}

auto HardwareController::getISO() -> std::optional<int> {
    // INDI typically doesn't support ISO
    return std::nullopt;
}

auto HardwareController::getISOList() -> std::vector<int> {
    // INDI typically doesn't support ISO list
    return {};
}

// Frame settings
auto HardwareController::getResolution() -> std::optional<AtomCameraFrame::Resolution> {
    if (!getCore()->isConnected()) {
        return std::nullopt;
    }

    AtomCameraFrame::Resolution res;
    res.width = frameWidth_.load();
    res.height = frameHeight_.load();
    res.maxWidth = maxFrameX_.load();
    res.maxHeight = maxFrameY_.load();

    return res;
}

auto HardwareController::setResolution(int x, int y, int width, int height) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertyNumber ccdFrame = device.getProperty("CCD_FRAME");
        if (!ccdFrame.isValid()) {
            spdlog::error("CCD_FRAME property not found");
            return false;
        }

        spdlog::info("Setting frame to [{}, {}, {}, {}]", x, y, width, height);
        ccdFrame[0].setValue(x);       // X
        ccdFrame[1].setValue(y);       // Y
        ccdFrame[2].setValue(width);   // Width
        ccdFrame[3].setValue(height);  // Height
        getCore()->sendNewProperty(ccdFrame);

        frameX_.store(x);
        frameY_.store(y);
        frameWidth_.store(width);
        frameHeight_.store(height);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set resolution: {}", e.what());
        return false;
    }
}

auto HardwareController::getMaxResolution() -> AtomCameraFrame::Resolution {
    AtomCameraFrame::Resolution res;
    res.maxWidth = maxFrameX_.load();
    res.maxHeight = maxFrameY_.load();
    res.width = maxFrameX_.load();
    res.height = maxFrameY_.load();
    return res;
}

// Binning control
auto HardwareController::getBinning() -> std::optional<AtomCameraFrame::Binning> {
    if (!getCore()->isConnected()) {
        return std::nullopt;
    }

    AtomCameraFrame::Binning bin;
    bin.horizontal = binHor_.load();
    bin.vertical = binVer_.load();
    return bin;
}

auto HardwareController::setBinning(int horizontal, int vertical) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertyNumber ccdBinning = device.getProperty("CCD_BINNING");
        if (!ccdBinning.isValid()) {
            spdlog::error("CCD_BINNING property not found");
            return false;
        }

        int maxHor = maxBinHor_.load();
        int maxVer = maxBinVer_.load();

        if (horizontal > maxHor || vertical > maxVer) {
            spdlog::error("Binning [{}, {}] exceeds maximum [{}, {}]",
                         horizontal, vertical, maxHor, maxVer);
            return false;
        }

        spdlog::info("Setting binning to [{}, {}]", horizontal, vertical);
        ccdBinning[0].setValue(horizontal);
        ccdBinning[1].setValue(vertical);
        getCore()->sendNewProperty(ccdBinning);

        binHor_.store(horizontal);
        binVer_.store(vertical);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set binning: {}", e.what());
        return false;
    }
}

auto HardwareController::getMaxBinning() -> AtomCameraFrame::Binning {
    AtomCameraFrame::Binning bin;
    bin.horizontal = maxBinHor_.load();
    bin.vertical = maxBinVer_.load();
    return bin;
}

// Frame type control
auto HardwareController::setFrameType(FrameType type) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch ccdFrameType = device.getProperty("CCD_FRAME_TYPE");
        if (!ccdFrameType.isValid()) {
            spdlog::error("CCD_FRAME_TYPE property not found");
            return false;
        }

        // Reset all switches
        for (int i = 0; i < ccdFrameType.size(); i++) {
            ccdFrameType[i].setState(ISS_OFF);
        }

        // Set the appropriate switch based on frame type
        switch (type) {
            case FrameType::FITS:
                if (ccdFrameType.size() > 0) ccdFrameType[0].setState(ISS_ON);
                break;
            case FrameType::NATIVE:
                if (ccdFrameType.size() > 1) ccdFrameType[1].setState(ISS_ON);
                break;
            case FrameType::XISF:
                if (ccdFrameType.size() > 2) ccdFrameType[2].setState(ISS_ON);
                break;
            case FrameType::JPG:
                if (ccdFrameType.size() > 3) ccdFrameType[3].setState(ISS_ON);
                break;
            case FrameType::PNG:
                if (ccdFrameType.size() > 4) ccdFrameType[4].setState(ISS_ON);
                break;
            case FrameType::TIFF:
                if (ccdFrameType.size() > 5) ccdFrameType[5].setState(ISS_ON);
                break;
        }

        getCore()->sendNewProperty(ccdFrameType);
        currentFrameType_ = type;

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set frame type: {}", e.what());
        return false;
    }
}

auto HardwareController::getFrameType() -> FrameType {
    return currentFrameType_;
}

auto HardwareController::setUploadMode(UploadMode mode) -> bool {
    currentUploadMode_ = mode;
    // INDI upload mode typically controlled through UPLOAD_MODE property
    return true;
}

auto HardwareController::getUploadMode() -> UploadMode {
    return currentUploadMode_;
}

// Pixel information
auto HardwareController::getPixelSize() -> double {
    return framePixel_.load();
}

auto HardwareController::getPixelSizeX() -> double {
    return framePixelX_.load();
}

auto HardwareController::getPixelSizeY() -> double {
    return framePixelY_.load();
}

auto HardwareController::getBitDepth() -> int {
    return frameDepth_.load();
}

// Shutter control
auto HardwareController::hasShutter() -> bool {
    if (!getCore()->isConnected()) {
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch shutterControl = device.getProperty("CCD_SHUTTER");
        return shutterControl.isValid();
    } catch (const std::exception& e) {
        return false;
    }
}

auto HardwareController::setShutter(bool open) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch shutterControl = device.getProperty("CCD_SHUTTER");
        if (!shutterControl.isValid()) {
            spdlog::error("CCD_SHUTTER property not found");
            return false;
        }

        if (open) {
            shutterControl[0].setState(ISS_ON);  // OPEN
            shutterControl[1].setState(ISS_OFF); // CLOSE
        } else {
            shutterControl[0].setState(ISS_OFF); // OPEN
            shutterControl[1].setState(ISS_ON);  // CLOSE
        }

        getCore()->sendNewProperty(shutterControl);
        shutterOpen_.store(open);

        spdlog::info("Shutter {}", open ? "opened" : "closed");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to control shutter: {}", e.what());
        return false;
    }
}

auto HardwareController::getShutterStatus() -> bool {
    return shutterOpen_.load();
}

// Fan control
auto HardwareController::hasFan() -> bool {
    if (!getCore()->isConnected()) {
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertyNumber fanControl = device.getProperty("CCD_FAN");
        return fanControl.isValid();
    } catch (const std::exception& e) {
        return false;
    }
}

auto HardwareController::setFanSpeed(int speed) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertyNumber fanControl = device.getProperty("CCD_FAN");
        if (!fanControl.isValid()) {
            spdlog::error("CCD_FAN property not found");
            return false;
        }

        spdlog::info("Setting fan speed to {}", speed);
        fanControl[0].setValue(speed);
        getCore()->sendNewProperty(fanControl);
        fanSpeed_.store(speed);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set fan speed: {}", e.what());
        return false;
    }
}

auto HardwareController::getFanSpeed() -> int {
    return fanSpeed_.load();
}

// Color and Bayer
auto HardwareController::isColor() const -> bool {
    return bayerPattern_ != BayerPattern::MONO;
}

auto HardwareController::getBayerPattern() const -> BayerPattern {
    return bayerPattern_;
}

auto HardwareController::setBayerPattern(BayerPattern pattern) -> bool {
    bayerPattern_ = pattern;
    return true;
}

// Frame info
auto HardwareController::getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> {
    auto frame = std::make_shared<AtomCameraFrame>();

    frame->resolution.width = frameWidth_.load();
    frame->resolution.height = frameHeight_.load();
    frame->resolution.maxWidth = maxFrameX_.load();
    frame->resolution.maxHeight = maxFrameY_.load();

    frame->binning.horizontal = binHor_.load();
    frame->binning.vertical = binVer_.load();

    frame->pixel.size = framePixel_.load();
    frame->pixel.sizeX = framePixelX_.load();
    frame->pixel.sizeY = framePixelY_.load();
    frame->pixel.depth = frameDepth_.load();

    return frame;
}

// Private methods - Property handlers
void HardwareController::handleGainProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }

    INDI::PropertyNumber gainProperty = property;
    if (!gainProperty.isValid()) {
        return;
    }

    if (gainProperty.size() > 0) {
        currentGain_.store(static_cast<int>(gainProperty[0].getValue()));
        minGain_.store(static_cast<int>(gainProperty[0].getMin()));
        maxGain_.store(static_cast<int>(gainProperty[0].getMax()));
    }
}

void HardwareController::handleOffsetProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }

    INDI::PropertyNumber offsetProperty = property;
    if (!offsetProperty.isValid()) {
        return;
    }

    if (offsetProperty.size() > 0) {
        currentOffset_.store(static_cast<int>(offsetProperty[0].getValue()));
        minOffset_.store(static_cast<int>(offsetProperty[0].getMin()));
        maxOffset_.store(static_cast<int>(offsetProperty[0].getMax()));
    }
}

void HardwareController::handleFrameProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }

    INDI::PropertyNumber frameProperty = property;
    if (!frameProperty.isValid() || frameProperty.size() < 4) {
        return;
    }

    frameX_.store(static_cast<int>(frameProperty[0].getValue()));
    frameY_.store(static_cast<int>(frameProperty[1].getValue()));
    frameWidth_.store(static_cast<int>(frameProperty[2].getValue()));
    frameHeight_.store(static_cast<int>(frameProperty[3].getValue()));
}

void HardwareController::handleBinningProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }

    INDI::PropertyNumber binProperty = property;
    if (!binProperty.isValid() || binProperty.size() < 2) {
        return;
    }

    binHor_.store(static_cast<int>(binProperty[0].getValue()));
    binVer_.store(static_cast<int>(binProperty[1].getValue()));
    maxBinHor_.store(static_cast<int>(binProperty[0].getMax()));
    maxBinVer_.store(static_cast<int>(binProperty[1].getMax()));
}

void HardwareController::handleInfoProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }

    INDI::PropertyNumber infoProperty = property;
    if (!infoProperty.isValid()) {
        return;
    }

    // CCD_INFO typically contains: MaxX, MaxY, PixelSize, PixelSizeX, PixelSizeY, BitDepth
    if (infoProperty.size() >= 6) {
        maxFrameX_.store(static_cast<int>(infoProperty[0].getValue()));
        maxFrameY_.store(static_cast<int>(infoProperty[1].getValue()));
        framePixel_.store(infoProperty[2].getValue());
        framePixelX_.store(infoProperty[3].getValue());
        framePixelY_.store(infoProperty[4].getValue());
        frameDepth_.store(static_cast<int>(infoProperty[5].getValue()));
    }
}

void HardwareController::handleFrameTypeProperty(INDI::Property property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }

    INDI::PropertySwitch frameTypeProperty = property;
    if (!frameTypeProperty.isValid()) {
        return;
    }

    // Find which frame type is selected
    for (int i = 0; i < frameTypeProperty.size(); i++) {
        if (frameTypeProperty[i].getState() == ISS_ON) {
            currentFrameType_ = static_cast<FrameType>(i);
            break;
        }
    }
}

void HardwareController::handleShutterProperty(INDI::Property property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }

    INDI::PropertySwitch shutterProperty = property;
    if (!shutterProperty.isValid() || shutterProperty.size() < 2) {
        return;
    }

    // Typically: OPEN=0, CLOSE=1
    shutterOpen_.store(shutterProperty[0].getState() == ISS_ON);
}

void HardwareController::handleFanProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }

    INDI::PropertyNumber fanProperty = property;
    if (!fanProperty.isValid()) {
        return;
    }

    if (fanProperty.size() > 0) {
        fanSpeed_.store(static_cast<int>(fanProperty[0].getValue()));
    }
}

void HardwareController::initializeDefaults() {
    // Initialize default values
    currentGain_.store(0);
    minGain_.store(0);
    maxGain_.store(100);

    currentOffset_.store(0);
    minOffset_.store(0);
    maxOffset_.store(100);

    frameX_.store(0);
    frameY_.store(0);
    frameWidth_.store(0);
    frameHeight_.store(0);
    maxFrameX_.store(0);
    maxFrameY_.store(0);

    framePixel_.store(0.0);
    framePixelX_.store(0.0);
    framePixelY_.store(0.0);
    frameDepth_.store(16);

    binHor_.store(1);
    binVer_.store(1);
    maxBinHor_.store(1);
    maxBinVer_.store(1);

    shutterOpen_.store(true);
    fanSpeed_.store(0);

    currentFrameType_ = FrameType::FITS;
    currentUploadMode_ = UploadMode::CLIENT;
    bayerPattern_ = BayerPattern::MONO;
}

} // namespace lithium::device::indi::camera
