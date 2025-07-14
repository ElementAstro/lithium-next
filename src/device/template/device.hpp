/*
 * device.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Enhanced Device Definition following INDI architecture

*************************************************/

#ifndef ATOM_DRIVER_HPP
#define ATOM_DRIVER_HPP

#include "atom/utils/uuid.hpp"
#include "atom/macro.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Device states following INDI convention
enum class DeviceState {
    IDLE = 0,
    BUSY,
    ALERT,
    ERROR,
    UNKNOWN
};

// Property states
enum class PropertyState {
    IDLE = 0,
    OK,
    BUSY,
    ALERT
};

// Connection types
enum class ConnectionType {
    SERIAL,
    TCP,
    UDP,
    USB,
    ETHERNET,
    BLUETOOTH,
    NONE
};

// Device capabilities
struct DeviceCapabilities {
    bool hasConnection{true};
    bool hasDriverInfo{true};
    bool hasConfigProcess{false};
    bool hasSnoop{false};
    bool hasInterfaceMask{false};
} ATOM_ALIGNAS(8);

// Device information structure
struct DeviceInfo {
    std::string driverName;
    std::string driverExec;
    std::string driverVersion;
    std::string driverInterface;
    std::string manufacturer;
    std::string model;
    std::string serialNumber;
    std::string firmwareVersion;
} ATOM_ALIGNAS(64);

// Property base class for INDI-like properties
class DeviceProperty {
public:
    explicit DeviceProperty(std::string name, std::string label = "")
        : name_(std::move(name)), label_(std::move(label)), state_(PropertyState::IDLE) {}

    virtual ~DeviceProperty() = default;

    const std::string& getName() const { return name_; }
    const std::string& getLabel() const { return label_; }
    PropertyState getState() const { return state_; }
    void setState(PropertyState state) { state_ = state; }
    const std::string& getGroup() const { return group_; }
    void setGroup(const std::string& group) { group_ = group; }

protected:
    std::string name_;
    std::string label_;
    std::string group_;
    PropertyState state_;
};

class AtomDriver {
public:
    explicit AtomDriver(std::string name)
        : name_(std::move(name)),
          uuid_(atom::utils::UUID().toString()),
          state_(DeviceState::UNKNOWN),
          connected_(false),
          simulated_(false) {}

    virtual ~AtomDriver() = default;

    // 核心接口
    virtual bool initialize() = 0;
    virtual bool destroy() = 0;
    virtual bool connect(const std::string &port = "", int timeout = 5000, int maxRetry = 3) = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const { return connected_; }
    virtual std::vector<std::string> scan() = 0;

    // 设备状态管理
    DeviceState getState() const { return state_; }
    void setState(DeviceState state) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = state;
    }

    // 设备信息
    const std::string& getUUID() const { return uuid_; }
    const std::string& getName() const { return name_; }
    void setName(const std::string &newName) { name_ = newName; }
    const std::string& getType() const { return type_; }
    void setType(const std::string& type) { type_ = type; }

    // 设备详细信息
    const DeviceInfo& getDeviceInfo() const { return device_info_; }
    void setDeviceInfo(const DeviceInfo& info) { device_info_ = info; }

    // 能力查询
    const DeviceCapabilities& getCapabilities() const { return capabilities_; }
    void setCapabilities(const DeviceCapabilities& caps) { capabilities_ = caps; }

    // 仿真模式
    bool isSimulated() const { return simulated_; }
    virtual bool setSimulated(bool enabled) { simulated_ = enabled; return true; }

    // 配置管理
    virtual bool loadConfig() { return true; }
    virtual bool saveConfig() { return true; }
    virtual bool resetConfig() { return true; }

    // 属性管理
    void addProperty(std::shared_ptr<DeviceProperty> property);
    std::shared_ptr<DeviceProperty> getProperty(const std::string& name);
    std::vector<std::shared_ptr<DeviceProperty>> getAllProperties();
    bool removeProperty(const std::string& name);

    // 调试和诊断
    virtual std::string getDriverVersion() const { return "1.0.0"; }
    virtual std::string getDriverName() const { return name_; }
    virtual std::string getDriverInfo() const;
    virtual bool runDiagnostics() { return true; }

    // 时间戳
    std::chrono::system_clock::time_point getLastUpdate() const { return last_update_; }
    void updateTimestamp() { last_update_ = std::chrono::system_clock::now(); }

protected:
    std::string name_;
    std::string uuid_;
    std::string type_;
    DeviceState state_;
    bool connected_;
    bool simulated_;

    DeviceInfo device_info_;
    DeviceCapabilities capabilities_;

    std::unordered_map<std::string, std::shared_ptr<DeviceProperty>> properties_;
    mutable std::mutex state_mutex_;
    mutable std::mutex properties_mutex_;

    std::chrono::system_clock::time_point last_update_;

    // 连接参数
    std::string connection_port_;
    ConnectionType connection_type_{ConnectionType::NONE};
    int connection_timeout_{5000};
};

#endif
