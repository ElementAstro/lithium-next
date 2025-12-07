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

#include "online.hpp"

namespace lithium::target::online {

/**
 * @brief Default implementation of OnlineSearchService
 *
 * Provides basic stub implementation for online search operations.
 * Can be extended with actual API integrations.
 */
class DefaultOnlineSearchService : public OnlineSearchService {
public:
    auto initialize(const OnlineSearchConfig& config)
        -> atom::type::Expected<void, std::string> override {
        config_ = config;
        return {};
    }

    auto searchByName(const std::string& query, int limit = 50)
        -> std::vector<std::string> override {
        // Stub implementation
        return {};
    }

    auto searchByCoordinates(double ra, double dec, double radiusDeg,
                             int limit = 100)
        -> std::vector<std::string> override {
        // Stub implementation
        return {};
    }

    auto getEphemeris(const std::string& objectName,
                      std::chrono::system_clock::time_point time)
        -> std::optional<EphemerisPoint> override {
        // Stub implementation
        return std::nullopt;
    }

    auto getObjectDetails(const std::string& identifier)
        -> std::optional<std::string> override {
        // Stub implementation
        return std::nullopt;
    }

private:
    OnlineSearchConfig config_;
};

/**
 * @brief Default implementation of ResultMerger
 */
class DefaultResultMerger : public ResultMerger {
public:
    auto mergeResults(const std::vector<std::string>& localResults,
                      const std::vector<std::string>& onlineResults)
        -> std::vector<std::string> override {
        // Merge and deduplicate results
        std::vector<std::string> merged = localResults;
        for (const auto& result : onlineResults) {
            auto it = std::find(merged.begin(), merged.end(), result);
            if (it == merged.end()) {
                merged.push_back(result);
            }
        }
        return merged;
    }
};

auto OnlineSearchServiceFactory::createService(const std::string& serviceType)
    -> std::shared_ptr<OnlineSearchService> {
    // Return default implementation for all service types
    return std::make_shared<DefaultOnlineSearchService>();
}

}  // namespace lithium::target::online
