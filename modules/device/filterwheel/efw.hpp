#pragma once

#include <libasi/EFW_filter.h>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <vector>
#include "device/template/filterwheel.hpp"

// 设备属性缓存
struct DeviceProperties {
    EFW_INFO info;
    bool isUnidirectional;
    unsigned char firmwareMajor;
    unsigned char firmwareMinor;
    unsigned char firmwareBuild;
    EFW_SN serialNumber;
    int lastErrorCode;
};

// ==================== 设备管理核心类 ====================
class EFWController : public AtomFilterWheel {
public:
    EFWController();
    explicit EFWController(int id);
    ~EFWController();

    // 允许移动构造和赋值
    EFWController(EFWController&& other) noexcept;
    EFWController& operator=(EFWController&& other) noexcept;

    // 禁用拷贝
    EFWController(const EFWController&) = delete;
    EFWController& operator=(const EFWController&) = delete;

    bool open();
    bool setPosition(int slot, int maxRetries = 3);
    void close();
    int getPositionValue();
    bool getProperties() const;
    bool calibrate();
    bool setUnidirectional(bool enabled);
    bool setAlias(const EFW_ID& alias);
    const DeviceProperties& getDeviceProperties() const;

    // 实现 AtomFilterWheel 的纯虚函数
    std::optional<std::tuple<double, double, double>> getPosition() override;
    bool setPosition(int position) override;
    std::optional<std::string> getSlotName() override;
    bool setSlotName(std::string_view name) override;

    // 新增虚函数实现
    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& port, int timeout = 5000,
                 int maxRetry = 3) override;
    bool disconnect() override;
    bool isConnected() const override;
    std::vector<std::string> scan() override;

private:
    void handleError(EFW_ERROR_CODE err);

    int deviceID;
    bool isOpen;
    mutable DeviceProperties properties;
    mutable std::mutex propMutex;
    std::vector<std::string> slotNames;  // 内部存储槽位名称

    // 全局互斥锁保证线程安全
    static std::mutex g_efwMutex;
};
