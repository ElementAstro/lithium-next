#include "indiserver.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "atom/system/software.hpp"

INDIManager::INDIManager(std::unique_ptr<Connector> conn,
                         const std::string& web_addr,
                         const std::string& hostname, int port)
    : connector(std::move(conn)),
      indihub_agent(std::make_unique<IndiHubAgent>(web_addr, hostname, port)) {}

INDIManager::~INDIManager() { stopIndiHub(); }

auto INDIManager::startServer() -> bool { return connector->startServer(); }

auto INDIManager::stopServer() -> bool { return connector->stopServer(); }

auto INDIManager::isRunning() -> bool { return connector->isRunning(); }

auto INDIManager::isInstalled() -> bool {
    return atom::system::checkSoftwareInstalled("indiserver");
}

auto INDIManager::startDriver(
    const std::shared_ptr<INDIDeviceContainer>& driver) -> bool {
    return connector->startDriver(driver);
}

auto INDIManager::stopDriver(const std::shared_ptr<INDIDeviceContainer>& driver)
    -> bool {
    return connector->stopDriver(driver);
}

auto INDIManager::setProp(const std::string& dev, const std::string& prop,
                          const std::string& element, const std::string& value)
    -> bool {
    return connector->setProp(dev, prop, element, value);
}

auto INDIManager::getProp(const std::string& dev, const std::string& prop,
                          const std::string& element) -> std::string {
    return connector->getProp(dev, prop, element);
}

std::string INDIManager::getState(const std::string& dev,
                                  const std::string& prop) {
    return connector->getState(dev, prop);
}

std::unordered_map<std::string, std::shared_ptr<INDIDeviceContainer>>
INDIManager::getRunningDrivers() {
    return connector->getRunningDrivers();
}

bool INDIManager::startIndiHub(const std::string& profile,
                               const std::string& mode) {
    if (!isRunning()) {
        LOG_WARN("Cannot start IndiHub: INDI server is not running");
        return false;
    }

    try {
        indihub_agent->start(profile, mode);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start IndiHub: {}", e.what());
        return false;
    }
}

void INDIManager::stopIndiHub() {
    if (indihub_agent) {
        indihub_agent->stop();
    }
}

bool INDIManager::isIndiHubRunning() const {
    return indihub_agent && indihub_agent->isRunning();
}

std::string INDIManager::getIndiHubMode() const {
    return indihub_agent ? indihub_agent->getMode() : "off";
}
