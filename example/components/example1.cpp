#ifndef LOGGER_COMPONENT_HPP
#define LOGGER_COMPONENT_HPP

#include "atom/components/component.hpp"
#include "atom/components/module_macro.hpp"
#include "atom/components/registry.hpp"

class LoggerComponent : public Component {
public:
    explicit LoggerComponent(std::string name) : Component(std::move(name)) {}

    auto initialize() -> bool override {
        LOG_F(INFO, "LoggerComponent initialized");
        return true;
    }

    auto destroy() -> bool override {
        LOG_F(INFO, "LoggerComponent destroyed");
        return true;
    }

    void logMessage(const std::string& message) {
        LOG_F(INFO, "Log: {}", message);
    }
};

#endif  // LOGGER_COMPONENT_HPP

ATOM_MODULE(logger, [](auto& component) {
    component.def("logMessage", &LoggerComponent::logMessage, "Log message");
})
