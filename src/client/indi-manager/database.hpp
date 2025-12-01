/*
 * database.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_CLIENT_INDI_MANAGER_DATABASE_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_DATABASE_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::client::indi_manager {

using json = nlohmann::json;

/**
 * @class Database
 * @brief A class to manage and store INDI profiles and drivers using JSON.
 */
class Database {
public:
    explicit Database(const std::string& filename);

    std::optional<std::string> getAutoProfile() const;
    std::vector<json> getProfiles() const;
    std::vector<json> getCustomDrivers() const;
    std::vector<std::string> getProfileDriversLabels(
        const std::string& name) const;
    std::optional<std::string> getProfileRemoteDrivers(
        const std::string& name) const;

    void deleteProfile(const std::string& name);
    int addProfile(const std::string& name);
    std::optional<json> getProfile(const std::string& name) const;
    void updateProfile(const std::string& name, int port,
                       bool autostart = false, bool autoconnect = false);
    void saveProfileDrivers(const std::string& name,
                            const std::vector<json>& drivers);
    void saveProfileCustomDriver(const json& driver);

private:
    void update();
    void create();
    void save() const;
    void load();

    std::filesystem::path filepath_;
    json db_;
    static constexpr const char* CURRENT_VERSION = "0.1.6";
};

}  // namespace lithium::client::indi_manager

#endif  // LITHIUM_CLIENT_INDI_MANAGER_DATABASE_HPP
