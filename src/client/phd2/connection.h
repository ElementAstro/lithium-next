// connection.h
#pragma once

#include "event_handler.h"
#include "types.h"

#include <curl/curl.h>
#include <atomic>
#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace phd2 {

/**
 * @brief Manages the TCP connection to PHD2 using libcurl
 */
class Connection {
public:
    /**
     * @brief Construct a new Connection object
     *
     * @param host The hostname where PHD2 is running
     * @param port The port number (default: 4400)
     * @param eventHandler The handler for received events
     */
    Connection(std::string host, int port,
               std::shared_ptr<EventHandler> eventHandler);

    /**
     * @brief Destroy the Connection object and close any open connections
     */
    ~Connection();

    /**
     * @brief Connect to PHD2
     *
     * @param timeoutMs Connection timeout in milliseconds
     * @return true if connection was successful
     */
    bool connect(int timeoutMs = 5000);

    /**
     * @brief Disconnect from PHD2
     */
    void disconnect();

    /**
     * @brief Check if currently connected to PHD2
     */
    bool isConnected() const;

    /**
     * @brief Send an RPC method call to PHD2
     *
     * @param method The method name
     * @param params The parameters (as a JSON object)
     * @param timeoutMs Timeout for the response in milliseconds
     * @return RpcResponse The response from PHD2
     */
    RpcResponse sendRpc(const std::string& method,
                        const json& params = json({}), int timeoutMs = 10000);

private:
    // Connection parameters and state
    std::string host_;
    int port_;
    std::shared_ptr<EventHandler> eventHandler_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> stopping_{false};

    // libcurl handles
    CURL* curlEasy_{nullptr};

    // Thread for receiving events
    std::unique_ptr<std::thread> receiveThread_;

    // Buffer for incoming data
    std::string receiveBuffer_;
    std::mutex bufferMutex_;

    // RPC response handling
    std::mutex rpcMutex_;
    std::condition_variable rpcCv_;
    int nextRpcId_{1};
    std::map<int, std::promise<RpcResponse>> pendingRpcs_;

    // Private methods
    void receiveLoop();
    void processLine(const std::string& line);
    void processEventMessage(const std::string& message);
    void processRpcResponse(const std::string& message, int id);
    Event parseEvent(const json& j);

    // Static libcurl callback
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb,
                                void* userdata);
};

}  // namespace phd2
