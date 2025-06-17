#ifndef LITHIUM_INDI_CAMERA_HARDWARE_CONTROLLER_HPP
#define LITHIUM_INDI_CAMERA_HARDWARE_CONTROLLER_HPP

#include "../component_base.hpp"
#include "../../../template/camera.hpp"

#include <atomic>
#include <optional>
#include <utility>
#include <vector>

namespace lithium::device::indi::camera {

/**
 * @brief Hardware control component for INDI cameras
 * 
 * This component handles hardware-specific controls including
 * shutter, fan, gain, offset, ISO, and frame settings.
 */
class HardwareController : public ComponentBase {
public:
    explicit HardwareController(INDICameraCore* core);
    ~HardwareController() override = default;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto handleProperty(INDI::Property property) -> bool override;

    // Gain control
    auto setGain(int gain) -> bool;
    auto getGain() -> std::optional<int>;
    auto getGainRange() -> std::pair<int, int>;

    // Offset control
    auto setOffset(int offset) -> bool;
    auto getOffset() -> std::optional<int>;
    auto getOffsetRange() -> std::pair<int, int>;

    // ISO control
    auto setISO(int iso) -> bool;
    auto getISO() -> std::optional<int>;
    auto getISOList() -> std::vector<int>;

    // Frame settings
    auto getResolution() -> std::optional<AtomCameraFrame::Resolution>;
    auto setResolution(int x, int y, int width, int height) -> bool;
    auto getMaxResolution() -> AtomCameraFrame::Resolution;

    // Binning control
    auto getBinning() -> std::optional<AtomCameraFrame::Binning>;
    auto setBinning(int horizontal, int vertical) -> bool;
    auto getMaxBinning() -> AtomCameraFrame::Binning;

    // Frame type control
    auto setFrameType(FrameType type) -> bool;
    auto getFrameType() -> FrameType;
    auto setUploadMode(UploadMode mode) -> bool;
    auto getUploadMode() -> UploadMode;

    // Pixel information
    auto getPixelSize() -> double;
    auto getPixelSizeX() -> double;
    auto getPixelSizeY() -> double;
    auto getBitDepth() -> int;

    // Shutter control
    auto hasShutter() -> bool;
    auto setShutter(bool open) -> bool;
    auto getShutterStatus() -> bool;

    // Fan control
    auto hasFan() -> bool;
    auto setFanSpeed(int speed) -> bool;
    auto getFanSpeed() -> int;

    // Color and Bayer
    auto isColor() const -> bool;
    auto getBayerPattern() const -> BayerPattern;
    auto setBayerPattern(BayerPattern pattern) -> bool;

    // Frame info
    auto getFrameInfo() const -> std::shared_ptr<AtomCameraFrame>;

private:
    // Gain and offset
    std::atomic<int> currentGain_{0};
    std::atomic<int> maxGain_{100};
    std::atomic<int> minGain_{0};
    std::atomic<int> currentOffset_{0};
    std::atomic<int> maxOffset_{100};
    std::atomic<int> minOffset_{0};

    // Frame parameters
    std::atomic<int> frameX_{0};
    std::atomic<int> frameY_{0};
    std::atomic<int> frameWidth_{0};
    std::atomic<int> frameHeight_{0};
    std::atomic<int> maxFrameX_{0};
    std::atomic<int> maxFrameY_{0};
    std::atomic<double> framePixel_{0.0};
    std::atomic<double> framePixelX_{0.0};
    std::atomic<double> framePixelY_{0.0};
    std::atomic<int> frameDepth_{16};

    // Binning parameters
    std::atomic<int> binHor_{1};
    std::atomic<int> binVer_{1};
    std::atomic<int> maxBinHor_{1};
    std::atomic<int> maxBinVer_{1};

    // Shutter and fan control
    std::atomic_bool shutterOpen_{true};
    std::atomic<int> fanSpeed_{0};

    // Frame type and upload mode
    FrameType currentFrameType_{FrameType::FITS};
    UploadMode currentUploadMode_{UploadMode::CLIENT};

    // Bayer pattern
    BayerPattern bayerPattern_{BayerPattern::MONO};

    // Property handlers
    void handleGainProperty(INDI::Property property);
    void handleOffsetProperty(INDI::Property property);
    void handleFrameProperty(INDI::Property property);
    void handleBinningProperty(INDI::Property property);
    void handleInfoProperty(INDI::Property property);
    void handleFrameTypeProperty(INDI::Property property);
    void handleShutterProperty(INDI::Property property);
    void handleFanProperty(INDI::Property property);

    // Helper methods
    void updateFrameInfo();
    void initializeDefaults();
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_HARDWARE_CONTROLLER_HPP
