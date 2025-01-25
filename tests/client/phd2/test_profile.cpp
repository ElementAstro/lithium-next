#ifndef PHD2_PROFILE_TEST_HPP
#define PHD2_PROFILE_TEST_HPP

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "atom/type/json.hpp"
#include "client/phd2/profile.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class PHD2ProfileSettingHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }

    void createTestProfile(const std::string& profileName) {
        json profile = {
            {"name", profileName},           {"camera", "Test Camera"},
            {"cameraCCD", "Test CCD"},       {"pixelSize", 4.5},
            {"telescope", "Test Telescope"}, {"focalLength", 1000.0},
            {"massChangeThreshold", 0.1},    {"massChangeFlag", true},
            {"calibrationDistance", 10.0},   {"calibrationDuration", 5.0}};
        std::ofstream file(ServerConfigData::PROFILE_SAVE_PATH /
                           (profileName + ".json"));
        file << profile.dump(4);
        file.close();
    }
};

TEST_F(PHD2ProfileSettingHandlerTest, LoadProfileFile) {
    PHD2ProfileSettingHandler handler;
    auto profile = handler.loadProfileFile();
    ASSERT_TRUE(profile.has_value());
    EXPECT_EQ(profile->name, "default");
}

TEST_F(PHD2ProfileSettingHandlerTest, LoadProfile) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("test_profile");
    EXPECT_TRUE(handler.loadProfile("test_profile"));
}

TEST_F(PHD2ProfileSettingHandlerTest, NewProfileSetting) {
    PHD2ProfileSettingHandler handler;
    EXPECT_TRUE(handler.newProfileSetting("new_profile"));
}

TEST_F(PHD2ProfileSettingHandlerTest, UpdateProfile) {
    PHD2ProfileSettingHandler handler;
    InterfacePHD2Profile profile = {"updated_profile",
                                    "Updated Camera",
                                    "Updated CCD",
                                    5.0,
                                    "Updated Telescope",
                                    1200.0,
                                    0.2,
                                    false,
                                    15.0,
                                    6.0};
    EXPECT_TRUE(handler.updateProfile(profile));
}

TEST_F(PHD2ProfileSettingHandlerTest, DeleteProfile) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("delete_profile");
    EXPECT_TRUE(handler.deleteProfile("delete_profile"));
}

TEST_F(PHD2ProfileSettingHandlerTest, SaveProfile) {
    PHD2ProfileSettingHandler handler;
    handler.saveProfile("saved_profile");
    EXPECT_TRUE(
        fs::exists(ServerConfigData::PROFILE_SAVE_PATH / "saved_profile.json"));
}

TEST_F(PHD2ProfileSettingHandlerTest, RestoreProfile) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("restore_profile");
    EXPECT_TRUE(handler.restoreProfile("restore_profile"));
}

TEST_F(PHD2ProfileSettingHandlerTest, ListProfiles) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("list_profile1");
    createTestProfile("list_profile2");
    auto profiles = handler.listProfiles();
    EXPECT_NE(std::find(profiles.begin(), profiles.end(), "list_profile1"),
              profiles.end());
    EXPECT_NE(std::find(profiles.begin(), profiles.end(), "list_profile2"),
              profiles.end());
}

TEST_F(PHD2ProfileSettingHandlerTest, ExportProfile) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("export_profile");
    EXPECT_TRUE(
        handler.exportProfile("export_profile", "./exported_profile.json"));
    EXPECT_TRUE(fs::exists("./exported_profile.json"));
}

TEST_F(PHD2ProfileSettingHandlerTest, ImportProfile) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("import_profile");
    EXPECT_TRUE(
        handler.importProfile("./import_profile.json", "imported_profile"));
    EXPECT_TRUE(fs::exists(ServerConfigData::PROFILE_SAVE_PATH /
                           "imported_profile.json"));
}

TEST_F(PHD2ProfileSettingHandlerTest, CompareProfiles) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("compare_profile1");
    createTestProfile("compare_profile2");
    EXPECT_TRUE(
        handler.compareProfiles("compare_profile1", "compare_profile2"));
}

