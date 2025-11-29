#ifndef LITHIUM_CLIENT_INDI_CAMERA_HPP
#define LITHIUM_CLIENT_INDI_CAMERA_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <optional>
#include <string>

#include "device/template/camera.hpp"

enum class ImageFormat { FITS, NATIVE, XISF, NONE };

enum class CameraState {
    IDLE,
    EXPOSING,
    DOWNLOADING,
    IDLE_DOWNLOADING,
    ABORTED,
    ERROR,
    UNKNOWN
};

class INDICamera : public INDI::BaseClient, public AtomCamera {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;  // 定义命名常量

    explicit INDICamera(std::string name);
    ~INDICamera() override = default;

    // AtomDriver接口
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& port, int timeout = DEFAULT_TIMEOUT_MS,
                 int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    [[nodiscard]] auto isConnected() const -> bool override;
    auto scan() -> std::vector<std::string> override;

    // 曝光控制
    auto startExposure(double duration) -> bool override;
    auto abortExposure() -> bool override;
    [[nodiscard]] auto isExposing() const -> bool override;
    auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> override;
    auto saveImage(const std::string& path) -> bool override;

    // 视频控制
    auto startVideo() -> bool override;
    auto stopVideo() -> bool override;
    [[nodiscard]] auto isVideoRunning() const -> bool override;
    auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame> override;

    // 温度控制
    auto startCooling(double targetTemp) -> bool override;
    auto stopCooling() -> bool override;
    [[nodiscard]] auto isCoolerOn() const -> bool override;
    [[nodiscard]] auto getTemperature() const -> std::optional<double> override;
    [[nodiscard]] auto getCoolingPower() const
        -> std::optional<double> override;
    [[nodiscard]] auto hasCooler() const -> bool override;

    // 参数控制
    auto setGain(int gain) -> bool override;
    [[nodiscard]] auto getGain() -> std::optional<int> override;
    auto setOffset(int offset) -> bool override;
    [[nodiscard]] auto getOffset() -> std::optional<int> override;
    auto setISO(int iso) -> bool override;
    [[nodiscard]] auto getISO() -> std::optional<int> override;

    // 帧设置
    auto setResolution(int posX, int posY, int width, int height)
        -> bool override;
    [[nodiscard]] auto getResolution()
        -> std::optional<AtomCameraFrame::Resolution> override;
    auto setBinning(int horizontal, int vertical) -> bool override;
    auto setFrameType(FrameType type) -> bool override;
    auto setUploadMode(UploadMode mode) -> bool override;
    [[nodiscard]] auto getFrameInfo() const -> AtomCameraFrame override;

    [[nodiscard]] auto isColor() const -> bool override;

    // INDI特有接口
    auto watchAdditionalProperty() -> bool;
    auto getDeviceInstance() -> INDI::BaseDevice&;
    void setPropertyNumber(std::string_view propertyName, double value);

protected:
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;

private:
    std::string name_;
    std::string deviceName_;

    std::string driverExec_;
    std::string driverVersion_;
    std::string driverInterface_;

    std::atomic<double> currentPollingPeriod_;

    std::atomic_bool isDebug_;

    std::atomic_bool isConnected_;

    std::atomic<double> currentExposure_;
    std::atomic_bool isExposing_;

    bool isCoolingEnable_;
    std::atomic_bool isCooling_;
    std::atomic<double> currentTemperature_;
    double maxTemperature_;
    double minTemperature_;
    std::atomic<double> currentSlope_;
    std::atomic<double> currentThreshold_;

    std::atomic<double> currentGain_;
    double maxGain_;
    double minGain_;

    std::atomic<double> currentOffset_;
    double maxOffset_;
    double minOffset_;

    double frameX_;
    double frameY_;
    double frameWidth_;
    double frameHeight_;
    double maxFrameX_;
    double maxFrameY_;

    double framePixel_;
    double framePixelX_;
    double framePixelY_;

    double frameDepth_;

    double binHor_;
    double binVer_;
    double maxBinHor_;
    double maxBinVer_;

    ImageFormat imageFormat_;

    INDI::BaseDevice device_;
    // Max: 相关的设备，也进行处理，可以联合操作
    INDI::BaseDevice telescope_;
    INDI::BaseDevice focuser_;
    INDI::BaseDevice rotator_;
    INDI::BaseDevice filterwheel_;
};

#endif
