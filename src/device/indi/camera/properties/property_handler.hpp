#ifndef LITHIUM_INDI_CAMERA_PROPERTY_HANDLER_HPP
#define LITHIUM_INDI_CAMERA_PROPERTY_HANDLER_HPP

#include "../component_base.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace lithium::device::indi::camera {

/**
 * @brief INDI property handling component
 * 
 * This component coordinates INDI property handling across all
 * camera components and provides centralized property management.
 */
class PropertyHandler : public ComponentBase {
public:
    explicit PropertyHandler(INDICameraCore* core);
    ~PropertyHandler() override = default;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto handleProperty(INDI::Property property) -> bool override;

    // Property registration for components
    auto registerPropertyHandler(const std::string& propertyName, 
                                ComponentBase* component) -> void;
    auto unregisterPropertyHandler(const std::string& propertyName, 
                                 ComponentBase* component) -> void;

    // Property utilities
    auto setPropertyNumber(const std::string& propertyName, double value) -> bool;
    auto setPropertySwitch(const std::string& propertyName, int index, bool state) -> bool;
    auto setPropertyText(const std::string& propertyName, const std::string& value) -> bool;

    // Property monitoring
    auto watchProperty(const std::string& propertyName, 
                      std::function<void(INDI::Property)> callback) -> void;
    auto unwatchProperty(const std::string& propertyName) -> void;

    // Property information
    auto getPropertyList() const -> std::vector<std::string>;
    auto isPropertyAvailable(const std::string& propertyName) const -> bool;

private:
    // Property to component mapping
    std::map<std::string, std::vector<ComponentBase*>> propertyHandlers_;
    
    // Property watchers
    std::map<std::string, std::function<void(INDI::Property)>> propertyWatchers_;
    
    // Available properties cache
    std::vector<std::string> availableProperties_;
    
    // Helper methods
    void updateAvailableProperties();
    void distributePropertyToComponents(INDI::Property property);
    auto validateProperty(INDI::Property property) -> bool;
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_PROPERTY_HANDLER_HPP
