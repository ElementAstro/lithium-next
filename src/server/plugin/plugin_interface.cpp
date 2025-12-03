/*
 * plugin_interface.cpp - Plugin Interface Implementation
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "plugin_interface.hpp"

#include "atom/type/json.hpp"

namespace lithium::server::plugin {

auto PluginMetadata::toJson() const -> json {
    return json{
        {"name", name},
        {"version", version},
        {"description", description},
        {"author", author},
        {"license", license},
        {"homepage", homepage},
        {"repository", repository},
        {"priority", priority},
        {"dependencies", dependencies},
        {"optionalDeps", optionalDeps},
        {"conflicts", conflicts},
        {"tags", tags},
        {"capabilities", capabilities}
    };
}

auto PluginMetadata::fromJson(const json& j) -> PluginMetadata {
    PluginMetadata meta;
    meta.name = j.value("name", "");
    meta.version = j.value("version", "1.0.0");
    meta.description = j.value("description", "");
    meta.author = j.value("author", "");
    meta.license = j.value("license", "");
    meta.homepage = j.value("homepage", "");
    meta.repository = j.value("repository", "");
    meta.priority = j.value("priority", 0);

    auto parseStringArray = [&j](const std::string& key) {
        std::vector<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) {
                if (item.is_string()) {
                    result.push_back(item.get<std::string>());
                }
            }
        }
        return result;
    };

    meta.dependencies = parseStringArray("dependencies");
    meta.optionalDeps = parseStringArray("optionalDeps");
    meta.conflicts = parseStringArray("conflicts");
    meta.tags = parseStringArray("tags");
    meta.capabilities = parseStringArray("capabilities");

    return meta;
}

auto PluginMetadata::hasCapability(const std::string& cap) const -> bool {
    return std::find(capabilities.begin(), capabilities.end(), cap) !=
           capabilities.end();
}

auto pluginStateToString(PluginState state) -> std::string {
    switch (state) {
        case PluginState::Unloaded:
            return "unloaded";
        case PluginState::Loading:
            return "loading";
        case PluginState::Loaded:
            return "loaded";
        case PluginState::Initialized:
            return "initialized";
        case PluginState::Running:
            return "running";
        case PluginState::Paused:
            return "paused";
        case PluginState::Stopping:
            return "stopping";
        case PluginState::Error:
            return "error";
        case PluginState::Disabled:
            return "disabled";
        default:
            return "unknown";
    }
}

}  // namespace lithium::server::plugin
