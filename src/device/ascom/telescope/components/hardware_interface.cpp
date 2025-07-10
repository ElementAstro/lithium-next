/*
 * hardware_interface_corrected.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Hardware Interface Implementation

This component provides a clean interface to ASCOM Telescope APIs,
handling low-level hardware communication, device management,
and both COM and Alpaca protocol integration.

*************************************************/

#include "hardware_interface.hpp"
#include <curl/curl.h>
#include <sstream>
#include <spdlog/spdlog.h>

namespace lithium::device::ascom::telescope::components {

HardwareInterface::HardwareInterface(boost::asio::io_context& io_context)
    : io_context_(io_context) {
    spdlog::info("HardwareInterface initialized");
}

HardwareInterface::~HardwareInterface() {
    if (connected_) {
        disconnect();
    }
}

// =========================================================================
// Initialization and Management
// =========================================================================

bool HardwareInterface::initialize() {
    try {
        initialized_ = true;
        spdlog::info("HardwareInterface initialized successfully");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize HardwareInterface: {}", e.what());
        return false;
    }
}

bool HardwareInterface::shutdown() {
    try {
        if (connected_) {
            disconnect();
        }
        initialized_ = false;
        spdlog::info("HardwareInterface shutdown successfully");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to shutdown HardwareInterface: {}", e.what());
        return false;
    }
}

// =========================================================================
// Device Discovery and Connection
// =========================================================================

std::vector<std::string> HardwareInterface::discoverDevices() {
    std::vector<std::string> devices;
    
    if (connectionType_ == ConnectionType::ALPACA_REST) {
        // Discover Alpaca devices
        try {
            // This is a placeholder - actual implementation would scan network
            devices.push_back("ASCOM.Simulator.Telescope");
            devices.push_back("ASCOM.Generic.Telescope");
        } catch (const std::exception& e) {
            spdlog::error("Failed to discover Alpaca devices: {}", e.what());
        }
    }
    
#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        // Discover COM devices
        try {
            // This would enumerate ASCOM drivers via COM
            devices.push_back("ASCOM.Simulator.Telescope");
        } catch (const std::exception& e) {
            spdlog::error("Failed to discover COM devices: {}", e.what());
        }
    }
#endif
    
    return devices;
}

bool HardwareInterface::connect(const ConnectionSettings& settings) {
    try {
        if (connected_) {
            spdlog::warn("Already connected to a telescope");
            return true;
        }
        
        currentSettings_ = settings;
        connectionType_ = settings.type;
        
        bool success = false;
        if (connectionType_ == ConnectionType::ALPACA_REST) {
            success = connectAlpaca(settings);
        }
#ifdef _WIN32
        else if (connectionType_ == ConnectionType::COM_DRIVER) {
            success = connectCOM(settings);
        }
#endif
        
        if (success) {
            connected_ = true;
            deviceName_ = settings.deviceName;
            spdlog::info("Connected to telescope: {}", deviceName_);
        }
        
        return success;
    } catch (const std::exception& e) {
        spdlog::error("Failed to connect to telescope: {}", e.what());
        setLastError("Connection failed: " + std::string(e.what()));
        return false;
    }
}

bool HardwareInterface::disconnect() {
    try {
        if (!connected_) {
            return true;
        }
        
        bool success = false;
        if (connectionType_ == ConnectionType::ALPACA_REST) {
            success = disconnectAlpaca();
        }
#ifdef _WIN32
        else if (connectionType_ == ConnectionType::COM_DRIVER) {
            success = disconnectCOM();
        }
#endif
        
        connected_ = false;
        deviceName_.clear();
        telescopeInfo_.reset();
        
        spdlog::info("Disconnected from telescope");
        return success;
    } catch (const std::exception& e) {
        spdlog::error("Failed to disconnect from telescope: {}", e.what());
        return false;
    }
}

// =========================================================================
// Alignment Operations
// =========================================================================

