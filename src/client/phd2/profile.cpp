#include "profile.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"

using json = nlohmann::json;

class PHD2ProfileSettingHandler::Impl {
public:
    std::optional<InterfacePHD2Profile> loadedConfigStatus;
    const fs::path PHD2_PROFILE_SAVE_PATH = ServerConfigData::PROFILE_SAVE_PATH;

    // Profile cache storage
    std::unordered_map<std::string, json> profileCache;
    std::unordered_map<std::string, std::chrono::system_clock::time_point>
        cacheTimestamps;

    static constexpr auto CACHE_VALIDITY_DURATION = std::chrono::minutes(5);
    static constexpr auto MAX_CACHE_SIZE = 100;

    static void replaceDoubleMarker(const fs::path& filePath) {
        std::ifstream inputFile(filePath, std::ios::binary);
        if (!inputFile.is_open()) {
            spdlog::error("Failed to open file for reading: {}",
                          filePath.string());
            THROW_FAIL_TO_OPEN_FILE("Failed to open file for reading.");
        }

        std::string content((std::istreambuf_iterator<char>(inputFile)),
                            std::istreambuf_iterator<char>());
        inputFile.close();

        size_t pos = content.find("\"\"#");
        while (pos != std::string::npos) {
            content.replace(pos, 3, "#");
            pos = content.find("\"\"#", pos + 1);
        }

        std::ofstream outputFile(filePath, std::ios::binary);
        if (!outputFile.is_open()) {
            spdlog::error("Failed to open file for writing: {}",
                          filePath.string());
            THROW_FAIL_TO_OPEN_FILE("Failed to open file for writing.");
        }

        outputFile << content;
    }

    [[nodiscard]] static auto loadJsonFile(const fs::path& filePath) -> json {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to open JSON file: {}", filePath.string());
            THROW_FAIL_TO_OPEN_FILE("Failed to open JSON file.");
        }

        json config;
        try {
            file >> config;
        } catch (const json::parse_error& e) {
            spdlog::error("JSON parsing error in file {}: {}",
                          filePath.string(), e.what());
            throw;
        }
        return config;
    }

    void updateCache(const std::string& profileName, const json& config) {
        if (profileCache.size() >= MAX_CACHE_SIZE) {
            // Remove oldest cache entry
            auto oldest =
                std::min_element(cacheTimestamps.begin(), cacheTimestamps.end(),
                                 [](const auto& a, const auto& b) {
                                     return a.second < b.second;
                                 });
            profileCache.erase(oldest->first);
            cacheTimestamps.erase(oldest->first);
        }
        profileCache[profileName] = config;
        cacheTimestamps[profileName] = std::chrono::system_clock::now();
    }

    [[nodiscard]] auto getCachedProfile(const std::string& profileName)
        -> std::optional<json> {
        auto it = profileCache.find(profileName);
        if (it != profileCache.end()) {
            auto now = std::chrono::system_clock::now();
            if (now - cacheTimestamps[profileName] < CACHE_VALIDITY_DURATION) {
                return it->second;
            }
            profileCache.erase(it);
            cacheTimestamps.erase(profileName);
        }
        return std::nullopt;
    }

    static void saveJsonFile(const fs::path& filePath, const json& config) {
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to open JSON file for writing: {}",
                          filePath.string());
            THROW_FAIL_TO_OPEN_FILE("Failed to open JSON file for writing.");
        }

        try {
            file << config.dump(4);
        } catch (const std::exception& e) {
            spdlog::error("Error writing JSON to file {}: {}",
                          filePath.string(), e.what());
            throw;
        }
        replaceDoubleMarker(filePath);
    }
};

PHD2ProfileSettingHandler::PHD2ProfileSettingHandler()
    : pImpl(std::make_unique<Impl>()) {
    spdlog::info("PHD2ProfileSettingHandler initialized.");
}

PHD2ProfileSettingHandler::~PHD2ProfileSettingHandler() = default;

PHD2ProfileSettingHandler::PHD2ProfileSettingHandler(
    PHD2ProfileSettingHandler&&) noexcept = default;

auto PHD2ProfileSettingHandler::operator=(PHD2ProfileSettingHandler&&) noexcept
    -> PHD2ProfileSettingHandler& = default;

