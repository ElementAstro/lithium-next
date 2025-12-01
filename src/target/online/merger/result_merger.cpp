// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "result_merger.hpp"

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <unordered_set>

namespace lithium::target::online {

/**
 * @brief Internal implementation of ResultMerger using PIMPL pattern
 */
class ResultMerger::Impl {
public:
    explicit Impl(const MergeConfig& config)
        : config_(config), stats_{} {}

    /**
     * @brief Normalize a string for comparison (lowercase, trim)
     */
    static auto normalizeString(const std::string& str) -> std::string {
        if (str.empty()) {
            return "";
        }
        std::string result;
        result.reserve(str.length());

        // Convert to lowercase
        for (char c : str) {
            result += std::tolower(static_cast<unsigned char>(c));
        }

        // Remove leading/trailing whitespace
        auto start = result.find_first_not_of(" \t\n\r\f\v");
        auto end = result.find_last_not_of(" \t\n\r\f\v");

        if (start == std::string::npos) {
            return "";
        }
        return result.substr(start, end - start + 1);
    }

    /**
     * @brief Calculate Haversine distance between two points
     * @param ra1 Right ascension of first point in degrees
     * @param dec1 Declination of first point in degrees
     * @param ra2 Right ascension of second point in degrees
     * @param dec2 Declination of second point in degrees
     * @return Angular distance in degrees
     */
    static auto calculateAngularDistance(double ra1, double dec1, double ra2,
                                         double dec2) -> double {
        // Convert to radians
        double ra1_rad = ra1 * M_PI / 180.0;
        double dec1_rad = dec1 * M_PI / 180.0;
        double ra2_rad = ra2 * M_PI / 180.0;
        double dec2_rad = dec2 * M_PI / 180.0;

        // Haversine formula
        double dRa = ra2_rad - ra1_rad;
        double dDec = dec2_rad - dec1_rad;

        double a = std::sin(dDec / 2.0) * std::sin(dDec / 2.0) +
                   std::cos(dec1_rad) * std::cos(dec2_rad) *
                       std::sin(dRa / 2.0) * std::sin(dRa / 2.0);
        double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
        double distance = 180.0 / M_PI * c;

        return distance;
    }

    /**
     * @brief Check if two identifiers match after normalization
     */
    auto doesNameMatch(const std::string& a, const std::string& b) const
        -> bool {
        if (!config_.matchByName) {
            return false;
        }

        if (a.empty() || b.empty()) {
            return false;
        }

        return normalizeString(a) == normalizeString(b);
    }

    /**
     * @brief Check if two coordinates match within the configured radius
     */
    auto doesCoordinateMatch(const CelestialObjectModel& a,
                             const CelestialObjectModel& b) const -> bool {
        if (!config_.matchByCoordinates) {
            return false;
        }

        // Check if both objects have valid coordinates
        if (a.radJ2000 == 0.0 && a.raJ2000.empty()) {
            return false;
        }
        if (b.radJ2000 == 0.0 && b.raJ2000.empty()) {
            return false;
        }

        // Calculate angular distance
        double distance = calculateAngularDistance(a.radJ2000, a.decDJ2000,
                                                  b.radJ2000, b.decDJ2000);

        return distance <= config_.coordinateMatchRadius;
    }

    /**
     * @brief Count non-empty fields in a celestial object model
     */
    static auto countNonEmptyFields(const CelestialObjectModel& obj)
        -> size_t {
        size_t count = 0;

        if (!obj.identifier.empty()) count++;
        if (!obj.mIdentifier.empty()) count++;
        if (!obj.extensionName.empty()) count++;
        if (!obj.component.empty()) count++;
        if (!obj.className.empty()) count++;
        if (obj.amateurRank != 0) count++;
        if (!obj.chineseName.empty()) count++;
        if (!obj.type.empty()) count++;
        if (!obj.duplicateType.empty()) count++;
        if (!obj.morphology.empty()) count++;
        if (!obj.constellationZh.empty()) count++;
        if (!obj.constellationEn.empty()) count++;
        if (!obj.raJ2000.empty()) count++;
        if (obj.radJ2000 != 0.0) count++;
        if (!obj.decJ2000.empty()) count++;
        if (obj.decDJ2000 != 0.0) count++;
        if (obj.visualMagnitudeV != 0.0) count++;
        if (obj.photographicMagnitudeB != 0.0) count++;
        if (obj.bMinusV != 0.0) count++;
        if (obj.surfaceBrightness != 0.0) count++;
        if (obj.majorAxis != 0.0) count++;
        if (obj.minorAxis != 0.0) count++;
        if (obj.positionAngle != 0.0) count++;
        if (!obj.detailedDescription.empty()) count++;
        if (!obj.briefDescription.empty()) count++;
        if (!obj.aliases.empty()) count++;

        return count;
    }

