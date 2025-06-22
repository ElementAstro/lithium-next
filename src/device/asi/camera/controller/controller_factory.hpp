/*
 * controller_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Controller Factory

This factory provides runtime selection between the monolithic and
modular ASI Camera Controller implementations.

*************************************************/

#pragma once

#include <memory>
#include <string>

// Forward declarations
namespace lithium::device::asi::camera::controller {
class ASICameraController;
class ASICameraControllerV2;
}

namespace lithium::device::asi::camera {

/**
 * @brief Controller Type Selection
 */
enum class ControllerType {
    MONOLITHIC,     // Original monolithic controller
    MODULAR,        // New modular controller (V2)
    AUTO            // Auto-select based on configuration
};

/**
 * @brief Factory for ASI Camera Controllers
 * 
 * Provides a unified interface to create either monolithic or modular
 * camera controllers based on runtime configuration or user preference.
 */
class ControllerFactory {
public:
    /**
     * @brief Create an ASI Camera Controller
     * 
     * @param type Controller type to create
     * @param parent Parent camera instance (for monolithic controller)
     * @return Pointer to the created controller (type-erased)
     */
    static std::unique_ptr<void> createController(ControllerType type, void* parent = nullptr);
    
    /**
     * @brief Create Monolithic Controller
     * 
     * @param parent Parent ASICamera instance
     * @return Unique pointer to ASICameraController
     */
    static std::unique_ptr<controller::ASICameraController> createMonolithicController(void* parent);
    
    /**
     * @brief Create Modular Controller
     * 
     * @return Unique pointer to ASICameraControllerV2
     */
    static std::unique_ptr<controller::ASICameraControllerV2> createModularController();
    
    /**
     * @brief Get Default Controller Type
     * 
     * Determines the default controller type based on configuration,
     * environment variables, or compile-time settings.
     * 
     * @return Default controller type
     */
    static ControllerType getDefaultControllerType();
    
    /**
     * @brief Set Default Controller Type
     * 
     * @param type New default controller type
     */
    static void setDefaultControllerType(ControllerType type);
    
    /**
     * @brief Check if Modular Controller is Available
     * 
     * @return True if modular controller components are available
     */
    static bool isModularControllerAvailable();
    
    /**
     * @brief Check if Monolithic Controller is Available
     * 
     * @return True if monolithic controller is available
     */
    static bool isMonolithicControllerAvailable();
    
    /**
     * @brief Get Controller Type Name
     * 
     * @param type Controller type
     * @return Human-readable name
     */
    static std::string getControllerTypeName(ControllerType type);
    
    /**
     * @brief Parse Controller Type from String
     * 
     * @param typeName Controller type name
     * @return Controller type, or AUTO if not recognized
     */
    static ControllerType parseControllerType(const std::string& typeName);

private:
    static ControllerType defaultType_;
    
    // Helper methods
    static ControllerType autoSelectControllerType();
    static bool checkModularComponents();
    static std::string getEnvironmentVariable(const std::string& name, const std::string& defaultValue = "");
};

/**
 * @brief RAII wrapper for type-erased controllers
 * 
 * This template class provides a type-safe wrapper around the factory-created
 * controllers, allowing users to work with a specific controller type while
 * maintaining the flexibility of runtime selection.
 */
template<typename ControllerType>
class ControllerWrapper {
public:
    explicit ControllerWrapper(std::unique_ptr<ControllerType> controller)
        : controller_(std::move(controller)) {}
    
    ControllerType* operator->() { return controller_.get(); }
    const ControllerType* operator->() const { return controller_.get(); }
    
    ControllerType& operator*() { return *controller_; }
    const ControllerType& operator*() const { return *controller_; }
    
    ControllerType* get() { return controller_.get(); }
    const ControllerType* get() const { return controller_.get(); }
    
    bool operator!() const { return !controller_; }
    explicit operator bool() const { return static_cast<bool>(controller_); }

private:
    std::unique_ptr<ControllerType> controller_;
};

// Type aliases for convenience
using MonolithicControllerPtr = ControllerWrapper<controller::ASICameraController>;
using ModularControllerPtr = ControllerWrapper<controller::ASICameraControllerV2>;

} // namespace lithium::device::asi::camera
