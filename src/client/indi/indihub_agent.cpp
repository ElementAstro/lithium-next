#include "indihub_agent.hpp"
#include "async_system_command.hpp"

#include "atom/error/exception.hpp"
#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/env.hpp"

#include <sstream>

namespace {
constexpr const char* INDIHUB_AGENT_OFF = "off";
constexpr const char* INDIHUB_AGENT_DEFAULT_MODE = "solo";

std::string getConfigPath() {
    LOG_F(INFO, "Getting config path");
    std::string configPath;
    try {
        atom::utils::Env env;
        configPath = env.getEnv("HOME");
        configPath += "/.indihub";
        LOG_F(INFO, "Config path set to: {}", configPath);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get HOME environment variable: {}", e.what());
        configPath = "/tmp/indihub";
    }

    if (!atom::io::isFolderExists(configPath)) {
        LOG_F(INFO, "Config path does not exist, creating: {}", configPath);
        if (!atom::io::createDirectory(configPath)) {
            THROW_RUNTIME_ERROR("Failed to create config directory");
        }
    }

    std::string fullPath = configPath + "/indihub.json";
    LOG_F(INFO, "Full config path: {}", fullPath);
    return fullPath;
}

const std::string INDIHUB_AGENT_CONFIG = getConfigPath();
}  // namespace

IndiHubAgent::IndiHubAgent(const std::string& web_addr,
                           const std::string& hostname, int port)
    : web_addr_(web_addr),
      hostname_(hostname),
      port_(port),
      mode_(INDIHUB_AGENT_OFF),
      async_cmd_(nullptr),
      command_thread_(nullptr) {
    LOG_F(INFO,
          "IndiHubAgent created with web_addr: {}, hostname: {}, port: {}",
          web_addr, hostname, port);
}

IndiHubAgent::~IndiHubAgent() {
    LOG_F(INFO, "IndiHubAgent destructor called");
    stop();
}

void IndiHubAgent::run(const std::string& profile, const std::string& mode,
                       const std::string& conf) {
    LOG_F(INFO, "Running IndiHubAgent with profile: {}, mode: {}, conf: {}",
          profile, mode, conf);
    std::stringstream cmd;
    cmd << "indihub-agent"
        << " -indi-server-manager=" << web_addr_ << " -indi-profile=" << profile
        << " -mode=" << mode << " -conf=" << conf
        << " -api-origins=" << hostname_ << ":" << port_ << "," << hostname_
        << ".local:" << port_ << " > /tmp/indihub-agent.log 2>&1 &";

    LOG_F(INFO, "Running command: {}", cmd.str());

    async_cmd_ = std::make_unique<AsyncSystemCommand>(cmd.str());
    command_thread_ =
        std::make_unique<std::thread>([this]() { async_cmd_->run(); });
}

void IndiHubAgent::start(const std::string& profile, const std::string& mode,
                         const std::string& conf) {
    LOG_F(INFO, "Starting IndiHubAgent with profile: {}, mode: {}, conf: {}",
          profile, mode, conf);
    if (isRunning()) {
        LOG_F(INFO, "IndiHubAgent is already running, stopping it first");
        stop();
    }
    run(profile, mode, conf);
    mode_ = mode;
    LOG_F(INFO, "IndiHubAgent started with mode: {}", mode);
}

void IndiHubAgent::stop() {
    LOG_F(INFO, "Stopping IndiHubAgent");
    if (!async_cmd_) {
        LOG_F(INFO, "IndiHubAgent is not running");
        return;
    }

    try {
        async_cmd_->terminate();
        if (command_thread_ && command_thread_->joinable()) {
            command_thread_->join();
        }
        LOG_F(INFO, "IndiHubAgent terminated successfully");
    } catch (const std::exception& e) {
        LOG_F(WARNING, "IndiHubAgent termination failed with error: {}",
              e.what());
    }
}

bool IndiHubAgent::isRunning() const {
    bool running = async_cmd_ && async_cmd_->isRunning();
    LOG_F(INFO, "IndiHubAgent isRunning: {}", running);
    return running;
}

std::string IndiHubAgent::getMode() const {
    LOG_F(INFO, "Getting IndiHubAgent mode: {}", mode_);
    return mode_;
}