TEST_F(PHD2ProfileSettingHandlerTest, PrintProfileDetails) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("print_profile");
    handler.printProfileDetails("print_profile");
}

TEST_F(PHD2ProfileSettingHandlerTest, ValidateProfile) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("validate_profile");
    EXPECT_TRUE(handler.validateProfile("validate_profile"));
}

TEST_F(PHD2ProfileSettingHandlerTest, ValidateAllProfiles) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("validate_all_profile1");
    createTestProfile("validate_all_profile2");
    auto invalidProfiles = handler.validateAllProfiles();
    EXPECT_TRUE(invalidProfiles.empty());
}

TEST_F(PHD2ProfileSettingHandlerTest, BatchExportProfiles) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("batch_export_profile1");
    createTestProfile("batch_export_profile2");
    std::vector<std::string> profiles = {"batch_export_profile1",
                                         "batch_export_profile2"};
    EXPECT_TRUE(handler.batchExportProfiles(profiles, "./batch_export"));
    EXPECT_TRUE(fs::exists("./batch_export/batch_export_profile1.json"));
    EXPECT_TRUE(fs::exists("./batch_export/batch_export_profile2.json"));
}

TEST_F(PHD2ProfileSettingHandlerTest, BatchImportProfiles) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("batch_import_profile1");
    createTestProfile("batch_import_profile2");
    EXPECT_EQ(handler.batchImportProfiles("./batch_import"), 2);
}

TEST_F(PHD2ProfileSettingHandlerTest, BatchDeleteProfiles) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("batch_delete_profile1");
    createTestProfile("batch_delete_profile2");
    std::vector<std::string> profiles = {"batch_delete_profile1",
                                         "batch_delete_profile2"};
    EXPECT_EQ(handler.batchDeleteProfiles(profiles), 2);
}

TEST_F(PHD2ProfileSettingHandlerTest, CreateBackup) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("backup_profile");
    EXPECT_TRUE(handler.createBackup("backup_profile"));
}

TEST_F(PHD2ProfileSettingHandlerTest, RestoreFromBackup) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("restore_backup_profile");
    handler.createBackup("restore_backup_profile");
    EXPECT_TRUE(handler.restoreFromBackup("restore_backup_profile"));
}

TEST_F(PHD2ProfileSettingHandlerTest, ListBackups) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("list_backup_profile");
    handler.createBackup("list_backup_profile");
    auto backups = handler.listBackups("list_backup_profile");
    EXPECT_FALSE(backups.empty());
}

TEST_F(PHD2ProfileSettingHandlerTest, ClearCache) {
    PHD2ProfileSettingHandler handler;
    handler.clearCache();
    // Verify cache is cleared (implementation-specific)
}

TEST_F(PHD2ProfileSettingHandlerTest, PreloadProfiles) {
    PHD2ProfileSettingHandler handler;
    handler.preloadProfiles();
    // Verify profiles are preloaded (implementation-specific)
}

TEST_F(PHD2ProfileSettingHandlerTest, GetProfileSettings) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("get_profile_settings");
    auto profile = handler.getProfileSettings("get_profile_settings");
    ASSERT_TRUE(profile.has_value());
    EXPECT_EQ(profile->name, "get_profile_settings");
}

TEST_F(PHD2ProfileSettingHandlerTest, FindProfilesByCamera) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("camera_profile1");
    createTestProfile("camera_profile2");
    auto profiles = handler.findProfilesByCamera("Test Camera");
    EXPECT_EQ(profiles.size(), 2);
}

TEST_F(PHD2ProfileSettingHandlerTest, FindProfilesByTelescope) {
    PHD2ProfileSettingHandler handler;
    createTestProfile("telescope_profile1");
    createTestProfile("telescope_profile2");
    auto profiles = handler.findProfilesByTelescope("Test Telescope");
    EXPECT_EQ(profiles.size(), 2);
}

#endif  // PHD2_PROFILE_TEST_HPP