auto PHD2ProfileSettingHandler::loadProfileFile()
    -> std::optional<InterfacePHD2Profile> {
    spdlog::info("Loading profile file.");
    if (!fs::exists(ServerConfigData::PHD2_HIDDEN_CONFIG_FILE)) {
        spdlog::warn(
            "Hidden config file does not exist. Copying default config.");
        fs::copy_file(ServerConfigData::DEFAULT_PHD2_CONFIG_FILE,
                      ServerConfigData::PHD2_HIDDEN_CONFIG_FILE,
                      fs::copy_options::overwrite_existing);
    }

    try {
        json phd2Config =
            pImpl->loadJsonFile(ServerConfigData::PHD2_HIDDEN_CONFIG_FILE);
        pImpl->loadedConfigStatus = InterfacePHD2Profile{
            .name =
                phd2Config.at("profile").at("1").at("name").get<std::string>(),
            .camera = phd2Config.at("profile")
                          .at("1")
                          .at("indi")
                          .at("INDIcam")
                          .get<std::string>(),
            .cameraCCD = phd2Config.at("profile")
                             .at("1")
                             .at("indi")
                             .at("INDIcam_ccd")
                             .get<std::string>(),
            .pixelSize = phd2Config.at("profile")
                             .at("1")
                             .at("camera")
                             .at("pixelsize")
                             .get<double>(),
            .telescope = phd2Config.at("profile")
                             .at("1")
                             .at("indi")
                             .at("INDImount")
                             .get<std::string>(),
            .focalLength = phd2Config.at("profile")
                               .at("1")
                               .at("frame")
                               .at("focalLength")
                               .get<double>(),
            .massChangeThreshold = phd2Config.at("profile")
                                       .at("1")
                                       .at("guider")
                                       .at("onestar")
                                       .at("MassChangeThreshold")
                                       .get<double>(),
            .massChangeFlag = phd2Config.at("profile")
                                  .at("1")
                                  .at("guider")
                                  .at("onestar")
                                  .at("MassChangeThresholdEnabled")
                                  .get<bool>(),
            .calibrationDistance = phd2Config.at("profile")
                                       .at("1")
                                       .at("scope")
                                       .at("CalibrationDistance")
                                       .get<double>(),
            .calibrationDuration = phd2Config.at("profile")
                                       .at("1")
                                       .at("scope")
                                       .at("CalibrationDuration")
                                       .get<double>()};
        spdlog::info("Profile file loaded successfully.");
    } catch (const json::exception& e) {
        spdlog::error("JSON parsing error: {}", e.what());
        fs::remove(ServerConfigData::PHD2_HIDDEN_CONFIG_FILE);
        fs::copy_file(ServerConfigData::DEFAULT_PHD2_CONFIG_FILE,
                      ServerConfigData::PHD2_HIDDEN_CONFIG_FILE,
                      fs::copy_options::overwrite_existing);
        return loadProfileFile();  // Recursive call with default config
    }

    return pImpl->loadedConfigStatus;
}

auto PHD2ProfileSettingHandler::loadProfile(const std::string& profileName)
    -> bool {
    spdlog::info("Loading profile: {}", profileName);
    fs::path profileFile =
        pImpl->PHD2_PROFILE_SAVE_PATH / (profileName + ".json");

    if (fs::exists(profileFile)) {
        fs::copy_file(profileFile, ServerConfigData::PHD2_HIDDEN_CONFIG_FILE,
                      fs::copy_options::overwrite_existing);
        try {
            auto result = loadProfileFile();  // Store the return value
            spdlog::info("Profile {} loaded successfully.", profileName);
            return result
                .has_value();  // Return true if profile loaded successfully
        } catch (const std::exception& e) {
            spdlog::error("Failed to load profile {}: {}", profileName,
                          e.what());
            return false;
        }
    } else {
        spdlog::warn("Profile {} does not exist. Loading default profile.",
                     profileName);
        fs::copy_file(ServerConfigData::DEFAULT_PHD2_CONFIG_FILE,
                      ServerConfigData::PHD2_HIDDEN_CONFIG_FILE,
                      fs::copy_options::overwrite_existing);
        auto result = loadProfileFile();  // Store the return value
        return result
            .has_value();  // Return true if default profile loaded successfully
    }
}

