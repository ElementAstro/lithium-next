#ifndef LITHIUM_TASK_GUIDE_AUTO_CONFIG_HPP
#define LITHIUM_TASK_GUIDE_AUTO_CONFIG_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Automated guide configuration optimization task.
 * Automatically optimizes PHD2 settings based on current conditions.
 */
class AutoGuideConfigTask : public Task {
public:
    AutoGuideConfigTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void optimizeConfiguration(const json& params);
    void analyzeCurrentPerformance();
    void adjustExposureTime();
    void optimizeAlgorithmParameters();
    void configureDitherSettings();

    struct SystemAnalysis {
        double current_rms;
        double star_brightness;
        double noise_level;
        int dropped_frames;
        bool is_stable;
    };

    SystemAnalysis current_analysis_;
};

/**
 * @brief Profile management task.
 * Manages different guide profiles for different equipment/conditions.
 */
class GuideProfileManagerTask : public Task {
public:
    GuideProfileManagerTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void manageProfile(const json& params);
    void saveCurrentProfile(const std::string& name);
    void loadProfile(const std::string& name);
    void listProfiles();
    void deleteProfile(const std::string& name);

    std::string getProfilePath(const std::string& name);
};

/**
 * @brief Intelligent weather-based configuration task.
 * Adjusts guide settings based on atmospheric conditions.
 */
class WeatherAdaptiveConfigTask : public Task {
public:
    WeatherAdaptiveConfigTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void adaptToWeatherConditions(const json& params);
    void analyzeSeeing(double seeing_arcsec);
    void adjustForWind(double wind_speed);
    void compensateForTemperature(double temperature);

    struct WeatherData {
        double seeing_arcsec;
        double wind_speed_ms;
        double temperature_c;
        double humidity_percent;
        double pressure_hpa;
    };
};

/**
 * @brief Equipment-specific auto-tuning task.
 * Automatically tunes settings for specific telescope/mount combinations.
 */
class EquipmentAutoTuneTask : public Task {
public:
    EquipmentAutoTuneTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performAutoTune(const json& params);
    void detectEquipmentType();
    void calibrateForFocalLength(double focal_length_mm);
    void optimizeForMount(const std::string& mount_type);
    void tuneForCamera(const std::string& camera_type);

    struct EquipmentProfile {
        std::string telescope_model;
        double focal_length_mm;
        double aperture_mm;
        std::string mount_model;
        std::string camera_model;
        double pixel_size_um;
    };

    EquipmentProfile detected_equipment_;
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_AUTO_CONFIG_HPP
