/*
 * test_system_config.cpp - Tests for System Config Controller
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/type/json.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Config Path Tests
// ============================================================================

class ConfigPathTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_config_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    fs::path testDir_;
};

TEST_F(ConfigPathTest, ValidConfigPath) {
    fs::path configPath = testDir_ / "config.json";

    // Create a test config file
    json config = {{"setting1", "value1"}, {"setting2", 42}};
    std::ofstream file(configPath);
    file << config.dump(2);
    file.close();

    EXPECT_TRUE(fs::exists(configPath));
}

TEST_F(ConfigPathTest, NestedConfigPath) {
    fs::path nestedPath = testDir_ / "nested" / "deep" / "config.json";
    fs::create_directories(nestedPath.parent_path());

    json config = {{"nested", true}};
    std::ofstream file(nestedPath);
    file << config.dump();
    file.close();

    EXPECT_TRUE(fs::exists(nestedPath));
}

TEST_F(ConfigPathTest, ConfigFileExtension) {
    std::vector<std::string> validExtensions = {".json", ".yaml", ".yml",
                                                ".toml"};

    for (const auto& ext : validExtensions) {
        fs::path configPath = testDir_ / ("config" + ext);
        std::ofstream file(configPath);
        file << "{}";
        file.close();

        EXPECT_TRUE(fs::exists(configPath));
    }
}

// ============================================================================
// Config JSON Structure Tests
// ============================================================================

class ConfigJsonStructureTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConfigJsonStructureTest, BasicConfigStructure) {
    json config = {{"version", "1.0.0"},
                   {"settings", {{"debug", false}, {"log_level", "info"}}},
                   {"devices", json::array()}};

    EXPECT_TRUE(config.contains("version"));
    EXPECT_TRUE(config.contains("settings"));
    EXPECT_TRUE(config.contains("devices"));
}

TEST_F(ConfigJsonStructureTest, DeviceConfigStructure) {
    json deviceConfig = {{"id", "camera_1"},
                         {"type", "camera"},
                         {"driver", "zwo_asi"},
                         {"enabled", true},
                         {"settings", {{"gain", 100}, {"exposure", 1.0}}}};

    EXPECT_EQ(deviceConfig["id"], "camera_1");
    EXPECT_EQ(deviceConfig["type"], "camera");
    EXPECT_TRUE(deviceConfig["enabled"].get<bool>());
}

TEST_F(ConfigJsonStructureTest, LocationConfigStructure) {
    json locationConfig = {{"latitude", 40.7128},
                           {"longitude", -74.0060},
                           {"elevation", 10.0},
                           {"timezone", "America/New_York"}};

    EXPECT_DOUBLE_EQ(locationConfig["latitude"], 40.7128);
    EXPECT_DOUBLE_EQ(locationConfig["longitude"], -74.0060);
}

TEST_F(ConfigJsonStructureTest, ImageConfigStructure) {
    json imageConfig = {{"saveBasePath", "/home/user/images"},
                        {"format", "fits"},
                        {"compression", false},
                        {"naming", {{"prefix", "img_"}, {"timestamp", true}}}};

    EXPECT_EQ(imageConfig["saveBasePath"], "/home/user/images");
    EXPECT_EQ(imageConfig["format"], "fits");
}

// ============================================================================
// Config Value Validation Tests
// ============================================================================

class ConfigValueValidationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool isValidLatitude(double lat) { return lat >= -90.0 && lat <= 90.0; }

    bool isValidLongitude(double lon) { return lon >= -180.0 && lon <= 180.0; }

    bool isValidLogLevel(const std::string& level) {
        static const std::vector<std::string> validLevels = {
            "trace", "debug", "info", "warn", "error", "critical", "off"};
        return std::find(validLevels.begin(), validLevels.end(), level) !=
               validLevels.end();
    }
};

TEST_F(ConfigValueValidationTest, ValidLatitude) {
    EXPECT_TRUE(isValidLatitude(0.0));
    EXPECT_TRUE(isValidLatitude(45.0));
    EXPECT_TRUE(isValidLatitude(-45.0));
    EXPECT_TRUE(isValidLatitude(90.0));
    EXPECT_TRUE(isValidLatitude(-90.0));
}

TEST_F(ConfigValueValidationTest, InvalidLatitude) {
    EXPECT_FALSE(isValidLatitude(91.0));
    EXPECT_FALSE(isValidLatitude(-91.0));
    EXPECT_FALSE(isValidLatitude(180.0));
}

TEST_F(ConfigValueValidationTest, ValidLongitude) {
    EXPECT_TRUE(isValidLongitude(0.0));
    EXPECT_TRUE(isValidLongitude(90.0));
    EXPECT_TRUE(isValidLongitude(-90.0));
    EXPECT_TRUE(isValidLongitude(180.0));
    EXPECT_TRUE(isValidLongitude(-180.0));
}

TEST_F(ConfigValueValidationTest, InvalidLongitude) {
    EXPECT_FALSE(isValidLongitude(181.0));
    EXPECT_FALSE(isValidLongitude(-181.0));
    EXPECT_FALSE(isValidLongitude(360.0));
}

TEST_F(ConfigValueValidationTest, ValidLogLevels) {
    EXPECT_TRUE(isValidLogLevel("trace"));
    EXPECT_TRUE(isValidLogLevel("debug"));
    EXPECT_TRUE(isValidLogLevel("info"));
    EXPECT_TRUE(isValidLogLevel("warn"));
    EXPECT_TRUE(isValidLogLevel("error"));
    EXPECT_TRUE(isValidLogLevel("critical"));
    EXPECT_TRUE(isValidLogLevel("off"));
}

TEST_F(ConfigValueValidationTest, InvalidLogLevels) {
    EXPECT_FALSE(isValidLogLevel("invalid"));
    EXPECT_FALSE(isValidLogLevel(""));
    EXPECT_FALSE(isValidLogLevel("DEBUG"));  // Case sensitive
}

// ============================================================================
// Config Merge Tests
// ============================================================================

class ConfigMergeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    json mergeConfigs(const json& base, const json& override_config) {
        json result = base;
        for (auto& [key, value] : override_config.items()) {
            if (result.contains(key) && result[key].is_object() &&
                value.is_object()) {
                result[key] = mergeConfigs(result[key], value);
            } else {
                result[key] = value;
            }
        }
        return result;
    }
};

TEST_F(ConfigMergeTest, SimpleOverride) {
    json base = {{"key1", "value1"}, {"key2", "value2"}};
    json override_config = {{"key2", "overridden"}};

    auto merged = mergeConfigs(base, override_config);

    EXPECT_EQ(merged["key1"], "value1");
    EXPECT_EQ(merged["key2"], "overridden");
}

TEST_F(ConfigMergeTest, AddNewKey) {
    json base = {{"key1", "value1"}};
    json override_config = {{"key2", "value2"}};

    auto merged = mergeConfigs(base, override_config);

    EXPECT_EQ(merged["key1"], "value1");
    EXPECT_EQ(merged["key2"], "value2");
}

TEST_F(ConfigMergeTest, NestedMerge) {
    json base = {{"settings", {{"a", 1}, {"b", 2}}}};
    json override_config = {{"settings", {{"b", 3}, {"c", 4}}}};

    auto merged = mergeConfigs(base, override_config);

    EXPECT_EQ(merged["settings"]["a"], 1);
    EXPECT_EQ(merged["settings"]["b"], 3);
    EXPECT_EQ(merged["settings"]["c"], 4);
}

TEST_F(ConfigMergeTest, EmptyOverride) {
    json base = {{"key", "value"}};
    json override_config = json::object();

    auto merged = mergeConfigs(base, override_config);

    EXPECT_EQ(merged, base);
}

// ============================================================================
// Config JSON Pointer Tests
// ============================================================================

class ConfigJsonPointerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = {{"quarcs",
                    {{"location", {{"latitude", 40.0}, {"longitude", -74.0}}},
                     {"image", {{"saveBasePath", "/images"}}}}}};
    }

    void TearDown() override {}

    json config_;
};

TEST_F(ConfigJsonPointerTest, GetNestedValue) {
    auto latitude = config_.at(json::json_pointer("/quarcs/location/latitude"));
    EXPECT_DOUBLE_EQ(latitude.get<double>(), 40.0);
}

TEST_F(ConfigJsonPointerTest, SetNestedValue) {
    config_[json::json_pointer("/quarcs/location/latitude")] = 45.0;
    EXPECT_DOUBLE_EQ(config_.at(json::json_pointer("/quarcs/location/latitude"))
                         .get<double>(),
                     45.0);
}

TEST_F(ConfigJsonPointerTest, CreateNewPath) {
    config_[json::json_pointer("/quarcs/new/setting")] = "value";
    EXPECT_EQ(config_.at(json::json_pointer("/quarcs/new/setting"))
                  .get<std::string>(),
              "value");
}

TEST_F(ConfigJsonPointerTest, InvalidPointer) {
    EXPECT_THROW(config_.at(json::json_pointer("/invalid/path")),
                 json::out_of_range);
}

// ============================================================================
// Config File Operations Tests
// ============================================================================

class ConfigFileOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_config_ops_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    bool saveConfig(const fs::path& path, const json& config) {
        try {
            std::ofstream file(path);
            if (!file.is_open())
                return false;
            file << config.dump(2);
            return true;
        } catch (...) {
            return false;
        }
    }

    std::optional<json> loadConfig(const fs::path& path) {
        try {
            std::ifstream file(path);
            if (!file.is_open())
                return std::nullopt;
            return json::parse(file);
        } catch (...) {
            return std::nullopt;
        }
    }

    fs::path testDir_;
};

TEST_F(ConfigFileOperationsTest, SaveAndLoad) {
    fs::path configPath = testDir_ / "test_config.json";
    json config = {{"key", "value"}, {"number", 42}};

    EXPECT_TRUE(saveConfig(configPath, config));

    auto loaded = loadConfig(configPath);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded.value()["key"], "value");
    EXPECT_EQ(loaded.value()["number"], 42);
}

TEST_F(ConfigFileOperationsTest, LoadNonexistent) {
    auto loaded = loadConfig(testDir_ / "nonexistent.json");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(ConfigFileOperationsTest, SaveToInvalidPath) {
    fs::path invalidPath = "/nonexistent/directory/config.json";
    json config = {{"key", "value"}};

    EXPECT_FALSE(saveConfig(invalidPath, config));
}

TEST_F(ConfigFileOperationsTest, LoadInvalidJson) {
    fs::path configPath = testDir_ / "invalid.json";
    std::ofstream file(configPath);
    file << "{ invalid json }";
    file.close();

    auto loaded = loadConfig(configPath);
    EXPECT_FALSE(loaded.has_value());
}

// ============================================================================
// Config Backup Tests
// ============================================================================

class ConfigBackupTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_config_backup_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    fs::path createBackup(const fs::path& configPath) {
        if (!fs::exists(configPath))
            return {};

        auto backupPath =
            configPath.parent_path() / (configPath.stem().string() + ".backup" +
                                        configPath.extension().string());

        fs::copy_file(configPath, backupPath,
                      fs::copy_options::overwrite_existing);
        return backupPath;
    }

    fs::path testDir_;
};

TEST_F(ConfigBackupTest, CreateBackup) {
    fs::path configPath = testDir_ / "config.json";
    std::ofstream file(configPath);
    file << R"({"key": "value"})";
    file.close();

    auto backupPath = createBackup(configPath);

    EXPECT_TRUE(fs::exists(backupPath));
    EXPECT_EQ(backupPath.filename(), "config.backup.json");
}

TEST_F(ConfigBackupTest, BackupNonexistent) {
    auto backupPath = createBackup(testDir_ / "nonexistent.json");
    EXPECT_TRUE(backupPath.empty());
}

// ============================================================================
// Config Schema Validation Tests
// ============================================================================

class ConfigSchemaValidationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool validateDeviceConfig(const json& config) {
        if (!config.contains("id") || !config["id"].is_string())
            return false;
        if (!config.contains("type") || !config["type"].is_string())
            return false;
        return true;
    }

    bool validateLocationConfig(const json& config) {
        if (!config.contains("latitude") || !config["latitude"].is_number())
            return false;
        if (!config.contains("longitude") || !config["longitude"].is_number())
            return false;
        return true;
    }
};

TEST_F(ConfigSchemaValidationTest, ValidDeviceConfig) {
    json config = {{"id", "camera_1"}, {"type", "camera"}};
    EXPECT_TRUE(validateDeviceConfig(config));
}

TEST_F(ConfigSchemaValidationTest, InvalidDeviceConfigMissingId) {
    json config = {{"type", "camera"}};
    EXPECT_FALSE(validateDeviceConfig(config));
}

TEST_F(ConfigSchemaValidationTest, InvalidDeviceConfigWrongType) {
    json config = {{"id", 123}, {"type", "camera"}};
    EXPECT_FALSE(validateDeviceConfig(config));
}

TEST_F(ConfigSchemaValidationTest, ValidLocationConfig) {
    json config = {{"latitude", 40.0}, {"longitude", -74.0}};
    EXPECT_TRUE(validateLocationConfig(config));
}

TEST_F(ConfigSchemaValidationTest, InvalidLocationConfigMissingField) {
    json config = {{"latitude", 40.0}};
    EXPECT_FALSE(validateLocationConfig(config));
}