auto PHD2ProfileSettingHandler::newProfileSetting(
    const std::string& newProfileName) -> bool {
    spdlog::info("Creating new profile: {}", newProfileName);
    fs::path newProfileFile =
        pImpl->PHD2_PROFILE_SAVE_PATH / (newProfileName + ".json");

    if (fs::exists(newProfileFile)) {
        spdlog::warn("Profile {} already exists. Restoring existing profile.",
                     newProfileName);
        return restoreProfile(newProfileName);
    }

    // Create directory if it doesn't exist
    if (!fs::exists(pImpl->PHD2_PROFILE_SAVE_PATH)) {
        try {
            fs::create_directories(pImpl->PHD2_PROFILE_SAVE_PATH);
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to create directory: {}", e.what());
            return false;
        }
    }

    fs::copy_file(ServerConfigData::DEFAULT_PHD2_CONFIG_FILE,
                  ServerConfigData::PHD2_HIDDEN_CONFIG_FILE,
                  fs::copy_options::overwrite_existing);
    auto result = loadProfileFile();  // Store the return value
    if (!result.has_value()) {
        return false;
    }
    saveProfile(newProfileName);
    spdlog::info("New profile {} created successfully.", newProfileName);
    return true;
}

auto PHD2ProfileSettingHandler::updateProfile(
    const InterfacePHD2Profile& phd2ProfileSetting) -> bool {
    spdlog::info("Updating profile: {}", phd2ProfileSetting.name);
    json phd2Config =
        pImpl->loadJsonFile(ServerConfigData::PHD2_HIDDEN_CONFIG_FILE);

    try {
        phd2Config["profile"]["1"]["name"] = phd2ProfileSetting.name;
        phd2Config["profile"]["1"]["indi"]["INDIcam"] =
            phd2ProfileSetting.camera;
        phd2Config["profile"]["1"]["indi"]["INDIcam_ccd"] =
            phd2ProfileSetting.cameraCCD;
        phd2Config["profile"]["1"]["camera"]["pixelsize"] =
            phd2ProfileSetting.pixelSize;
        phd2Config["profile"]["1"]["indi"]["INDImount"] =
            phd2ProfileSetting.telescope;
        phd2Config["profile"]["1"]["frame"]["focalLength"] =
            phd2ProfileSetting.focalLength;
        phd2Config["profile"]["1"]["guider"]["onestar"]["MassChangeThreshold"] =
            phd2ProfileSetting.massChangeThreshold;
        phd2Config["profile"]["1"]["guider"]["onestar"]
                  ["MassChangeThresholdEnabled"] =
                      phd2ProfileSetting.massChangeFlag;
        phd2Config["profile"]["1"]["scope"]["CalibrationDistance"] =
            phd2ProfileSetting.calibrationDistance;
        phd2Config["profile"]["1"]["scope"]["CalibrationDuration"] =
            phd2ProfileSetting.calibrationDuration;
    } catch (const json::exception& e) {
        spdlog::error("Error updating profile: {}", e.what());
        throw std::runtime_error("Error updating profile: " +
                                 std::string(e.what()));
    }

    pImpl->saveJsonFile(ServerConfigData::PHD2_HIDDEN_CONFIG_FILE, phd2Config);
    spdlog::info("Profile {} updated successfully.", phd2ProfileSetting.name);
    return true;
}

auto PHD2ProfileSettingHandler::deleteProfile(
    const std::string& toDeleteProfile) -> bool {
    spdlog::info("Deleting profile: {}", toDeleteProfile);
    fs::path toDeleteProfileFile =
        pImpl->PHD2_PROFILE_SAVE_PATH / (toDeleteProfile + ".json");
    if (fs::exists(toDeleteProfileFile)) {
        try {
            fs::remove(toDeleteProfileFile);

            // Also remove from cache if present
            auto it = pImpl->profileCache.find(toDeleteProfile);
            if (it != pImpl->profileCache.end()) {
                pImpl->profileCache.erase(it);
                pImpl->cacheTimestamps.erase(toDeleteProfile);
            }

            spdlog::info("Profile {} deleted successfully.", toDeleteProfile);
            return true;
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to delete profile {}: {}", toDeleteProfile,
                          e.what());
            return false;
        }
    }
    spdlog::warn("Profile {} does not exist.", toDeleteProfile);
    return false;
}

