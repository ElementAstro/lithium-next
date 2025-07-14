/*
 * device_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#pragma once

#include <string>
#include <memory>

namespace lithium {

/**
 * @brief Basic device interface for integrated device manager
 */
class IDevice {
public:
    virtual ~IDevice() = default;
    
    // Basic device operations
    virtual bool connect(const std::string& address = "", int timeout = 30000, int retry = 1) = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    // Device identification
    virtual std::string getName() const = 0;
    virtual std::string getType() const = 0;
    
    // Status
    virtual bool isHealthy() const { return isConnected(); }
};

// Type alias for backward compatibility with AtomDriver
using AtomDriverInterface = IDevice;

} // namespace lithium
