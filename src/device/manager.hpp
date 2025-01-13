#ifndef LITHIUM_DEVICE_MANAGER_HPP
#define LITHIUM_DEVICE_MANAGER_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "template/device.hpp"

#include "atom/error/exception.hpp"

class DeviceNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_NOT_FOUND(...)                               \
    throw DeviceNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__)

class DeviceTypeNotFoundException : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_DEVICE_TYPE_NOT_FOUND(...)                              \
    throw DeviceTypeNotFoundException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)


namespace lithium {

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();
    
    // 禁用拷贝
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;
    
    // 设备管理接口
    void addDevice(const std::string& type, std::shared_ptr<AtomDriver> device);
    void removeDevice(const std::string& type, std::shared_ptr<AtomDriver> device);
    void removeDeviceByName(const std::string& name);
    std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>> getDevices() const;
    
    // 主设备管理
    void setPrimaryDevice(const std::string& type, std::shared_ptr<AtomDriver> device);
    std::shared_ptr<AtomDriver> getPrimaryDevice(const std::string& type) const;
    
    // 设备操作接口
    void connectAllDevices();
    void disconnectAllDevices();
    void connectDevicesByType(const std::string& type);
    void disconnectDevicesByType(const std::string& type);
    void connectDeviceByName(const std::string& name);
    void disconnectDeviceByName(const std::string& name);
    
    // 查询接口
    std::shared_ptr<AtomDriver> getDeviceByName(const std::string& name) const;
    std::shared_ptr<AtomDriver> findDeviceByName(const std::string& name) const;
    std::vector<std::shared_ptr<AtomDriver>> findDevicesByType(const std::string& type) const;
    bool isDeviceConnected(const std::string& name) const;
    std::string getDeviceType(const std::string& name) const;
    
    // 设备生命周期管理
    bool initializeDevice(const std::string& name);
    bool destroyDevice(const std::string& name);
    std::vector<std::string> scanDevices(const std::string& type);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}  // namespace lithium

#endif  // LITHIUM_DEVICE_MANAGER_HPP