void PHD2ProfileSettingHandler::saveProfile(const std::string& profileName) {
    spdlog::info("Saving current profile as: {}", profileName);

    // Create directory structure if it doesn't exist
    if (!fs::exists(pImpl->PHD2_PROFILE_SAVE_PATH)) {
        try {
            fs::create_directories(pImpl->PHD2_PROFILE_SAVE_PATH);
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to create directory structure: {}", e.what());
            throw std::runtime_error("Failed to create directory structure: " +
                                     std::string(e.what()));
        }
    }

    fs::path profileFile =
        pImpl->PHD2_PROFILE_SAVE_PATH / (profileName + ".json");

    if (fs::exists(ServerConfigData::PHD2_HIDDEN_CONFIG_FILE)) {
        try {
            if (fs::exists(profileFile)) {
                fs::remove(profileFile);
                spdlog::info("Existing profile file {} removed.",
                             profileFile.string());
            }
            fs::copy_file(ServerConfigData::PHD2_HIDDEN_CONFIG_FILE,
                          profileFile, fs::copy_options::overwrite_existing);

            // Cache the profile data
            json configData = pImpl->loadJsonFile(profileFile);
            pImpl->updateCache(profileName, configData);

            spdlog::info("Profile saved successfully as {}.", profileName);
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to save profile {}: {}", profileName,
                          e.what());
            throw std::runtime_error("Failed to save profile: " +
                                     std::string(e.what()));
        }
    } else {
        spdlog::error(
            "Hidden config file does not exist. Cannot save profile.");
        throw std::runtime_error("Hidden config file does not exist.");
    }
}

auto PHD2ProfileSettingHandler::restoreProfile(
    const std::string& toRestoreProfile) -> bool {
    spdlog::info("Restoring profile: {}", toRestoreProfile);
    fs::path toRestoreFile =
        pImpl->PHD2_PROFILE_SAVE_PATH / (toRestoreProfile + ".json");
    if (fs::exists(toRestoreFile)) {
        try {
            // Check cache first
            auto cachedProfile = pImpl->getCachedProfile(toRestoreProfile);
            if (cachedProfile) {
                pImpl->saveJsonFile(ServerConfigData::PHD2_HIDDEN_CONFIG_FILE,
                                    *cachedProfile);
                auto result = loadProfileFile();
                if (!result.has_value()) {
                    spdlog::error("Failed to load profile from cache");
                    return false;
                }
                spdlog::info("Profile {} restored from cache.",
                             toRestoreProfile);
                return true;
            }

            fs::copy_file(toRestoreFile,
                          ServerConfigData::PHD2_HIDDEN_CONFIG_FILE,
                          fs::copy_options::overwrite_existing);
            auto result = loadProfileFile();
            if (!result.has_value()) {
                spdlog::error("Failed to load profile from file");
                return false;
            }

            // Update cache with the loaded profile
            json configData = pImpl->loadJsonFile(toRestoreFile);
            pImpl->updateCache(toRestoreProfile, configData);

            spdlog::info("Profile {} restored successfully.", toRestoreProfile);
            return true;
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to restore profile {}: {}", toRestoreProfile,
                          e.what());
            return false;
        }
    } else {
        spdlog::warn("Profile {} does not exist. Creating new profile.",
                     toRestoreProfile);
        return newProfileSetting(toRestoreProfile);
    }
}

auto PHD2ProfileSettingHandler::listProfiles() const
    -> std::vector<std::string> {
    spdlog::info("Listing all profiles.");
    std::vector<std::string> profiles;

    try {
        // Create directory if it doesn't exist
        if (!fs::exists(pImpl->PHD2_PROFILE_SAVE_PATH)) {
            fs::create_directories(pImpl->PHD2_PROFILE_SAVE_PATH);
        }

        for (const auto& entry :
             fs::directory_iterator(pImpl->PHD2_PROFILE_SAVE_PATH)) {
            if (entry.path().extension() == ".json") {
                profiles.push_back(entry.path().stem().string());
            }
        }
        spdlog::info("Found {} profiles.", profiles.size());
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Error listing profiles: {}", e.what());
        throw std::runtime_error("Error listing profiles: " +
                                 std::string(e.what()));
    }
    return profiles;
}

auto PHD2ProfileSettingHandler::exportProfile(const std::string& profileName,
                                              const fs::path& exportPath) const
    -> bool {
    spdlog::info("Exporting profile {} to {}", profileName,
                 exportPath.string());
    fs::path sourceFile =
        pImpl->PHD2_PROFILE_SAVE_PATH / (profileName + ".json");
    if (fs::exists(sourceFile)) {
        try {
            // Create parent directories if they don't exist
            if (exportPath.has_parent_path() &&
                !fs::exists(exportPath.parent_path())) {
                fs::create_directories(exportPath.parent_path());
            }

            fs::copy_file(sourceFile, exportPath,
                          fs::copy_options::overwrite_existing);
            spdlog::info("Profile {} exported successfully to {}.", profileName,
                         exportPath.string());
            return true;
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to export profile {}: {}", profileName,
                          e.what());
            return false;
        }
    }
    spdlog::warn("Profile {} does not exist. Cannot export.", profileName);
    return false;
}

