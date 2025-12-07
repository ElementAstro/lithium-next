/*
 * versioning.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file versioning.cpp
 * @brief Implementation of script version management
 * @date 2024-1-13
 */

#include "versioning.hpp"

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace lithium::shell {

/**
 * @brief Private implementation of VersionManager using Pimpl pattern
 */
class VersionManager::Impl {
public:
    /// Storage structure: scriptName -> list of versions
    std::unordered_map<std::string, std::vector<ScriptVersion>> versionStorage_;

    /// Maximum versions to keep per script
    size_t maxVersions_{10};

    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex_;

    /**
     * @brief Prune old versions if exceeding limit
     * @param scriptName Script name to prune
     * @pre mutex_ must be locked with exclusive access
     */
    void pruneOldVersions(const std::string& scriptName) noexcept {
        auto it = versionStorage_.find(scriptName);
        if (it == versionStorage_.end()) {
            return;
        }

        auto& versions = it->second;

        if (versions.size() > maxVersions_) {
            size_t toRemove = versions.size() - maxVersions_;
            versions.erase(versions.begin(), versions.begin() + toRemove);

            spdlog::debug(
                "VersionManager: pruned {} old versions for script '{}', "
                "keeping {} versions",
                toRemove, scriptName, maxVersions_);
        }
    }

    /**
     * @brief Calculate next version number for a script
     * @param scriptName Script name
     * @return Next version number
     * @pre mutex_ must be locked
     */
    [[nodiscard]] auto getNextVersionNumber(
        const std::string& scriptName) const noexcept -> size_t {
        auto it = versionStorage_.find(scriptName);
        if (it == versionStorage_.end() || it->second.empty()) {
            return 1;
        }
        return it->second.back().versionNumber + 1;
    }
};

VersionManager::VersionManager(size_t maxVersions) noexcept
    : pImpl_(std::make_unique<Impl>()) {
    pImpl_->maxVersions_ = maxVersions;
    spdlog::debug("VersionManager: initialized with maxVersions={}",
                  maxVersions);
}

VersionManager::~VersionManager() noexcept {
    spdlog::debug("VersionManager: destroying instance");
}

auto VersionManager::saveVersion(std::string_view scriptName,
                                 std::string_view content,
                                 std::string_view author,
                                 std::string_view changeDescription) -> size_t {
    std::unique_lock<std::shared_mutex> lock(pImpl_->mutex_);

    std::string scriptNameStr(scriptName);

    // Get next version number
    size_t versionNumber = pImpl_->getNextVersionNumber(scriptNameStr);

    // Create version record
    ScriptVersion version;
    version.versionNumber = versionNumber;
    version.content = content;
    version.timestamp = std::chrono::system_clock::now();
    version.author = author;
    version.changeDescription = changeDescription;

    // Store version
    pImpl_->versionStorage_[scriptNameStr].push_back(version);

    // Prune old versions if necessary
    pImpl_->pruneOldVersions(scriptNameStr);

    spdlog::debug(
        "VersionManager: saved version {} for script '{}' by author '{}'",
        versionNumber, scriptName, author);

    return versionNumber;
}

auto VersionManager::getVersion(std::string_view scriptName,
                                size_t versionNumber) const
    -> std::optional<ScriptVersion> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->mutex_);

    auto it = pImpl_->versionStorage_.find(std::string(scriptName));
    if (it == pImpl_->versionStorage_.end()) {
        spdlog::debug("VersionManager: script '{}' not found", scriptName);
        return std::nullopt;
    }

    const auto& versions = it->second;
    auto versionIt = std::find_if(versions.begin(), versions.end(),
                                  [versionNumber](const ScriptVersion& v) {
                                      return v.versionNumber == versionNumber;
                                  });

    if (versionIt != versions.end()) {
        spdlog::debug("VersionManager: retrieved version {} of script '{}'",
                      versionNumber, scriptName);
        return *versionIt;
    }

    spdlog::debug(
        "VersionManager: version {} not found for script '{}' "
        "(available: {})",
        versionNumber, scriptName, versions.size());
    return std::nullopt;
}

auto VersionManager::getLatestVersion(std::string_view scriptName) const
    -> std::optional<ScriptVersion> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->mutex_);

    auto it = pImpl_->versionStorage_.find(std::string(scriptName));
    if (it == pImpl_->versionStorage_.end() || it->second.empty()) {
        spdlog::debug(
            "VersionManager: no versions found for script '{}', returning "
            "nullopt",
            scriptName);
        return std::nullopt;
    }

    spdlog::debug("VersionManager: retrieved latest version of script '{}'",
                  scriptName);
    return it->second.back();
}

