// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "memory_repository.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <spdlog/spdlog.h>

namespace lithium::target::repository {

constexpr double DEGREES_TO_RADIANS = M_PI / 180.0;

// ============================================================================
// Helper Functions
// ============================================================================

auto MemoryRepository::generateId() -> int64_t {
    return nextId_++;
}

auto MemoryRepository::toLower(const std::string& str) -> std::string {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

auto MemoryRepository::matchesPattern(const std::string& str,
                                      const std::string& pattern) -> bool {
    // Simple wildcard matching: * matches any sequence
    if (pattern == "*") return true;

    size_t strIdx = 0, patIdx = 0;

    while (strIdx < str.length() && patIdx < pattern.length()) {
        if (pattern[patIdx] == '*') {
            if (patIdx == pattern.length() - 1) return true;

            size_t nextPat = patIdx + 1;
            size_t nextStr = strIdx;

            while (nextStr < str.length() &&
                   str[nextStr] != pattern[nextPat]) {
                nextStr++;
            }

            if (nextStr == str.length()) return false;

            strIdx = nextStr + 1;
            patIdx = nextPat + 1;
        } else if (pattern[patIdx] == str[strIdx]) {
            strIdx++;
            patIdx++;
        } else {
            return false;
        }
    }

    // Check remaining pattern (should be all * or empty)
    while (patIdx < pattern.length() && pattern[patIdx] == '*') {
        patIdx++;
    }

    return strIdx == str.length() && patIdx == pattern.length();
}

auto MemoryRepository::levenshteinDistance(const std::string& s1,
                                           const std::string& s2) -> int {
    size_t m = s1.length();
    size_t n = s2.length();

    if (m == 0) return n;
    if (n == 0) return m;

    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (toLower(std::string(1, s1[i - 1])) ==
                toLower(std::string(1, s2[j - 1]))) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] =
                    1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }

    return dp[m][n];
}

auto MemoryRepository::haversineDistance(double ra1, double dec1, double ra2,
                                         double dec2) -> double {
    double dRa = (ra2 - ra1) * DEGREES_TO_RADIANS;
    double dDec = (dec2 - dec1) * DEGREES_TO_RADIANS;

    double a = std::sin(dDec / 2.0) * std::sin(dDec / 2.0) +
               std::cos(dec1 * DEGREES_TO_RADIANS) *
                   std::cos(dec2 * DEGREES_TO_RADIANS) *
                   std::sin(dRa / 2.0) * std::sin(dRa / 2.0);

    double c = 2.0 * std::asin(std::sqrt(a));
    return c / DEGREES_TO_RADIANS;  // Convert to degrees
}

// ============================================================================
// MemoryRepository Implementation
// ============================================================================

auto MemoryRepository::insert(const CelestialObjectModel& obj)
    -> std::expected<int64_t, std::string> {
    try {
        std::unique_lock lock(mutex_);

        // Check if identifier already exists
        if (byIdentifier_.find(obj.identifier) != byIdentifier_.end()) {
            return std::unexpected("Duplicate identifier: " + obj.identifier);
        }

        int64_t id = generateId();
        CelestialObjectModel newObj = obj;
        newObj.id = id;

        byId_[id] = newObj;
        byIdentifier_[obj.identifier] = newObj;

        SPDLOG_DEBUG("Object inserted: {} (id={})", obj.identifier, id);
        return id;
    } catch (const std::exception& e) {
        std::string error = "Insert failed: " + std::string(e.what());
        SPDLOG_ERROR(error);
        return std::unexpected(error);
    }
}

auto MemoryRepository::update(const CelestialObjectModel& obj)
    -> std::expected<void, std::string> {
    try {
        std::unique_lock lock(mutex_);

        auto it = byId_.find(obj.id);
        if (it == byId_.end()) {
            return std::unexpected("Object not found: id=" +
                                   std::to_string(obj.id));
        }

        // Update in both indexes
        std::string oldIdentifier = it->second.identifier;
        byId_[obj.id] = obj;

        // Update identifier index if changed
        if (oldIdentifier != obj.identifier) {
            byIdentifier_.erase(oldIdentifier);
            byIdentifier_[obj.identifier] = obj;
        } else {
            byIdentifier_[obj.identifier] = obj;
        }

        SPDLOG_DEBUG("Object updated: {} (id={})", obj.identifier, obj.id);
        return {};
    } catch (const std::exception& e) {
        std::string error = "Update failed: " + std::string(e.what());
        SPDLOG_ERROR(error);
        return std::unexpected(error);
    }
}

auto MemoryRepository::remove(int64_t id) -> bool {
    try {
        std::unique_lock lock(mutex_);

        auto it = byId_.find(id);
        if (it == byId_.end()) {
            return false;
        }

        std::string identifier = it->second.identifier;
        byId_.erase(it);
        byIdentifier_.erase(identifier);

        SPDLOG_DEBUG("Object removed: id={}", id);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Remove failed: {}", e.what());
        return false;
    }
}

