/*
 * indihub_agent.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDIHub Agent - Manages INDIHub agent process

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_MANAGER_INDIHUB_AGENT_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_INDIHUB_AGENT_HPP

#include <memory>
#include <string>
#include <thread>

namespace lithium::client::indi_manager {

class AsyncSystemCommand;

static const std::string INDIHUB_AGENT_DEFAULT_MODE = "local";
static const std::string INDIHUB_AGENT_CONFIG = "/tmp/indihub_agent.conf";

/**
 * @class IndiHubAgent
 * @brief A class to manage the INDIHub agent.
 *
 * This class provides functionality to start and stop the INDIHub agent,
 * check its running status, and manage its configuration.
 */
class IndiHubAgent {
public:
    /**
     * @brief Constructs an IndiHubAgent with the given parameters.
     * @param webAddr The web address of the INDIHub server.
     * @param hostname The hostname of the INDIHub agent.
     * @param port The port number of the INDIHub agent.
     */
    IndiHubAgent(const std::string& webAddr, const std::string& hostname,
                 int port);

    /**
     * @brief Destructor for IndiHubAgent.
     */
    ~IndiHubAgent();

    /**
     * @brief Starts the INDIHub agent with the given profile and mode.
     * @param profile The profile to use for the INDIHub agent.
     * @param mode The mode to run the INDIHub agent in (default is "local").
     * @param conf The path to the configuration file.
     */
    void start(const std::string& profile,
               const std::string& mode = INDIHUB_AGENT_DEFAULT_MODE,
               const std::string& conf = INDIHUB_AGENT_CONFIG);

    /**
     * @brief Stops the INDIHub agent.
     */
    void stop();

    /**
     * @brief Checks if the INDIHub agent is currently running.
     * @return True if the agent is running, false otherwise.
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Gets the current mode of the INDIHub agent.
     * @return The current mode of the agent.
     */
    [[nodiscard]] std::string getMode() const;

private:
    /**
     * @brief Runs the INDIHub agent with the given profile, mode, and
     * configuration.
     */
    void run(const std::string& profile, const std::string& mode,
             const std::string& conf);

    std::string webAddr_;
    std::string hostname_;
    int port_;
    std::string mode_;
    std::unique_ptr<AsyncSystemCommand> asyncCmd_;
    std::unique_ptr<std::thread> commandThread_;
};

}  // namespace lithium::client::indi_manager

#endif  // LITHIUM_CLIENT_INDI_MANAGER_INDIHUB_AGENT_HPP