    /**
     * @brief Merge two objects into one based on configured strategy
     */
    auto mergeObjectsInternal(const CelestialObjectModel& primary,
                              const CelestialObjectModel& secondary) const
        -> CelestialObjectModel {
        CelestialObjectModel result = primary;

        // Determine which object to use as base
        CelestialObjectModel base;
        CelestialObjectModel source;

        switch (config_.strategy) {
            case MergeStrategy::PreferLocal: {
                base = primary;
                source = secondary;
                break;
            }
            case MergeStrategy::PreferOnline: {
                base = secondary;
                source = primary;
                break;
            }
            case MergeStrategy::MostComplete: {
                size_t primaryFields = countNonEmptyFields(primary);
                size_t secondaryFields = countNonEmptyFields(secondary);
                if (secondaryFields > primaryFields) {
                    base = secondary;
                    source = primary;
                } else {
                    base = primary;
                    source = secondary;
                }
                break;
            }
            default:  // PreferLocal is default
                base = primary;
                source = secondary;
                break;
        }

        result = base;

        // Merge missing fields from source into result
        if (result.identifier.empty() && !source.identifier.empty()) {
            result.identifier = source.identifier;
        }
        if (result.mIdentifier.empty() && !source.mIdentifier.empty()) {
            result.mIdentifier = source.mIdentifier;
        }
        if (result.extensionName.empty() && !source.extensionName.empty()) {
            result.extensionName = source.extensionName;
        }
        if (result.component.empty() && !source.component.empty()) {
            result.component = source.component;
        }
        if (result.className.empty() && !source.className.empty()) {
            result.className = source.className;
        }
        if (result.amateurRank == 0 && source.amateurRank != 0) {
            result.amateurRank = source.amateurRank;
        }
        if (result.chineseName.empty() && !source.chineseName.empty()) {
            result.chineseName = source.chineseName;
        }
        if (result.type.empty() && !source.type.empty()) {
            result.type = source.type;
        }
        if (result.duplicateType.empty() && !source.duplicateType.empty()) {
            result.duplicateType = source.duplicateType;
        }
        if (result.morphology.empty() && !source.morphology.empty()) {
            result.morphology = source.morphology;
        }
        if (result.constellationZh.empty() && !source.constellationZh.empty()) {
            result.constellationZh = source.constellationZh;
        }
        if (result.constellationEn.empty() && !source.constellationEn.empty()) {
            result.constellationEn = source.constellationEn;
        }
        if (result.raJ2000.empty() && !source.raJ2000.empty()) {
            result.raJ2000 = source.raJ2000;
        }
        if (result.radJ2000 == 0.0 && source.radJ2000 != 0.0) {
            result.radJ2000 = source.radJ2000;
        }
        if (result.decJ2000.empty() && !source.decJ2000.empty()) {
            result.decJ2000 = source.decJ2000;
        }
        if (result.decDJ2000 == 0.0 && source.decDJ2000 != 0.0) {
            result.decDJ2000 = source.decDJ2000;
        }
        if (result.visualMagnitudeV == 0.0 && source.visualMagnitudeV != 0.0) {
            result.visualMagnitudeV = source.visualMagnitudeV;
        }
        if (result.photographicMagnitudeB == 0.0 &&
            source.photographicMagnitudeB != 0.0) {
            result.photographicMagnitudeB = source.photographicMagnitudeB;
        }
        if (result.bMinusV == 0.0 && source.bMinusV != 0.0) {
            result.bMinusV = source.bMinusV;
        }
        if (result.surfaceBrightness == 0.0 &&
            source.surfaceBrightness != 0.0) {
            result.surfaceBrightness = source.surfaceBrightness;
        }
        if (result.majorAxis == 0.0 && source.majorAxis != 0.0) {
            result.majorAxis = source.majorAxis;
        }
        if (result.minorAxis == 0.0 && source.minorAxis != 0.0) {
            result.minorAxis = source.minorAxis;
        }
        if (result.positionAngle == 0.0 && source.positionAngle != 0.0) {
            result.positionAngle = source.positionAngle;
        }
        if (result.detailedDescription.empty() &&
            !source.detailedDescription.empty()) {
            result.detailedDescription = source.detailedDescription;
        }
        if (result.briefDescription.empty() &&
            !source.briefDescription.empty()) {
            result.briefDescription = source.briefDescription;
        }
        if (result.aliases.empty() && !source.aliases.empty()) {
            result.aliases = source.aliases;
        }

        return result;
    }