auto MemoryRepository::findById(int64_t id)
    -> std::optional<CelestialObjectModel> {
    try {
        std::shared_lock lock(mutex_);

        auto it = byId_.find(id);
        if (it != byId_.end()) {
            return it->second;
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Find by id failed: {}", e.what());
        return std::nullopt;
    }
}

auto MemoryRepository::findByIdentifier(const std::string& identifier)
    -> std::optional<CelestialObjectModel> {
    try {
        std::shared_lock lock(mutex_);

        auto it = byIdentifier_.find(identifier);
        if (it != byIdentifier_.end()) {
            return it->second;
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Find by identifier failed: {}", e.what());
        return std::nullopt;
    }
}

// ==================== Batch Operations ====================

auto MemoryRepository::batchInsert(
    std::span<const CelestialObjectModel> objects, size_t chunkSize) -> int {
    try {
        std::unique_lock lock(mutex_);
        int successCount = 0;

        for (const auto& obj : objects) {
            // Check for duplicates
            if (byIdentifier_.find(obj.identifier) != byIdentifier_.end()) {
                SPDLOG_WARN("Skipping duplicate: {}", obj.identifier);
                continue;
            }

            int64_t id = generateId();
            CelestialObjectModel newObj = obj;
            newObj.id = id;

            byId_[id] = newObj;
            byIdentifier_[obj.identifier] = newObj;
            successCount++;
        }

        SPDLOG_DEBUG("Batch inserted {} objects", successCount);
        return successCount;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Batch insert failed: {}", e.what());
        return 0;
    }
}

auto MemoryRepository::batchUpdate(
    std::span<const CelestialObjectModel> objects, size_t chunkSize) -> int {
    try {
        std::unique_lock lock(mutex_);
        int successCount = 0;

        for (const auto& obj : objects) {
            auto it = byId_.find(obj.id);
            if (it == byId_.end()) {
                SPDLOG_WARN("Object not found for update: id={}", obj.id);
                continue;
            }

            std::string oldIdentifier = it->second.identifier;
            byId_[obj.id] = obj;

            if (oldIdentifier != obj.identifier) {
                byIdentifier_.erase(oldIdentifier);
                byIdentifier_[obj.identifier] = obj;
            } else {
                byIdentifier_[obj.identifier] = obj;
            }

            successCount++;
        }

        SPDLOG_DEBUG("Batch updated {} objects", successCount);
        return successCount;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Batch update failed: {}", e.what());
        return 0;
    }
}

auto MemoryRepository::upsert(
    std::span<const CelestialObjectModel> objects) -> int {
    try {
        std::unique_lock lock(mutex_);
        int successCount = 0;

        for (const auto& obj : objects) {
            if (obj.id != 0 && byId_.find(obj.id) != byId_.end()) {
                // Update existing
                std::string oldIdentifier = byId_[obj.id].identifier;
                byId_[obj.id] = obj;

                if (oldIdentifier != obj.identifier) {
                    byIdentifier_.erase(oldIdentifier);
                }
                byIdentifier_[obj.identifier] = obj;
            } else {
                // Insert new
                int64_t id = generateId();
                CelestialObjectModel newObj = obj;
                newObj.id = id;

                byId_[id] = newObj;
                byIdentifier_[obj.identifier] = newObj;
            }
            successCount++;
        }

        SPDLOG_DEBUG("Upserted {} objects", successCount);
        return successCount;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Upsert failed: {}", e.what());
        return 0;
    }
}

// ==================== Search Operations ====================

auto MemoryRepository::searchByName(const std::string& pattern, int limit)
    -> std::vector<CelestialObjectModel> {
    try {
        std::shared_lock lock(mutex_);

        std::vector<CelestialObjectModel> results;
        std::string patternLower = toLower(pattern);

        for (const auto& [_, obj] : byId_) {
            if (matchesPattern(toLower(obj.identifier), patternLower) ||
                matchesPattern(toLower(obj.chineseName), patternLower)) {
                results.push_back(obj);
                if (results.size() >= static_cast<size_t>(limit)) {
                    break;
                }
            }
        }

        return results;
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Search by name failed: {}", e.what());
        return {};
    }
}

auto MemoryRepository::fuzzySearch(const std::string& name, int tolerance,
                                   int limit)
    -> std::vector<std::pair<CelestialObjectModel, int>> {
    try {
        std::shared_lock lock(mutex_);

        std::vector<std::pair<CelestialObjectModel, int>> candidates;

        for (const auto& [_, obj] : byId_) {
            int distance = levenshteinDistance(toLower(obj.identifier),
                                               toLower(name));
            if (distance <= tolerance) {
                candidates.push_back({obj, distance});
            }
        }

        // Sort by distance
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) {
                      return a.second < b.second;
                  });

        // Limit results
        if (candidates.size() > static_cast<size_t>(limit)) {
            candidates.resize(limit);
        }

        return candidates;
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Fuzzy search failed: {}", e.what());
        return {};
    }
}