std::optional<AlignmentMode> HardwareInterface::getAlignmentMode() const {
    try {
        if (!connected_) {
            setLastError("Not connected to telescope");
            return std::nullopt;
        }
        
        if (connectionType_ == ConnectionType::ALPACA_REST) {
            auto response = sendAlpacaRequest("GET", "alignmentmode");
            if (response) {
                try {
                    auto json_response = nlohmann::json::parse(*response);
                    if (json_response.contains("Value")) {
                        int mode = json_response["Value"];
                        return static_cast<AlignmentMode>(mode);
                    }
                } catch (const nlohmann::json::exception& e) {
                    spdlog::error("Failed to parse alignment mode response: {}", e.what());
                }
            }
        }
        
        setLastError("Failed to get alignment mode");
        return std::nullopt;
    } catch (const std::exception& e) {
        setLastError("Exception in getAlignmentMode: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool HardwareInterface::setAlignmentMode(AlignmentMode mode) {
    try {
        if (!connected_) {
            setLastError("Not connected to telescope");
            return false;
        }
        
        if (connectionType_ == ConnectionType::ALPACA_REST) {
            std::string params = "AlignmentMode=" + std::to_string(static_cast<int>(mode));
            auto response = sendAlpacaRequest("PUT", "alignmentmode", params);
            if (response) {
                try {
                    auto json_response = nlohmann::json::parse(*response);
                    if (json_response.contains("ErrorNumber") && json_response["ErrorNumber"] == 0) {
                        clearError();
                        return true;
                    }
                } catch (const nlohmann::json::exception& e) {
                    spdlog::error("Failed to parse set alignment mode response: {}", e.what());
                }
            }
        }
        
        setLastError("Failed to set alignment mode");
        return false;
    } catch (const std::exception& e) {
        setLastError("Exception in setAlignmentMode: " + std::string(e.what()));
        return false;
    }
}

bool HardwareInterface::addAlignmentPoint(const EquatorialCoordinates& measured,
                                        const EquatorialCoordinates& target) {
    try {
        if (!connected_) {
            setLastError("Not connected to telescope");
            return false;
        }
        
        if (connectionType_ == ConnectionType::ALPACA_REST) {
            std::stringstream params;
            params << "MeasuredRA=" << measured.ra 
                   << "&MeasuredDec=" << measured.dec
                   << "&TargetRA=" << target.ra
                   << "&TargetDec=" << target.dec;
            
            auto response = sendAlpacaRequest("PUT", "addalignmentpoint", params.str());
            if (response) {
                try {
                    auto json_response = nlohmann::json::parse(*response);
                    if (json_response.contains("ErrorNumber") && json_response["ErrorNumber"] == 0) {
                        clearError();
                        return true;
                    }
                } catch (const nlohmann::json::exception& e) {
                    spdlog::error("Failed to parse add alignment point response: {}", e.what());
                }
            }
        }
        
        setLastError("Failed to add alignment point");
        return false;
    } catch (const std::exception& e) {
        setLastError("Exception in addAlignmentPoint: " + std::string(e.what()));
        return false;
    }
}

bool HardwareInterface::clearAlignment() {
    try {
        if (!connected_) {
            setLastError("Not connected to telescope");
            return false;
        }
        
        if (connectionType_ == ConnectionType::ALPACA_REST) {
            auto response = sendAlpacaRequest("PUT", "clearalignment");
            if (response) {
                try {
                    auto json_response = nlohmann::json::parse(*response);
                    if (json_response.contains("ErrorNumber") && json_response["ErrorNumber"] == 0) {
                        clearError();
                        return true;
                    }
                } catch (const nlohmann::json::exception& e) {
                    spdlog::error("Failed to parse clear alignment response: {}", e.what());
                }
            }
        }
        
        setLastError("Failed to clear alignment");
        return false;
    } catch (const std::exception& e) {
        setLastError("Exception in clearAlignment: " + std::string(e.what()));
        return false;
    }
}

std::optional<int> HardwareInterface::getAlignmentPointCount() const {
    try {
        if (!connected_) {
            setLastError("Not connected to telescope");
            return std::nullopt;
        }
        
        if (connectionType_ == ConnectionType::ALPACA_REST) {
            auto response = sendAlpacaRequest("GET", "alignmentpointcount");
            if (response) {
                try {
                    auto json_response = nlohmann::json::parse(*response);
                    if (json_response.contains("Value")) {
                        return json_response["Value"];
                    }
                } catch (const nlohmann::json::exception& e) {
                    spdlog::error("Failed to parse alignment point count response: {}", e.what());
                }
            }
        }
        
        setLastError("Failed to get alignment point count");
        return std::nullopt;
    } catch (const std::exception& e) {
        setLastError("Exception in getAlignmentPointCount: " + std::string(e.what()));
        return std::nullopt;
    }
}

// =========================================================================
// Helper Methods
// =========================================================================

bool HardwareInterface::connectAlpaca(const ConnectionSettings& settings) {
    try {
        // Simple connection test without complex client creation
        // In a real implementation, this would use a proper Alpaca client
        
        // Test connection with a simple request
        auto response = sendAlpacaRequest("GET", "connected");
        if (response) {
            try {
                auto json_response = nlohmann::json::parse(*response);
                // Try to connect if not already connected
                if (!json_response["Value"].get<bool>()) {
                    response = sendAlpacaRequest("PUT", "connected", "Connected=true");
                    if (!response) {
                        return false;
                    }
                    json_response = nlohmann::json::parse(*response);
                    if (json_response["ErrorNumber"] != 0) {
                        return false;
                    }
                }
                return true;
            } catch (const nlohmann::json::exception& e) {
                spdlog::error("Failed to parse Alpaca connection response: {}", e.what());
                return false;
            }
        }
        
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Alpaca connection failed: {}", e.what());
        return false;
    }
}

bool HardwareInterface::disconnectAlpaca() {
    try {
        auto response = sendAlpacaRequest("PUT", "connected", "Connected=false");
        if (response) {
            try {
                auto json_response = nlohmann::json::parse(*response);
                return json_response["ErrorNumber"] == 0;
            } catch (const nlohmann::json::exception& e) {
                spdlog::error("Failed to parse Alpaca disconnection response: {}", e.what());
                return false;
            }
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Alpaca disconnection failed: {}", e.what());
        return false;
    }
}

#ifdef _WIN32
bool HardwareInterface::connectCOM(const ConnectionSettings& settings) {
    // COM connection implementation would go here
    // This is a placeholder for Windows COM integration
    spdlog::info("COM connection not implemented yet");
    return false;
}

bool HardwareInterface::disconnectCOM() {
    // COM disconnection implementation would go here
    return true;
}
#endif

std::optional<std::string> HardwareInterface::sendAlpacaRequest(const std::string& method, 
                                                               const std::string& endpoint, 
                                                               const std::string& params) const {
    try {
        // This is a simplified mock implementation
        // In a real implementation, this would use CURL or a proper HTTP client
        std::stringstream url;
        url << "http://" << currentSettings_.host << ":" << currentSettings_.port
            << "/api/v1/telescope/" << currentSettings_.deviceNumber << "/" << endpoint;
        
        // Mock response generation based on endpoint
        nlohmann::json mockResponse;
        mockResponse["ErrorNumber"] = 0;
        mockResponse["ErrorMessage"] = "";
        
        if (endpoint == "alignmentmode") {
            mockResponse["Value"] = static_cast<int>(AlignmentMode::UNKNOWN);
        } else if (endpoint == "alignmentpointcount") {
            mockResponse["Value"] = 0;
        } else if (endpoint == "connected") {
            mockResponse["Value"] = true;
        }
        
        return mockResponse.dump();
    } catch (const std::exception& e) {
        spdlog::error("Alpaca request failed: {}", e.what());
        return std::nullopt;
    }
}

} // namespace lithium::device::ascom::telescope::components