    /**
     * @brief Check if two objects are exact duplicates by identifier
     */
    auto isIdentifierDuplicate(const CelestialObjectModel& a,
                               const CelestialObjectModel& b) const -> bool {
        // Exact match on primary identifier
        if (!a.identifier.empty() && !b.identifier.empty()) {
            if (doesNameMatch(a.identifier, b.identifier)) {
                return true;
            }
        }

        // Match on Messier identifier
        if (!a.mIdentifier.empty() && !b.mIdentifier.empty()) {
            if (doesNameMatch(a.mIdentifier, b.mIdentifier)) {
                return true;
            }
        }

        // Check alias matches
        if (!a.aliases.empty() && !b.aliases.empty()) {
            // Simple check for now - both can have the same identifier in aliases
            auto normalizedAliasesA = normalizeString(a.aliases);
            auto normalizedAliasesB = normalizeString(b.aliases);
            if (normalizedAliasesA == normalizedAliasesB) {
                return true;
            }
        }

        return false;
    }

    MergeConfig config_;
    MergeStats stats_;
};

// ============================================================================
// ResultMerger Implementation
// ============================================================================

ResultMerger::ResultMerger(const MergeConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {}

ResultMerger::~ResultMerger() = default;

ResultMerger::ResultMerger(ResultMerger&&) noexcept = default;

ResultMerger& ResultMerger::operator=(ResultMerger&&) noexcept = default;

auto ResultMerger::merge(
    const std::vector<CelestialObjectModel>& localResults,
    const std::vector<CelestialObjectModel>& onlineResults)
    -> std::vector<CelestialObjectModel> {
    pImpl_->stats_.localCount = localResults.size();
    pImpl_->stats_.onlineCount = onlineResults.size();
    pImpl_->stats_.mergedCount = 0;
    pImpl_->stats_.duplicatesRemoved = 0;
    pImpl_->stats_.conflictsResolved = 0;

    if (pImpl_->config_.strategy == MergeStrategy::Union) {
        // For Union strategy, combine all results and remove duplicates
        std::vector<CelestialObjectModel> result = localResults;
        result.insert(result.end(), onlineResults.begin(),
                      onlineResults.end());

        if (pImpl_->config_.removeDuplicates) {
            // Remove duplicates
            std::unordered_set<std::string> seen;
            std::vector<CelestialObjectModel> deduplicated;

            for (const auto& obj : result) {
                std::string key = obj.identifier.empty() ? obj.mIdentifier
                                                         : obj.identifier;
                if (key.empty()) {
                    // Use coordinates as key if no identifier
                    key = std::to_string(obj.radJ2000) + "_" +
                          std::to_string(obj.decDJ2000);
                }

                if (seen.find(key) == seen.end()) {
                    seen.insert(key);
                    deduplicated.push_back(obj);
                } else {
                    pImpl_->stats_.duplicatesRemoved++;
                }
            }

            pImpl_->stats_.mergedCount = deduplicated.size();
            return deduplicated;
        } else {
            pImpl_->stats_.mergedCount = result.size();
            return result;
        }
    }

    // For other strategies, match and merge duplicates
    std::vector<CelestialObjectModel> merged;
    std::unordered_set<size_t> usedOnlineIndices;

    // First pass: find and merge duplicates
    for (const auto& local : localResults) {
        CelestialObjectModel current = local;
        bool found = false;

        for (size_t i = 0; i < onlineResults.size(); ++i) {
            if (usedOnlineIndices.find(i) != usedOnlineIndices.end()) {
                continue;
            }

            const auto& online = onlineResults[i];

            // Check for duplicates
            if (isDuplicate(local, online)) {
                current = mergeObjects(local, online);
                usedOnlineIndices.insert(i);
                pImpl_->stats_.conflictsResolved++;
                found = true;
                break;
            }
        }

        merged.push_back(current);
    }

    // Second pass: add remaining online results
    for (size_t i = 0; i < onlineResults.size(); ++i) {
        if (usedOnlineIndices.find(i) == usedOnlineIndices.end()) {
            merged.push_back(onlineResults[i]);
        }
    }

    pImpl_->stats_.mergedCount = merged.size();

    // Apply max results limit
    if (static_cast<int>(merged.size()) > pImpl_->config_.maxResults) {
        merged.resize(pImpl_->config_.maxResults);
    }

    return merged;
}

auto ResultMerger::mergeScored(
    const std::vector<model::ScoredSearchResult>& localResults,
    const std::vector<CelestialObjectModel>& onlineResults,
    double baseOnlineScore) -> std::vector<model::ScoredSearchResult> {
    pImpl_->stats_.localCount = localResults.size();
    pImpl_->stats_.onlineCount = onlineResults.size();
    pImpl_->stats_.mergedCount = 0;
    pImpl_->stats_.duplicatesRemoved = 0;
    pImpl_->stats_.conflictsResolved = 0;

    std::vector<model::ScoredSearchResult> result;
    result.reserve(localResults.size() + onlineResults.size());

    // Add scored local results with bonus
    for (const auto& scored : localResults) {
        auto adjusted = scored;
        adjusted.relevanceScore =
            std::min(1.0, adjusted.relevanceScore + pImpl_->config_.localScoreBonus);
        if (adjusted.relevanceScore >= pImpl_->config_.minScore) {
            result.push_back(adjusted);
        }
    }

    // Track online results that were merged with local
    std::unordered_set<std::string> mergedOnlineKeys;

    // Try to merge online results with local results
    for (auto& scored : result) {
        if (!scored.object) {
            continue;
        }

        // Convert object to model for comparison
        CelestialObjectModel scoreObj;
        scoreObj.identifier = scored.object->Identifier;
        scoreObj.radJ2000 = scored.object->RADJ2000;
        scoreObj.decDJ2000 = scored.object->DecDJ2000;

        for (size_t i = 0; i < onlineResults.size(); ++i) {
            const auto& online = onlineResults[i];

            if (isDuplicate(scoreObj, online)) {
                // Found a duplicate - merge fields
                auto merged = mergeObjects(scoreObj, online);
                scored.relevanceScore =
                    std::min(1.0, scored.relevanceScore +
                             pImpl_->config_.onlineScoreBonus);

                // Create key to track this online result
                std::string key = online.identifier.empty() ? online.mIdentifier
                                                            : online.identifier;
                mergedOnlineKeys.insert(key);
                pImpl_->stats_.conflictsResolved++;
                break;
            }
        }
    }

    // Add online results that weren't merged
    for (const auto& online : onlineResults) {
        std::string key = online.identifier.empty() ? online.mIdentifier
                                                    : online.identifier;
        if (mergedOnlineKeys.find(key) == mergedOnlineKeys.end()) {
            model::ScoredSearchResult newResult;
            double score = baseOnlineScore + pImpl_->config_.onlineScoreBonus;
            newResult.relevanceScore = std::min(1.0, score);

            if (newResult.relevanceScore >= pImpl_->config_.minScore) {
                result.push_back(newResult);
            }
        }
    }

    // Sort by relevance score (descending)
    std::sort(result.begin(), result.end(),
              [](const model::ScoredSearchResult& a,
                 const model::ScoredSearchResult& b) {
                  return a.relevanceScore > b.relevanceScore;
              });

    // Apply max results limit
    if (static_cast<int>(result.size()) > pImpl_->config_.maxResults) {
        result.resize(pImpl_->config_.maxResults);
    }

    pImpl_->stats_.mergedCount = result.size();
    return result;
}

auto ResultMerger::mergeMultiple(
    const std::vector<OnlineQueryResult>& results)
    -> std::vector<CelestialObjectModel> {
    // Combine all objects from all providers
    std::vector<CelestialObjectModel> allObjects;
    pImpl_->stats_.onlineCount = 0;

    for (const auto& result : results) {
        allObjects.insert(allObjects.end(), result.objects.begin(),
                          result.objects.end());
        pImpl_->stats_.onlineCount += result.objects.size();
    }

    // Empty vector for local results
    std::vector<CelestialObjectModel> emptyLocal;

    // Use regular merge with empty local results
    return merge(emptyLocal, allObjects);
}

auto ResultMerger::getLastMergeStats() const -> MergeStats {
    return pImpl_->stats_;
}

void ResultMerger::setConfig(const MergeConfig& config) {
    pImpl_->config_ = config;
}

auto ResultMerger::getConfig() const -> const MergeConfig& {
    return pImpl_->config_;
}

auto ResultMerger::isDuplicate(const CelestialObjectModel& a,
                               const CelestialObjectModel& b) const -> bool {
    // Check for identifier matches first
    if (pImpl_->isIdentifierDuplicate(a, b)) {
        return true;
    }

    // Check for coordinate matches
    if (pImpl_->doesCoordinateMatch(a, b)) {
        return true;
    }

    // Check for cross-matches (e.g., a.identifier matches b.mIdentifier)
    if (pImpl_->config_.matchByName) {
        if (!a.identifier.empty() && !b.mIdentifier.empty()) {
            if (pImpl_->doesNameMatch(a.identifier, b.mIdentifier)) {
                return true;
            }
        }
        if (!a.mIdentifier.empty() && !b.identifier.empty()) {
            if (pImpl_->doesNameMatch(a.mIdentifier, b.identifier)) {
                return true;
            }
        }
    }

    return false;
}

auto ResultMerger::mergeObjects(const CelestialObjectModel& primary,
                                const CelestialObjectModel& secondary) const
    -> CelestialObjectModel {
    return pImpl_->mergeObjectsInternal(primary, secondary);
}

}  // namespace lithium::target::online
