#ifndef LITHIUM_INDI_FILTERWHEEL_CONFIGURATION_MANAGER_HPP
#define LITHIUM_INDI_FILTERWHEEL_CONFIGURATION_MANAGER_HPP

#include "component_base.hpp"
#include "device/template/filterwheel.hpp"
#include <unordered_map>
#include <vector>
#include <string>

namespace lithium::device::indi::filterwheel {

/**
 * @brief Configuration data for a complete filter wheel setup.
 */
struct FilterWheelConfiguration {
    std::string name;
    std::vector<FilterInfo> filters;
    int maxSlots = 8;
    std::string description;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point lastUsed;
};

/**
 * @brief Manages configuration presets for INDI filter wheels.
 *
 * This component handles saving, loading, and managing complete filter wheel
 * configurations including filter names, types, and properties. Configurations
 * can be saved as named presets and loaded later for quick setup.
 */
class ConfigurationManager : public FilterWheelComponentBase {
public:
    /**
     * @brief Constructor with shared core.
     * @param core Shared pointer to the INDIFilterWheelCore
     */
    explicit ConfigurationManager(std::shared_ptr<INDIFilterWheelCore> core);

    /**
     * @brief Virtual destructor.
     */
    ~ConfigurationManager() override = default;

    /**
     * @brief Initialize the configuration manager.
     * @return true if initialization was successful, false otherwise.
     */
    bool initialize() override;

    /**
     * @brief Cleanup resources and shutdown the component.
     */
    void shutdown() override;

    /**
     * @brief Get the component's name for logging and identification.
     * @return Name of the component.
     */
    std::string getComponentName() const override { return "ConfigurationManager"; }

    /**
     * @brief Save current filter configuration with a name.
     * @param name Configuration name/identifier.
     * @return true if saved successfully, false otherwise.
     */
    bool saveFilterConfiguration(const std::string& name);

    /**
     * @brief Load a saved filter configuration.
     * @param name Configuration name to load.
     * @return true if loaded successfully, false otherwise.
     */
    bool loadFilterConfiguration(const std::string& name);

    /**
     * @brief Delete a saved configuration.
     * @param name Configuration name to delete.
     * @return true if deleted successfully, false otherwise.
     */
    bool deleteFilterConfiguration(const std::string& name);

    /**
     * @brief Get list of available configuration names.
     * @return Vector of configuration names.
     */
    std::vector<std::string> getAvailableConfigurations() const;

    /**
     * @brief Get details of a specific configuration.
     * @param name Configuration name.
     * @return Configuration details if found, nullopt otherwise.
     */
    std::optional<FilterWheelConfiguration> getConfiguration(const std::string& name) const;

    /**
     * @brief Export configuration to file.
     * @param name Configuration name.
     * @param filePath Path to export file.
     * @return true if exported successfully, false otherwise.
     */
    bool exportConfiguration(const std::string& name, const std::string& filePath);

    /**
     * @brief Import configuration from file.
     * @param filePath Path to import file.
     * @return Configuration name if imported successfully, nullopt otherwise.
     */
    std::optional<std::string> importConfiguration(const std::string& filePath);

private:
    bool initialized_{false};
    std::unordered_map<std::string, FilterWheelConfiguration> configurations_;

    // File operations
    bool saveConfigurationsToFile();
    bool loadConfigurationsFromFile();
    std::string getConfigurationFilePath() const;

    // Current state capture
    FilterWheelConfiguration captureCurrentConfiguration(const std::string& name);
    bool applyConfiguration(const FilterWheelConfiguration& config);

    // Validation
    bool isValidConfigurationName(const std::string& name) const;
};

}  // namespace lithium::device::indi::filterwheel

#endif  // LITHIUM_INDI_FILTERWHEEL_CONFIGURATION_MANAGER_HPP
