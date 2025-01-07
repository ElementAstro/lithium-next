/*
 * device.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Basic Device Defintion

*************************************************/

#ifndef ATOM_DRIVER_HPP
#define ATOM_DRIVER_HPP

#include "atom/utils/uuid.hpp"

#include <string>
#include <vector>

class AtomDriver {
public:
    explicit AtomDriver(std::string name)
        : name_(name), uuid_(atom::utils::UUID().toString()) {}
    virtual ~AtomDriver() = default;

    // 核心接口
    virtual bool initialize() = 0;
    virtual bool destroy() = 0;
    virtual bool connect(const std::string &port, int timeout = 5000,
                         int maxRetry = 3) = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual std::vector<std::string> scan() = 0;

    // 设备信息
    std::string getUUID() const { return uuid_; }
    std::string getName() const { return name_; }
    void setName(const std::string &newName) { name_ = newName; }
    std::string getType() const { return type_; }

protected:
    std::string name_;
    std::string uuid_;
    std::string type_;
};

#endif