auto PHD2ProfileSettingHandler::importProfile(const fs::path& importPath,
                                              const std::string& newProfileName)
    -> bool {
    spdlog::info("Importing profile from {} as {}", importPath.string(),
                 newProfileName);
    if (fs::exists(importPath)) {
        fs::path destinationFile =
            pImpl->PHD2_PROFILE_SAVE_PATH / (newProfileName + ".json");

        try {
            // Create directory structure if it doesn't exist
            if (!fs::exists(pImpl->PHD2_PROFILE_SAVE_PATH)) {
                fs::create_directories(pImpl->PHD2_PROFILE_SAVE_PATH);
            }

            // Validate the imported JSON file
            json importedConfig = pImpl->loadJsonFile(importPath);

            // Basic validation
            if (!importedConfig.contains("profile") ||
                !importedConfig["profile"].contains("1")) {
                spdlog::error("Invalid profile format in the imported file.");
                return false;
            }

            fs::copy_file(importPath, destinationFile,
                          fs::copy_options::overwrite_existing);

            // Update cache with the imported profile
            pImpl->updateCache(newProfileName, importedConfig);

            spdlog::info("Profile imported successfully as {}.",
                         newProfileName);
            return true;
        } catch (const json::exception& e) {
            spdlog::error("Invalid JSON in imported profile: {}", e.what());
            return false;
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to import profile as {}: {}", newProfileName,
                          e.what());
            return false;
        }
    }
    spdlog::warn("Import path {} does not exist. Cannot import profile.",
                 importPath.string());
    return false;
}

auto PHD2ProfileSettingHandler::compareProfiles(
    const std::string& profile1, const std::string& profile2) const -> bool {
    spdlog::info("Comparing profiles: {} and {}", profile1, profile2);
    fs::path file1 = pImpl->PHD2_PROFILE_SAVE_PATH / (profile1 + ".json");
    fs::path file2 = pImpl->PHD2_PROFILE_SAVE_PATH / (profile2 + ".json");

    if (!fs::exists(file1) || !fs::exists(file2)) {
        spdlog::error("One or both profiles do not exist.");
        return false;
    }

    try {
        // Try cache first
        auto cachedProfile1 = pImpl->getCachedProfile(profile1);
        auto cachedProfile2 = pImpl->getCachedProfile(profile2);

        json config1, config2;

        if (cachedProfile1) {
            config1 = *cachedProfile1;
        } else {
            config1 = pImpl->loadJsonFile(file1);
        }

        if (cachedProfile2) {
            config2 = *cachedProfile2;
        } else {
            config2 = pImpl->loadJsonFile(file2);
        }

        bool areEqual = (config1 == config2);
        if (areEqual) {
            spdlog::info("Profiles {} and {} are identical.", profile1,
                         profile2);
        } else {
            spdlog::info("Profiles {} and {} have differences.", profile1,
                         profile2);
        }
        return areEqual;
    } catch (const std::exception& e) {
        spdlog::error("Error comparing profiles: {}", e.what());
        return false;
    }
}

void PHD2ProfileSettingHandler::printProfileDetails(
    const std::string& profileName) const {
    spdlog::info("Printing details of profile: {}", profileName);
    fs::path profileFile =
        pImpl->PHD2_PROFILE_SAVE_PATH / (profileName + ".json");
    if (fs::exists(profileFile)) {
        try {
            // Check cache first
            auto cachedProfile = pImpl->getCachedProfile(profileName);
            json config;

            if (cachedProfile) {
                config = *cachedProfile;
            } else {
                config = pImpl->loadJsonFile(profileFile);
            }

            std::cout << "Profile: " << profileName << std::endl;
            std::cout << "Details:" << std::endl;
            std::cout << config.dump(4) << std::endl;
            spdlog::info("Profile details printed successfully.");
        } catch (const std::exception& e) {
            spdlog::error("Failed to print profile details: {}", e.what());
            throw std::runtime_error("Failed to print profile details: " +
                                     std::string(e.what()));
        }
    } else {
        spdlog::warn("Profile {} does not exist.", profileName);
        std::cout << "Profile " << profileName << " does not exist."
                  << std::endl;
    }
}

