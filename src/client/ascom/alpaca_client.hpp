/*
 * alpaca_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: ASCOM Alpaca REST API client

*************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_ALPACA_CLIENT_HPP
#define LITHIUM_CLIENT_ASCOM_ALPACA_CLIENT_HPP

#include "ascom_types.hpp"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace lithium::client::ascom {

/**
 * @brief HTTP request method
 */
enum class HttpMethod {
    GET,
    PUT,
    POST,
    DELETE
};

/**
 * @brief Alpaca API client for communicating with ASCOM devices
 * 
 * This client implements the ASCOM Alpaca REST API protocol
 * for device discovery and control.
 */
class AlpacaClient {
public:
    /**
     * @brief Construct a new Alpaca Client
     * @param host Server host address
     * @param port Server port (default 11111)
     */
    explicit AlpacaClient(std::string host = "localhost", int port = 11111);
    
    /**
     * @brief Destructor
     */
    ~AlpacaClient();
    
    // Disable copy
    AlpacaClient(const AlpacaClient&) = delete;
    AlpacaClient& operator=(const AlpacaClient&) = delete;
    
    // Move operations deleted due to mutex member
    AlpacaClient(AlpacaClient&&) = delete;
    AlpacaClient& operator=(AlpacaClient&&) = delete;
    
    // ==================== Connection ====================
    
    /**
     * @brief Connect to the Alpaca server
     * @param timeout Connection timeout in milliseconds
     * @return true if connection successful
     */
    bool connect(int timeout = 5000);
    
    /**
     * @brief Disconnect from the server
     */
    void disconnect();
    
    /**
     * @brief Check if connected to server
     */
    [[nodiscard]] bool isConnected() const;
    
    /**
     * @brief Set server address
     */
    void setServer(const std::string& host, int port);
    
    // ==================== Discovery ====================
    
    /**
     * @brief Discover Alpaca servers on the network
     * @param timeout Discovery timeout in milliseconds
     * @return Vector of discovered server addresses (host:port)
     */
    static std::vector<std::string> discoverServers(int timeout = 5000);
    
    /**
     * @brief Get server information
     * @return Server info or nullopt on failure
     */
    std::optional<AlpacaServerInfo> getServerInfo();
    
    /**
     * @brief Get list of configured devices
     * @return Vector of device descriptions
     */
    std::vector<ASCOMDeviceDescription> getConfiguredDevices();
    
    // ==================== Device Operations ====================
    
    /**
     * @brief Connect to a specific device
     * @param deviceType Device type
     * @param deviceNumber Device number
     * @return true if connection successful
     */
    bool connectDevice(ASCOMDeviceType deviceType, int deviceNumber);
    
    /**
     * @brief Disconnect from a specific device
     * @param deviceType Device type
     * @param deviceNumber Device number
     * @return true if disconnection successful
     */
    bool disconnectDevice(ASCOMDeviceType deviceType, int deviceNumber);
    
    /**
     * @brief Check if device is connected
     * @param deviceType Device type
     * @param deviceNumber Device number
     * @return true if device is connected
     */
    bool isDeviceConnected(ASCOMDeviceType deviceType, int deviceNumber);
    
    // ==================== Common Device Properties ====================
    
    /**
     * @brief Get device name
     */
    std::string getDeviceName(ASCOMDeviceType deviceType, int deviceNumber);
    
    /**
     * @brief Get device description
     */
    std::string getDeviceDescription(ASCOMDeviceType deviceType, int deviceNumber);
    
    /**
     * @brief Get driver info
     */
    std::string getDriverInfo(ASCOMDeviceType deviceType, int deviceNumber);
    
    /**
     * @brief Get driver version
     */
    std::string getDriverVersion(ASCOMDeviceType deviceType, int deviceNumber);
    
    /**
     * @brief Get interface version
     */
    int getInterfaceVersion(ASCOMDeviceType deviceType, int deviceNumber);
    
    /**
     * @brief Get supported actions
     */
    std::vector<std::string> getSupportedActions(ASCOMDeviceType deviceType, int deviceNumber);
    
    // ==================== Generic API Access ====================
    
    /**
     * @brief Execute GET request
     * @param deviceType Device type
     * @param deviceNumber Device number
     * @param method API method name
     * @param params Query parameters
     * @return API response
     */
    AlpacaResponse get(ASCOMDeviceType deviceType, int deviceNumber,
                       const std::string& method,
                       const std::unordered_map<std::string, std::string>& params = {});
    
    /**
     * @brief Execute PUT request
     * @param deviceType Device type
     * @param deviceNumber Device number
     * @param method API method name
     * @param params Form parameters
     * @return API response
     */
    AlpacaResponse put(ASCOMDeviceType deviceType, int deviceNumber,
                       const std::string& method,
                       const std::unordered_map<std::string, std::string>& params = {});
    
    /**
     * @brief Execute action
     * @param deviceType Device type
     * @param deviceNumber Device number
     * @param action Action name
     * @param parameters Action parameters (JSON string)
     * @return Action result
     */
    std::string action(ASCOMDeviceType deviceType, int deviceNumber,
                       const std::string& action, const std::string& parameters = "");
    
    /**
     * @brief Send command (blind)
     * @param deviceType Device type
     * @param deviceNumber Device number
     * @param command Command string
     * @param raw If true, send raw command
     */
    void commandBlind(ASCOMDeviceType deviceType, int deviceNumber,
                      const std::string& command, bool raw = false);
    
    /**
     * @brief Send command and get boolean result
     */
    bool commandBool(ASCOMDeviceType deviceType, int deviceNumber,
                     const std::string& command, bool raw = false);
    
    /**
     * @brief Send command and get string result
     */
    std::string commandString(ASCOMDeviceType deviceType, int deviceNumber,
                              const std::string& command, bool raw = false);
    
    // ==================== Configuration ====================
    
    /**
     * @brief Set request timeout
     * @param timeout Timeout in milliseconds
     */
    void setTimeout(int timeout);
    
    /**
     * @brief Get current timeout
     */
    [[nodiscard]] int getTimeout() const { return timeout_; }
    
    /**
     * @brief Get host address
     */
    [[nodiscard]] const std::string& getHost() const { return host_; }
    
    /**
     * @brief Get port number
     */
    [[nodiscard]] int getPort() const { return port_; }
    
    /**
     * @brief Get next client transaction ID
     */
    int getNextTransactionId();
    
private:
    /**
     * @brief Build API URL
     */
    std::string buildUrl(ASCOMDeviceType deviceType, int deviceNumber,
                         const std::string& method) const;
    
    /**
     * @brief Execute HTTP request
     */
    AlpacaResponse executeRequest(HttpMethod method, const std::string& url,
                                  const std::unordered_map<std::string, std::string>& params);
    
    /**
     * @brief Parse JSON response
     */
    static AlpacaResponse parseResponse(const std::string& responseBody);
    
    std::string host_;
    int port_;
    int timeout_{5000};
    std::atomic<bool> connected_{false};
    std::atomic<int> transactionId_{1};
    mutable std::mutex mutex_;
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_ALPACA_CLIENT_HPP
