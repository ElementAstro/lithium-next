#ifndef LITHIUM_DEVICE_FOCUSER_EAF_HPP
#define LITHIUM_DEVICE_FOCUSER_EAF_HPP

#include <libasi/EAF_focuser.h>
#include <mutex>
#include <vector>
#include "device/template/focuser.hpp"

// Device properties structure
struct DeviceProperties {
    EAF_INFO info;
    bool isReversed;
    int backlash;
    float temperature;
    bool beepEnabled;
    unsigned char firmwareMajor;
    unsigned char firmwareMinor;
    unsigned char firmwareBuild;
    EAF_SN serialNumber;
};

class EAFController : public AtomFocuser {
private:
    int m_deviceID;
    bool m_isOpen;
    mutable DeviceProperties m_properties;  // 重命名
    mutable std::mutex m_propMutex;
    double m_speed;
    int m_maxLimit;
    bool m_isConnected;  // 添加连接状态标志

public:
    explicit EAFController(const std::string& name);
    ~EAFController() override;

    // 添加移动构造函数和移动赋值运算符声明
    EAFController(EAFController&& other) noexcept;
    EAFController& operator=(EAFController&& other) noexcept;

    // 禁用拷贝构造和拷贝赋值
    EAFController(const EAFController&) = delete;
    EAFController& operator=(const EAFController&) = delete;

    // AtomFocuser interface implementation
    std::optional<double> getSpeed() override;
    bool setSpeed(double speed) override;
    std::optional<FocusDirection> getDirection() override;
    bool setDirection(FocusDirection direction) override;
    std::optional<int> getMaxLimit() override;
    bool setMaxLimit(int maxLimit) override;
    std::optional<bool> isReversed() override;
    bool setReversed(bool reversed) override;
    bool moveSteps(int steps) override;
    bool moveToPosition(int position) override;
    std::optional<int> getPosition() override;
    bool moveForDuration(int durationMs) override;
    bool abortMove() override;
    bool syncPosition(int position) override;
    std::optional<double> getExternalTemperature() override;
    std::optional<double> getChipTemperature() override;

    // 添加新的接口实现
    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& port, int timeout = 5000,
                 int maxRetry = 3) override;
    bool disconnect() override;
    bool isConnected() const override;
    std::vector<std::string> scan() override;

    // EAF specific methods
    bool Open();
    void Close();
    bool Move(int position, bool waitComplete);
    bool Stop();
    bool UpdateProperties() const;
    const DeviceProperties& GetDeviceProperties() const;
};

#endif  // LITHIUM_DEVICE_FOCUSER_EAF_HPP
