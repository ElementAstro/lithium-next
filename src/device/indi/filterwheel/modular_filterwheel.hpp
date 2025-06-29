#ifndef LITHIUM_INDI_FILTERWHEEL_MODULAR_FILTERWHEEL_HPP
#define LITHIUM_INDI_FILTERWHEEL_MODULAR_FILTERWHEEL_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>
#include <memory>

#include "device/template/filterwheel.hpp"
#include "core/indi_filterwheel_core.hpp"
#include "property_manager.hpp"
#include "filter_controller.hpp"
#include "statistics_manager.hpp"
#include "temperature_manager.hpp"
#include "configuration_manager.hpp"
#include "profiler.hpp"

namespace lithium::device::indi::filterwheel {

// Forward declarations
struct FilterPerformanceStats;

/**
 * @brief Modular INDI FilterWheel implementation
 *
 * This class orchestrates various components to provide complete filterwheel
 * functionality while maintaining clean separation of concerns. It follows
 * the same architectural pattern as ModularINDIFocuser.
 */
class ModularINDIFilterWheel : public INDI::BaseClient, public AtomFilterWheel {
public:
    explicit ModularINDIFilterWheel(std::string name);
    ~ModularINDIFilterWheel() override = default;

    // Non-copyable, non-movable due to atomic members
    ModularINDIFilterWheel(const ModularINDIFilterWheel& other) = delete;
    ModularINDIFilterWheel& operator=(const ModularINDIFilterWheel& other) = delete;
    ModularINDIFilterWheel(ModularINDIFilterWheel&& other) = delete;
    ModularINDIFilterWheel& operator=(ModularINDIFilterWheel&& other) = delete;

    // AtomFilterWheel interface implementation
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName, int timeout, int maxRetry)
        -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    [[nodiscard]] auto isConnected() const -> bool override;

    // Filter control (delegated to FilterController)
    auto getPosition() -> std::optional<int> override;
    auto setPosition(int position) -> bool override;
    auto getFilterCount() -> int override;
    auto isValidPosition(int position) -> bool override;
    auto isMoving() const -> bool override;
    auto abortMotion() -> bool override;

    // Filter information (delegated to FilterController)
    auto getSlotName(int slot) -> std::optional<std::string> override;
    auto setSlotName(int slot, const std::string& name) -> bool override;
    auto getAllSlotNames() -> std::vector<std::string> override;
    auto getCurrentFilterName() -> std::string override;

    // Enhanced filter management
    auto getFilterInfo(int slot) -> std::optional<FilterInfo> override;
    auto setFilterInfo(int slot, const FilterInfo& info) -> bool override;
    auto getAllFilterInfo() -> std::vector<FilterInfo> override;

    // Filter search and selection
    auto findFilterByName(const std::string& name) -> std::optional<int> override;
    auto findFilterByType(const std::string& type) -> std::vector<int> override;
    auto selectFilterByName(const std::string& name) -> bool override;
    auto selectFilterByType(const std::string& type) -> bool override;

    // Motion control
    auto homeFilterWheel() -> bool override;
    auto calibrateFilterWheel() -> bool override;

    // Temperature (if supported)
    auto getTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Statistics (delegated to StatisticsManager)
    auto getTotalMoves() -> uint64_t override;
    auto resetTotalMoves() -> bool override;
    auto getLastMoveTime() -> int override;

    // Configuration presets (delegated to ConfigurationManager)
    auto saveFilterConfiguration(const std::string& name) -> bool override;
    auto loadFilterConfiguration(const std::string& name) -> bool override;
    auto deleteFilterConfiguration(const std::string& name) -> bool override;
    auto getAvailableConfigurations() -> std::vector<std::string> override;

    // Advanced profiling and performance monitoring
    auto getPerformanceStats() -> FilterPerformanceStats;
    auto predictMoveDuration(int fromSlot, int toSlot) -> std::chrono::milliseconds;
    auto hasPerformanceDegraded() -> bool;
    auto getOptimizationRecommendations() -> std::vector<std::string>;
    auto exportProfilingData(const std::string& filePath) -> bool;
    auto setProfiling(bool enabled) -> void;
    auto isProfilingEnabled() -> bool;

    // Component access for advanced usage
    PropertyManager& getPropertyManager() { return *propertyManager_; }
    FilterController& getFilterController() { return *filterController_; }
    StatisticsManager& getStatisticsManager() { return *statisticsManager_; }
    TemperatureManager& getTemperatureManager() { return *temperatureManager_; }
    ConfigurationManager& getConfigurationManager() { return *configurationManager_; }
    FilterWheelProfiler& getProfiler() { return *profiler_; }

protected:
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;

private:
    // Shared core
    std::shared_ptr<INDIFilterWheelCore> core_;

    // Component managers
    std::unique_ptr<PropertyManager> propertyManager_;
    std::unique_ptr<FilterController> filterController_;
    std::unique_ptr<StatisticsManager> statisticsManager_;
    std::unique_ptr<TemperatureManager> temperatureManager_;
    std::unique_ptr<ConfigurationManager> configurationManager_;
    std::unique_ptr<FilterWheelProfiler> profiler_;

    // Component initialization
    bool initializeComponents();
    void cleanupComponents();

    // Device connection helpers
    void setupDeviceWatchers();
    void setupInitialConnection(const std::string& deviceName);
};

}  // namespace lithium::device::indi::filterwheel

#endif  // LITHIUM_INDI_FILTERWHEEL_MODULAR_FILTERWHEEL_HPP