auto VersionManager::rollback(std::string_view scriptName,
                              size_t versionNumber) const
    -> std::optional<std::string> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->mutex_);

    auto it = pImpl_->versionStorage_.find(std::string(scriptName));
    if (it == pImpl_->versionStorage_.end()) {
        spdlog::warn("VersionManager: cannot rollback - script '{}' not found",
                     scriptName);
        return std::nullopt;
    }

    const auto& versions = it->second;
    auto versionIt = std::find_if(versions.begin(), versions.end(),
                                  [versionNumber](const ScriptVersion& v) {
                                      return v.versionNumber == versionNumber;
                                  });

    if (versionIt != versions.end()) {
        spdlog::info("VersionManager: rolled back script '{}' to version {}",
                     scriptName, versionNumber);
        return versionIt->content;
    }

    spdlog::warn(
        "VersionManager: cannot rollback - version {} not found for script "
        "'{}'",
        versionNumber, scriptName);
    return std::nullopt;
}

auto VersionManager::getVersionHistory(std::string_view scriptName) const
    -> std::vector<ScriptVersion> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->mutex_);

    auto it = pImpl_->versionStorage_.find(std::string(scriptName));
    if (it == pImpl_->versionStorage_.end()) {
        spdlog::debug("VersionManager: no version history for script '{}'",
                      scriptName);
        return {};
    }

    spdlog::debug(
        "VersionManager: retrieved version history for script '{}' "
        "({} versions)",
        scriptName, it->second.size());
    return it->second;
}

auto VersionManager::getVersionCount(std::string_view scriptName) const
    -> size_t {
    std::shared_lock<std::shared_mutex> lock(pImpl_->mutex_);

    auto it = pImpl_->versionStorage_.find(std::string(scriptName));
    if (it == pImpl_->versionStorage_.end()) {
        return 0;
    }

    return it->second.size();
}

void VersionManager::setMaxVersions(size_t maxVersions) noexcept {
    std::unique_lock<std::shared_mutex> lock(pImpl_->mutex_);

    size_t oldMax = pImpl_->maxVersions_;
    pImpl_->maxVersions_ = maxVersions;

    // Prune all scripts to respect new limit
    for (auto& [scriptName, versions] : pImpl_->versionStorage_) {
        if (versions.size() > maxVersions) {
            size_t toRemove = versions.size() - maxVersions;
            versions.erase(versions.begin(), versions.begin() + toRemove);

            spdlog::debug(
                "VersionManager: adjusted max versions from {} to {} for "
                "script '{}', removed {} versions",
                oldMax, maxVersions, scriptName, toRemove);
        }
    }

    spdlog::info("VersionManager: max versions changed from {} to {}", oldMax,
                 maxVersions);
}

auto VersionManager::getMaxVersions() const noexcept -> size_t {
    std::shared_lock<std::shared_mutex> lock(pImpl_->mutex_);
    return pImpl_->maxVersions_;
}

void VersionManager::clearVersionHistory(std::string_view scriptName) {
    std::unique_lock<std::shared_mutex> lock(pImpl_->mutex_);

    auto it = pImpl_->versionStorage_.find(std::string(scriptName));
    if (it != pImpl_->versionStorage_.end()) {
        size_t count = it->second.size();
        pImpl_->versionStorage_.erase(it);
        spdlog::info("VersionManager: cleared {} versions for script '{}'",
                     count, scriptName);
    }
}

void VersionManager::clearAllVersionHistory() noexcept {
    std::unique_lock<std::shared_mutex> lock(pImpl_->mutex_);

    size_t totalCount = 0;
    for (const auto& [_, versions] : pImpl_->versionStorage_) {
        totalCount += versions.size();
    }

    pImpl_->versionStorage_.clear();

    spdlog::info(
        "VersionManager: cleared all version history ({} total "
        "versions removed)",
        totalCount);
}

auto VersionManager::hasVersions(std::string_view scriptName) const noexcept
    -> bool {
    std::shared_lock<std::shared_mutex> lock(pImpl_->mutex_);

    auto it = pImpl_->versionStorage_.find(std::string(scriptName));
    return it != pImpl_->versionStorage_.end() && !it->second.empty();
}

auto VersionManager::getAllVersionedScripts() const
    -> std::vector<std::string> {
    std::shared_lock<std::shared_mutex> lock(pImpl_->mutex_);

    std::vector<std::string> scripts;
    scripts.reserve(pImpl_->versionStorage_.size());

    for (const auto& [scriptName, _] : pImpl_->versionStorage_) {
        scripts.push_back(scriptName);
    }

    spdlog::debug("VersionManager: retrieved list of {} versioned scripts",
                  scripts.size());
    return scripts;
}

}  // namespace lithium::shell
