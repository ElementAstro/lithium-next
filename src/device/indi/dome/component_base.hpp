/*
 * component_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_DEVICE_INDI_DOME_COMPONENT_BASE_HPP
#define LITHIUM_DEVICE_INDI_DOME_COMPONENT_BASE_HPP

#include <libindi/indiapi.h>
#include <libindi/indiproperty.h>

#include <memory>
#include <string>

namespace lithium::device::indi {

// Forward declaration
class INDIDomeCore;

/**
 * @brief Base class for all dome components providing common functionality
 *        and standardized interface for property handling and core interaction.
 */
class DomeComponentBase {
public:
    explicit DomeComponentBase(std::shared_ptr<INDIDomeCore> core, std::string name)
        : core_(std::move(core)), component_name_(std::move(name)) {}

    virtual ~DomeComponentBase() = default;

    // Non-copyable, non-movable
    DomeComponentBase(const DomeComponentBase&) = delete;
    DomeComponentBase& operator=(const DomeComponentBase&) = delete;
    DomeComponentBase(DomeComponentBase&&) = delete;
    DomeComponentBase& operator=(DomeComponentBase&&) = delete;

    /**
     * @brief Initialize the component
     * @return true if initialization successful, false otherwise
     */
    virtual auto initialize() -> bool = 0;

    /**
     * @brief Cleanup component resources
     * @return true if cleanup successful, false otherwise
     */
    virtual auto cleanup() -> bool = 0;

    /**
     * @brief Handle INDI property updates
     * @param property The updated property
     */
    virtual void handlePropertyUpdate(const INDI::Property& property) = 0;

    /**
     * @brief Get component name
     * @return Component name
     */
    [[nodiscard]] auto getName() const -> const std::string& { return component_name_; }

    /**
     * @brief Check if component is initialized
     * @return true if initialized, false otherwise
     */
    [[nodiscard]] auto isInitialized() const -> bool { return is_initialized_; }

protected:
    /**
     * @brief Get reference to the core dome controller
     * @return Shared pointer to core, may be null if core was destroyed
     */
    [[nodiscard]] auto getCore() const -> std::shared_ptr<INDIDomeCore> { return core_.lock(); }

    /**
     * @brief Check if property belongs to our device
     * @param property Property to check
     * @return true if property is from our device, false otherwise
     */
    [[nodiscard]] auto isOurProperty(const INDI::Property& property) const -> bool;

    /**
     * @brief Log informational message with component name prefix
     * @param message Message to log
     */
    void logInfo(const std::string& message) const;

    /**
     * @brief Log warning message with component name prefix
     * @param message Message to log
     */
    void logWarning(const std::string& message) const;

    /**
     * @brief Log error message with component name prefix
     * @param message Message to log
     */
    void logError(const std::string& message) const;

    /**
     * @brief Set initialization state
     * @param initialized Initialization state
     */
    void setInitialized(bool initialized) { is_initialized_ = initialized; }

private:
    std::weak_ptr<INDIDomeCore> core_;
    std::string component_name_;
    bool is_initialized_{false};
};

} // namespace lithium::device::indi

#endif // LITHIUM_DEVICE_INDI_DOME_COMPONENT_BASE_HPP