auto MemoryRepository::search(const CelestialSearchFilter& filter)
    -> std::vector<CelestialObjectModel> {
    try {
        std::shared_lock lock(mutex_);

        std::vector<CelestialObjectModel> results;

        for (const auto& [_, obj] : byId_) {
            // Check name pattern
            if (!filter.namePattern.empty()) {
                if (!matchesPattern(toLower(obj.identifier),
                                    toLower(filter.namePattern)) &&
                    !matchesPattern(toLower(obj.chineseName),
                                    toLower(filter.namePattern))) {
                    continue;
                }
            }

            // Check type
            if (!filter.type.empty() && obj.type != filter.type) {
                continue;
            }

            // Check magnitude range
            if (obj.visualMagnitudeV < filter.minMagnitude ||
                obj.visualMagnitudeV > filter.maxMagnitude) {
                continue;
            }

            // Check RA range
            if (obj.radJ2000 < filter.minRA || obj.radJ2000 > filter.maxRA) {
                continue;
            }

            // Check Dec range
            if (obj.decDJ2000 < filter.minDec ||
                obj.decDJ2000 > filter.maxDec) {
                continue;
            }

            results.push_back(obj);
        }

        // Apply offset and limit
        if (filter.offset >= static_cast<int>(results.size())) {
            return {};
        }

        auto start = results.begin() + filter.offset;
        auto end = start + std::min(filter.limit, (int)(results.size() - filter.offset));

        return {start, end};
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Complex search failed: {}", e.what());
        return {};
    }
}

auto MemoryRepository::autocomplete(const std::string& prefix, int limit)
    -> std::vector<std::string> {
    try {
        std::shared_lock lock(mutex_);

        std::vector<std::string> results;
        std::string prefixLower = toLower(prefix);

        for (const auto& [_, obj] : byId_) {
            if (toLower(obj.identifier).substr(0, prefix.length()) ==
                prefixLower) {
                results.push_back(obj.identifier);
                if (results.size() >= static_cast<size_t>(limit)) {
                    break;
                }
            }
        }

        return results;
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Autocomplete failed: {}", e.what());
        return {};
    }
}

auto MemoryRepository::searchByCoordinates(double ra, double dec,
                                           double radius, int limit)
    -> std::vector<CelestialObjectModel> {
    try {
        std::shared_lock lock(mutex_);

        std::vector<std::pair<CelestialObjectModel, double>> candidates;

        for (const auto& [_, obj] : byId_) {
            double distance =
                haversineDistance(obj.radJ2000, obj.decDJ2000, ra, dec);
            if (distance <= radius) {
                candidates.push_back({obj, distance});
            }
        }

        // Sort by distance
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) {
                      return a.second < b.second;
                  });

        // Extract objects and limit
        std::vector<CelestialObjectModel> results;
        for (const auto& [obj, _] : candidates) {
            results.push_back(obj);
            if (results.size() >= static_cast<size_t>(limit)) {
                break;
            }
        }

        return results;
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Coordinate search failed: {}", e.what());
        return {};
    }
}

auto MemoryRepository::getByType(const std::string& type, int limit)
    -> std::vector<CelestialObjectModel> {
    try {
        std::shared_lock lock(mutex_);

        std::vector<CelestialObjectModel> results;

        for (const auto& [_, obj] : byId_) {
            if (obj.type == type) {
                results.push_back(obj);
                if (results.size() >= static_cast<size_t>(limit)) {
                    break;
                }
            }
        }

        return results;
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Get by type failed: {}", e.what());
        return {};
    }
}

auto MemoryRepository::getByMagnitudeRange(double minMag, double maxMag,
                                           int limit)
    -> std::vector<CelestialObjectModel> {
    try {
        std::shared_lock lock(mutex_);

        std::vector<CelestialObjectModel> results;

        for (const auto& [_, obj] : byId_) {
            if (obj.visualMagnitudeV >= minMag &&
                obj.visualMagnitudeV <= maxMag) {
                results.push_back(obj);
                if (results.size() >= static_cast<size_t>(limit)) {
                    break;
                }
            }
        }

        return results;
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Get by magnitude range failed: {}", e.what());
        return {};
    }
}

// ==================== Statistics ====================

auto MemoryRepository::count() -> size_t {
    try {
        std::shared_lock lock(mutex_);
        return byId_.size();
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Count failed: {}", e.what());
        return 0;
    }
}

auto MemoryRepository::countByType()
    -> std::unordered_map<std::string, int64_t> {
    try {
        std::shared_lock lock(mutex_);

        std::unordered_map<std::string, int64_t> counts;

        for (const auto& [_, obj] : byId_) {
            counts[obj.type]++;
        }

        return counts;
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Count by type failed: {}", e.what());
        return {};
    }
}

// ==================== Maintenance ====================

void MemoryRepository::clear() {
    try {
        std::unique_lock lock(mutex_);
        byId_.clear();
        byIdentifier_.clear();
        nextId_ = 1;
        SPDLOG_DEBUG("Repository cleared");
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Clear failed: {}", e.what());
    }
}

auto MemoryRepository::size() const -> size_t {
    try {
        std::shared_lock lock(mutex_);
        return byId_.size();
    } catch (const std::exception& e) {
        SPDLOG_DEBUG("Size query failed: {}", e.what());
        return 0;
    }
}

}  // namespace lithium::target::repository