auto PHD2ProfileSettingHandler::validateProfile(
    const std::string& profileName) const -> bool {
    try {
        fs::path profilePath =
            pImpl->PHD2_PROFILE_SAVE_PATH / (profileName + ".json");
        if (!fs::exists(profilePath)) {
            spdlog::error("Profile {} does not exist", profileName);
            return false;
        }

        // Check cache first
        auto cachedProfile = pImpl->getCachedProfile(profileName);
        json config;

        if (cachedProfile) {
            config = *cachedProfile;
        } else {
            config = pImpl->loadJsonFile(profilePath);
        }

        // Validate required fields
        const std::vector<std::string> requiredFields = {
            "/profile/1/name", "/profile/1/camera/pixelsize",
            "/profile/1/frame/focalLength"};

        for (const auto& field : requiredFields) {
            if (!config.contains(json::json_pointer(field))) {
                spdlog::error("Missing required field: {} in profile {}", field,
                              profileName);
                return false;
            }
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error validating profile {}: {}", profileName, e.what());
        return false;
    }
}

auto PHD2ProfileSettingHandler::validateAllProfiles() const
    -> std::vector<std::string> {
    spdlog::info("Validating all profiles");
    std::vector<std::string> invalidProfiles;

    try {
        auto profiles = listProfiles();
        for (const auto& profile : profiles) {
            if (!validateProfile(profile)) {
                invalidProfiles.push_back(profile);
            }
        }

        if (invalidProfiles.empty()) {
            spdlog::info("All profiles are valid");
        } else {
            spdlog::warn("Found {} invalid profiles", invalidProfiles.size());
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during profile validation: {}", e.what());
    }

    return invalidProfiles;
}

auto PHD2ProfileSettingHandler::batchExportProfiles(
    const std::vector<std::string>& profileNames,
    const fs::path& exportDir) const -> bool {
    if (!fs::exists(exportDir)) {
        try {
            fs::create_directories(exportDir);
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to create export directory: {}", e.what());
            return false;
        }
    }

    bool allSuccess = true;
    for (const auto& profile : profileNames) {
        if (!exportProfile(profile, exportDir / (profile + ".json"))) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

auto PHD2ProfileSettingHandler::batchImportProfiles(const fs::path& importDir)
    -> size_t {
    spdlog::info("Batch importing profiles from {}", importDir.string());
    size_t successCount = 0;

    if (!fs::exists(importDir)) {
        spdlog::error("Import directory does not exist");
        return 0;
    }

    try {
        for (const auto& entry : fs::directory_iterator(importDir)) {
            if (entry.path().extension() == ".json") {
                std::string profileName = entry.path().stem().string();
                if (importProfile(entry.path(), profileName)) {
                    successCount++;
                }
            }
        }
        spdlog::info("Successfully imported {} profiles", successCount);
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Error during batch import: {}", e.what());
    }

    return successCount;
}

auto PHD2ProfileSettingHandler::batchDeleteProfiles(
    const std::vector<std::string>& profileNames) -> size_t {
    spdlog::info("Batch deleting {} profiles", profileNames.size());
    size_t deletedCount = 0;

    for (const auto& profile : profileNames) {
        if (deleteProfile(profile)) {
            deletedCount++;
        }
    }

    spdlog::info("Successfully deleted {} profiles", deletedCount);
    return deletedCount;
}

auto PHD2ProfileSettingHandler::createBackup(const std::string& profileName)
    -> bool {
    spdlog::info("Creating backup of profile: {}", profileName);

    fs::path profilePath =
        pImpl->PHD2_PROFILE_SAVE_PATH / (profileName + ".json");
    if (!fs::exists(profilePath)) {
        spdlog::error("Profile {} does not exist", profileName);
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();

    fs::path backupDir =
        pImpl->PHD2_PROFILE_SAVE_PATH / "backups" / profileName;
    fs::path backupPath =
        backupDir / (profileName + "_" + std::to_string(timestamp) + ".json");

    try {
        if (!fs::exists(backupDir)) {
            fs::create_directories(backupDir);
        }

        fs::copy_file(profilePath, backupPath);
        spdlog::info("Backup created at {}", backupPath.string());
        return true;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to create backup: {}", e.what());
        return false;
    }
}

auto PHD2ProfileSettingHandler::restoreFromBackup(
    const std::string& profileName) -> bool {
    spdlog::info("Restoring profile {} from backup", profileName);

    fs::path backupDir =
        pImpl->PHD2_PROFILE_SAVE_PATH / "backups" / profileName;
    if (!fs::exists(backupDir)) {
        spdlog::error("No backups found for profile {}", profileName);
        return false;
    }

    try {
        // Find the latest backup
        fs::path latestBackup;
        std::time_t latestTime = 0;

        for (const auto& entry : fs::directory_iterator(backupDir)) {
            if (entry.path().extension() == ".json") {
                auto modTime = fs::last_write_time(entry);
                auto modTimeT = std::chrono::system_clock::to_time_t(
                    std::chrono::file_clock::to_sys(modTime));

                if (modTimeT > latestTime) {
                    latestTime = modTimeT;
                    latestBackup = entry.path();
                }
            }
        }

        if (latestBackup.empty()) {
            spdlog::error("No backup files found for profile {}", profileName);
            return false;
        }

        fs::path profilePath =
            pImpl->PHD2_PROFILE_SAVE_PATH / (profileName + ".json");
        fs::copy_file(latestBackup, profilePath,
                      fs::copy_options::overwrite_existing);

        // Update cache
        json configData = pImpl->loadJsonFile(profilePath);
        pImpl->updateCache(profileName, configData);

        spdlog::info("Profile {} restored from backup {}", profileName,
                     latestBackup.string());
        return true;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to restore from backup: {}", e.what());
        return false;
    }
}

auto PHD2ProfileSettingHandler::listBackups(
    const std::string& profileName) const -> std::vector<std::string> {
    spdlog::info("Listing backups for profile {}", profileName);

    std::vector<std::string> backupList;
    fs::path backupDir =
        pImpl->PHD2_PROFILE_SAVE_PATH / "backups" / profileName;

    if (!fs::exists(backupDir)) {
        spdlog::info("No backups found for profile {}", profileName);
        return backupList;
    }

    try {
        for (const auto& entry : fs::directory_iterator(backupDir)) {
            if (entry.path().extension() == ".json") {
                backupList.push_back(entry.path().filename().string());
            }
        }

        spdlog::info("Found {} backups for profile {}", backupList.size(),
                     profileName);
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Error listing backups: {}", e.what());
    }

    return backupList;
}

void PHD2ProfileSettingHandler::clearCache() {
    pImpl->profileCache.clear();
    pImpl->cacheTimestamps.clear();
    spdlog::info("Profile cache cleared");
}

void PHD2ProfileSettingHandler::preloadProfiles() {
    spdlog::info("Preloading profiles into cache");

    try {
        auto profiles = listProfiles();
        size_t loadedCount = 0;

        for (const auto& profile : profiles) {
            fs::path profilePath =
                pImpl->PHD2_PROFILE_SAVE_PATH / (profile + ".json");
            try {
                json configData = pImpl->loadJsonFile(profilePath);
                pImpl->updateCache(profile, configData);
                loadedCount++;
            } catch (const std::exception& e) {
                spdlog::warn("Failed to preload profile {}: {}", profile,
                             e.what());
            }

            // Limit the number of preloaded profiles to MAX_CACHE_SIZE
            if (loadedCount >= pImpl->MAX_CACHE_SIZE) {
                break;
            }
        }

        spdlog::info("Preloaded {} profiles into cache", loadedCount);
    } catch (const std::exception& e) {
        spdlog::error("Error preloading profiles: {}", e.what());
    }
}

auto PHD2ProfileSettingHandler::getProfileSettings(
    const std::string& profileName) const
    -> std::optional<InterfacePHD2Profile> {
    spdlog::info("Getting settings for profile {}", profileName);

    fs::path profilePath =
        pImpl->PHD2_PROFILE_SAVE_PATH / (profileName + ".json");
    if (!fs::exists(profilePath)) {
        spdlog::error("Profile {} does not exist", profileName);
        return std::nullopt;
    }

    try {
        // Check cache first
        auto cachedProfile = pImpl->getCachedProfile(profileName);
        json config;

        if (cachedProfile) {
            config = *cachedProfile;
        } else {
            config = pImpl->loadJsonFile(profilePath);
        }

        return InterfacePHD2Profile{
            .name = config.at("profile").at("1").at("name").get<std::string>(),
            .camera = config.at("profile")
                          .at("1")
                          .at("indi")
                          .at("INDIcam")
                          .get<std::string>(),
            .cameraCCD = config.at("profile")
                             .at("1")
                             .at("indi")
                             .at("INDIcam_ccd")
                             .get<std::string>(),
            .pixelSize = config.at("profile")
                             .at("1")
                             .at("camera")
                             .at("pixelsize")
                             .get<double>(),
            .telescope = config.at("profile")
                             .at("1")
                             .at("indi")
                             .at("INDImount")
                             .get<std::string>(),
            .focalLength = config.at("profile")
                               .at("1")
                               .at("frame")
                               .at("focalLength")
                               .get<double>(),
            .massChangeThreshold = config.at("profile")
                                       .at("1")
                                       .at("guider")
                                       .at("onestar")
                                       .at("MassChangeThreshold")
                                       .get<double>(),
            .massChangeFlag = config.at("profile")
                                  .at("1")
                                  .at("guider")
                                  .at("onestar")
                                  .at("MassChangeThresholdEnabled")
                                  .get<bool>(),
            .calibrationDistance = config.at("profile")
                                       .at("1")
                                       .at("scope")
                                       .at("CalibrationDistance")
                                       .get<double>(),
            .calibrationDuration = config.at("profile")
                                       .at("1")
                                       .at("scope")
                                       .at("CalibrationDuration")
                                       .get<double>()};
    } catch (const json::exception& e) {
        spdlog::error("Error parsing profile settings: {}", e.what());
        return std::nullopt;
    }
}

auto PHD2ProfileSettingHandler::findProfilesByCamera(
    const std::string& cameraModel) const -> std::vector<std::string> {
    spdlog::info("Finding profiles using camera: {}", cameraModel);
    std::vector<std::string> matches;

    try {
        if (!fs::exists(pImpl->PHD2_PROFILE_SAVE_PATH)) {
            return matches;
        }

        for (const auto& entry :
             fs::directory_iterator(pImpl->PHD2_PROFILE_SAVE_PATH)) {
            if (entry.path().extension() == ".json") {
                std::string profileName = entry.path().stem().string();

                // Check cache first
                auto cachedProfile = pImpl->getCachedProfile(profileName);
                json config;

                try {
                    if (cachedProfile) {
                        config = *cachedProfile;
                    } else {
                        config = pImpl->loadJsonFile(entry.path());
                    }

                    if (config["profile"]["1"]["indi"]["INDIcam"] ==
                        cameraModel) {
                        matches.push_back(profileName);
                    }
                } catch (const std::exception& e) {
                    spdlog::warn(
                        "Failed to check camera model in profile {}: {}",
                        profileName, e.what());
                }
            }
        }

        spdlog::info("Found {} profiles using camera {}", matches.size(),
                     cameraModel);
    } catch (const std::exception& e) {
        spdlog::error("Error searching profiles by camera: {}", e.what());
    }

    return matches;
}

auto PHD2ProfileSettingHandler::findProfilesByTelescope(
    const std::string& telescopeModel) const -> std::vector<std::string> {
    spdlog::info("Finding profiles using telescope: {}", telescopeModel);
    std::vector<std::string> matches;

    try {
        if (!fs::exists(pImpl->PHD2_PROFILE_SAVE_PATH)) {
            return matches;
        }

        for (const auto& entry :
             fs::directory_iterator(pImpl->PHD2_PROFILE_SAVE_PATH)) {
            if (entry.path().extension() == ".json") {
                std::string profileName = entry.path().stem().string();

                // Check cache first
                auto cachedProfile = pImpl->getCachedProfile(profileName);
                json config;

                try {
                    if (cachedProfile) {
                        config = *cachedProfile;
                    } else {
                        config = pImpl->loadJsonFile(entry.path());
                    }

                    if (config["profile"]["1"]["indi"]["INDImount"] ==
                        telescopeModel) {
                        matches.push_back(profileName);
                    }
                } catch (const std::exception& e) {
                    spdlog::warn(
                        "Failed to check telescope model in profile {}: {}",
                        profileName, e.what());
                }
            }
        }

        spdlog::info("Found {} profiles using telescope {}", matches.size(),
                     telescopeModel);
    } catch (const std::exception& e) {
        spdlog::error("Error searching profiles by telescope: {}", e.what());
    }

    return matches;
}