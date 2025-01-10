#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct ServerConfigData {
    static inline const fs::path PHD2_HIDDEN_CONFIG_FILE =
        "./phd2_hidden_config.json";
    static inline const fs::path DEFAULT_PHD2_CONFIG_FILE =
        "./default_phd2_config.json";
    static inline const fs::path PROFILE_SAVE_PATH = "./server/data/phd2";
};

/**
 * @struct InterfacePHD2Profile
 * @brief Structure representing a PHD2 profile.
 */
struct InterfacePHD2Profile {
    std::string name;             ///< Profile name
    std::string camera;           ///< Camera name
    std::string cameraCCD;        ///< Camera CCD name
    double pixelSize;             ///< Pixel size in microns
    std::string telescope;        ///< Telescope name
    double focalLength;           ///< Focal length in millimeters
    double massChangeThreshold;   ///< Mass change threshold
    bool massChangeFlag;          ///< Mass change flag
    double calibrationDistance;   ///< Calibration distance in arcseconds
    double calibrationDuration;   ///< Calibration duration in seconds
} __attribute__((aligned(128)));  // Align to 128 bytes

/**
 * @class PHD2ProfileSettingHandler
 * @brief Class to handle PHD2 profile settings.
 */
class PHD2ProfileSettingHandler {
public:
    /**
     * @brief Constructs a new PHD2ProfileSettingHandler object.
     */
    PHD2ProfileSettingHandler();

    /**
     * @brief Destroys the PHD2ProfileSettingHandler object.
     */
    ~PHD2ProfileSettingHandler();

    // Disable copy operations
    PHD2ProfileSettingHandler(const PHD2ProfileSettingHandler&) = delete;
    auto operator=(const PHD2ProfileSettingHandler&)
        -> PHD2ProfileSettingHandler& = delete;

    // Enable move operations
    PHD2ProfileSettingHandler(PHD2ProfileSettingHandler&&) noexcept;
    auto operator=(PHD2ProfileSettingHandler&&) noexcept
        -> PHD2ProfileSettingHandler&;

    /**
     * @brief Loads the profile file.
     * @return An optional InterfacePHD2Profile object.
     */
    [[nodiscard]] auto loadProfileFile() -> std::optional<InterfacePHD2Profile>;

    /**
     * @brief Loads a profile by name.
     * @param profileName The name of the profile to load.
     * @return True if the profile was loaded successfully, false otherwise.
     */
    auto loadProfile(const std::string& profileName) -> bool;

    /**
     * @brief Creates a new profile setting.
     * @param newProfileName The name of the new profile.
     * @return True if the profile was created successfully, false otherwise.
     */
    auto newProfileSetting(const std::string& newProfileName) -> bool;

    /**
     * @brief Updates an existing profile.
     * @param phd2ProfileSetting The profile settings to update.
     * @return True if the profile was updated successfully, false otherwise.
     */
    auto updateProfile(const InterfacePHD2Profile& phd2ProfileSetting) -> bool;

    /**
     * @brief Deletes a profile by name.
     * @param toDeleteProfile The name of the profile to delete.
     * @return True if the profile was deleted successfully, false otherwise.
     */
    auto deleteProfile(const std::string& toDeleteProfile) -> bool;

    /**
     * @brief Saves the current profile.
     * @param profileName The name of the profile to save.
     */
    void saveProfile(const std::string& profileName);

    /**
     * @brief Restores a profile by name.
     * @param toRestoreProfile The name of the profile to restore.
     * @return True if the profile was restored successfully, false otherwise.
     */
    auto restoreProfile(const std::string& toRestoreProfile) -> bool;

    /**
     * @brief Lists all available profiles.
     * @return A vector of profile names.
     */
    [[nodiscard]] auto listProfiles() const -> std::vector<std::string>;

    /**
     * @brief Exports a profile to a specified path.
     * @param profileName The name of the profile to export.
     * @param exportPath The path to export the profile to.
     * @return True if the profile was exported successfully, false otherwise.
     */
    [[nodiscard]] auto exportProfile(
        const std::string& profileName,
        const std::filesystem::path& exportPath) const -> bool;

