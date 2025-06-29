#ifndef LITHIUM_TASK_ADVANCED_WEATHER_MONITOR_TASK_HPP
#define LITHIUM_TASK_ADVANCED_WEATHER_MONITOR_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Weather Monitoring and Response Task
 *
 * Continuously monitors weather conditions and takes appropriate actions
 * such as closing equipment, pausing sequences, or parking telescopes.
 */
class WeatherMonitorTask : public Task {
public:
    WeatherMonitorTask()
        : Task("WeatherMonitor",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "WeatherMonitor"; }

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateWeatherMonitorParameters(const json& params);

private:
    void executeImpl(const json& params);
    json getCurrentWeatherData();
    bool evaluateWeatherConditions(const json& weather, const json& limits);
    void handleUnsafeWeather();
    void handleSafeWeather();
    void sendWeatherAlert(const std::string& message);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_WEATHER_MONITOR_TASK_HPP
