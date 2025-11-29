/*
 * metadata.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file metadata.cpp
 * @brief Script metadata management implementation
 * @date 2024-1-13
 */

#include "metadata.hpp"

#include <algorithm>
#include <shared_mutex>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace lithium::shell {

auto ScriptMetadata::create() -> ScriptMetadata {
    ScriptMetadata meta;
    meta.createdAt = std::chrono::system_clock::now();
    meta.lastModified = meta.createdAt;
    return meta;
}

void ScriptMetadata::touch() {
    lastModified = std::chrono::system_clock::now();
}

class MetadataManager::Impl {
public:
    std::unordered_map<std::string, ScriptMetadata> storage_;
    mutable std::shared_mutex mutex_;
};

MetadataManager::MetadataManager() : pImpl_(std::make_unique<Impl>()) {}

MetadataManager::~MetadataManager() = default;

MetadataManager::MetadataManager(MetadataManager&&) noexcept = default;
MetadataManager& MetadataManager::operator=(MetadataManager&&) noexcept = default;

void MetadataManager::setMetadata(std::string_view scriptName,
                                  const ScriptMetadata& metadata) {
    std::unique_lock lock(pImpl_->mutex_);
    pImpl_->storage_[std::string(scriptName)] = metadata;
    spdlog::debug("MetadataManager: set metadata for '{}'", scriptName);
}

auto MetadataManager::getMetadata(std::string_view scriptName) const
    -> std::optional<ScriptMetadata> {
    std::shared_lock lock(pImpl_->mutex_);
    auto it = pImpl_->storage_.find(std::string(scriptName));
    if (it != pImpl_->storage_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto MetadataManager::removeMetadata(std::string_view scriptName) -> bool {
    std::unique_lock lock(pImpl_->mutex_);
    auto erased = pImpl_->storage_.erase(std::string(scriptName));
    if (erased > 0) {
        spdlog::debug("MetadataManager: removed metadata for '{}'", scriptName);
        return true;
    }
    return false;
}

auto MetadataManager::hasMetadata(std::string_view scriptName) const -> bool {
    std::shared_lock lock(pImpl_->mutex_);
    return pImpl_->storage_.contains(std::string(scriptName));
}

auto MetadataManager::getAllScriptNames() const -> std::vector<std::string> {
    std::shared_lock lock(pImpl_->mutex_);
    std::vector<std::string> names;
    names.reserve(pImpl_->storage_.size());
    for (const auto& [name, _] : pImpl_->storage_) {
        names.push_back(name);
    }
    return names;
}

auto MetadataManager::findByTag(std::string_view tag) const
    -> std::vector<std::string> {
    std::shared_lock lock(pImpl_->mutex_);
    std::vector<std::string> result;
    std::string tagStr(tag);

    for (const auto& [name, meta] : pImpl_->storage_) {
        if (std::find(meta.tags.begin(), meta.tags.end(), tagStr) !=
            meta.tags.end()) {
            result.push_back(name);
        }
    }
    return result;
}

auto MetadataManager::findByLanguage(ScriptLanguage language) const
    -> std::vector<std::string> {
    std::shared_lock lock(pImpl_->mutex_);
    std::vector<std::string> result;

    for (const auto& [name, meta] : pImpl_->storage_) {
        if (meta.language == language) {
            result.push_back(name);
        }
    }
    return result;
}

auto MetadataManager::findByAuthor(std::string_view author) const
    -> std::vector<std::string> {
    std::shared_lock lock(pImpl_->mutex_);
    std::vector<std::string> result;
    std::string authorStr(author);

    for (const auto& [name, meta] : pImpl_->storage_) {
        if (meta.author == authorStr) {
            result.push_back(name);
        }
    }
    return result;
}

void MetadataManager::clear() {
    std::unique_lock lock(pImpl_->mutex_);
    pImpl_->storage_.clear();
    spdlog::debug("MetadataManager: cleared all metadata");
}

auto MetadataManager::size() const -> size_t {
    std::shared_lock lock(pImpl_->mutex_);
    return pImpl_->storage_.size();
}

}  // namespace lithium::shell