    /**
     * @brief Imports a profile from a specified path.
     * @param importPath The path to import the profile from.
     * @param newProfileName The name of the new profile.
     * @return True if the profile was imported successfully, false otherwise.
     */
    auto importProfile(const std::filesystem::path& importPath,
                       const std::string& newProfileName) -> bool;

    /**
     * @brief Compares two profiles by name.
     * @param profile1 The name of the first profile.
     * @param profile2 The name of the second profile.
     * @return True if the profiles are identical, false otherwise.
     */
    [[nodiscard]] auto compareProfiles(
        const std::string& profile1, const std::string& profile2) const -> bool;

    /**
     * @brief Prints the details of a profile.
     * @param profileName The name of the profile to print.
     */
    void printProfileDetails(const std::string& profileName) const;

    /**
     * @brief Validates a profile by name.
     * @param profileName The name of the profile to validate.
     * @return True if the profile is valid, false otherwise.
     */
    [[nodiscard]] auto validateProfile(const std::string& profileName) const
        -> bool;

    /**
     * @brief Validates all profiles.
     * @return A vector of invalid profile names.
     */
    [[nodiscard]] auto validateAllProfiles() const -> std::vector<std::string>;

    /**
     * @brief Batch exports profiles to a specified directory.
     * @param profileNames The names of the profiles to export.
     * @param exportDir The directory to export the profiles to.
     * @return True if the profiles were exported successfully, false otherwise.
     */
    auto batchExportProfiles(const std::vector<std::string>& profileNames,
                             const std::filesystem::path& exportDir) const
        -> bool;

    /**
     * @brief Batch imports profiles from a specified directory.
     * @param importDir The directory to import the profiles from.
     * @return The number of profiles imported successfully.
     */
    auto batchImportProfiles(const std::filesystem::path& importDir) -> size_t;

    /**
     * @brief Batch deletes profiles by name.
     * @param profileNames The names of the profiles to delete.
     * @return The number of profiles deleted successfully.
     */
    auto batchDeleteProfiles(const std::vector<std::string>& profileNames)
        -> size_t;

    /**
     * @brief Creates a backup of a profile.
     * @param profileName The name of the profile to backup.
     * @return True if the backup was created successfully, false otherwise.
     */
    auto createBackup(const std::string& profileName) -> bool;

    /**
     * @brief Restores a profile from a backup.
     * @param profileName The name of the profile to restore.
     * @return True if the profile was restored successfully, false otherwise.
     */
    auto restoreFromBackup(const std::string& profileName) -> bool;

    /**
     * @brief Lists all backups for a profile.
     * @param profileName The name of the profile.
     * @return A vector of backup names.
     */
    auto listBackups(const std::string& profileName) const
        -> std::vector<std::string>;

    /**
     * @brief Clears the profile cache.
     */
    void clearCache();

    /**
     * @brief Preloads profiles into the cache.
     */
    void preloadProfiles();

    /**
     * @brief Gets the settings of a profile by name.
     * @param profileName The name of the profile.
     * @return An optional InterfacePHD2Profile object.
     */
    [[nodiscard]] auto getProfileSettings(const std::string& profileName) const
        -> std::optional<InterfacePHD2Profile>;

    /**
     * @brief Finds profiles by camera model.
     * @param cameraModel The camera model to search for.
     * @return A vector of profile names.
     */
    [[nodiscard]] auto findProfilesByCamera(
        const std::string& cameraModel) const -> std::vector<std::string>;

    /**
     * @brief Finds profiles by telescope model.
     * @param telescopeModel The telescope model to search for.
     * @return A vector of profile names.
     */
    [[nodiscard]] auto findProfilesByTelescope(
        const std::string& telescopeModel) const -> std::vector<std::string>;

private:
    class Impl;  ///< Forward declaration of the implementation class.
    std::unique_ptr<Impl> pImpl;  ///< Pointer to the implementation.
};