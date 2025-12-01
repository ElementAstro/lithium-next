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

#include "open_ngc_provider.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace lithium::target::online {

namespace {

/**
 * @brief Celestial object from OpenNGC CSV
 */
struct OpenNgcObject {
    std::string name;          // NGC/IC identifier
    std::string type;          // Object type (Gx, Pn, etc.)
    double raJ2000 = 0.0;      // RA in degrees
    double decJ2000 = 0.0;     // Dec in degrees
    std::string constellation; // Constellation code
    double majorAxis = 0.0;    // Major axis in arcmin
    double minorAxis = 0.0;    // Minor axis in arcmin
    double positionAngle = 0.0;
    double bMagnitude = 0.0;   // B magnitude
    double vMagnitude = 0.0;   // V magnitude
    double surfBrightness = 0.0;
    std::string hubbleType;
    std::string messier;       // Messier number if applicable
    std::string ngc;           // NGC number
    std::string ic;            // IC number
};

/**
 * @brief Convert RA string (HH:MM:SS.S) to degrees
 */
auto parseRA(const std::string& raStr) -> double {
    // Handle format: "HH:MM:SS.SS"
    if (raStr.empty() || raStr == "N/A" || raStr == "") {
        return 0.0;
    }

    try {
        size_t pos1 = raStr.find(':');
        size_t pos2 = raStr.rfind(':');

        if (pos1 == std::string::npos || pos2 == std::string::npos) {
            return 0.0;
        }

        double hours = std::stod(raStr.substr(0, pos1));
        double minutes = std::stod(raStr.substr(pos1 + 1, pos2 - pos1 - 1));
        double seconds = std::stod(raStr.substr(pos2 + 1));

        return (hours + minutes / 60.0 + seconds / 3600.0) * 15.0;  // Convert to degrees
    } catch (const std::exception& e) {
        spdlog::debug("Failed to parse RA: {} - {}", raStr, e.what());
        return 0.0;
    }
}

/**
 * @brief Convert Dec string (+/-DD:MM:SS.S) to degrees
 */
auto parseDec(const std::string& decStr) -> double {
    // Handle format: "+DD:MM:SS.SS" or "-DD:MM:SS.SS"
    if (decStr.empty() || decStr == "N/A" || decStr == "") {
        return 0.0;
    }

    try {
        bool negative = decStr[0] == '-';
        std::string workStr = decStr;
        if (workStr[0] == '+' || workStr[0] == '-') {
            workStr = workStr.substr(1);
        }

        size_t pos1 = workStr.find(':');
        size_t pos2 = workStr.rfind(':');

        if (pos1 == std::string::npos || pos2 == std::string::npos) {
            return 0.0;
        }

        double degrees = std::stod(workStr.substr(0, pos1));
        double minutes = std::stod(workStr.substr(pos1 + 1, pos2 - pos1 - 1));
        double seconds = std::stod(workStr.substr(pos2 + 1));

        double result = degrees + minutes / 60.0 + seconds / 3600.0;
        return negative ? -result : result;
    } catch (const std::exception& e) {
        spdlog::debug("Failed to parse Dec: {} - {}", decStr, e.what());
        return 0.0;
    }
}

/**
 * @brief Parse double value with error handling
 */
auto parseDouble(const std::string& str) -> double {
    if (str.empty() || str == "N/A") {
        return 0.0;
    }
    try {
        return std::stod(str);
    } catch (const std::exception&) {
        return 0.0;
    }
}

/**
 * @brief Trim whitespace from string
 */
auto trim(const std::string& str) -> std::string {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        ++start;
    }

    auto end = str.end();
    do {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::string(start, end + 1);
}

/**
 * @brief Split CSV line handling quoted fields and semicolon delimiters
 */
auto splitCSVLine(const std::string& line) -> std::vector<std::string> {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];

        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ';' && !inQuotes) {
            fields.push_back(trim(field));
            field.clear();
        } else {
            field += c;
        }
    }

    if (!field.empty()) {
        fields.push_back(trim(field));
    }

    return fields;
}

/**
 * @brief Parse OpenNGC CSV line
 *
 * CSV columns (semicolon-delimited):
 * 0: Name (NGC/IC)
 * 1: Type (Gx, Pn, etc.)
 * 2: RA (HH:MM:SS.S)
 * 3: Dec (+/-DD:MM:SS.S)
 * 4: Const (constellation)
 * 5: MajAx (major axis in arcmin)
 * 6: MinAx (minor axis in arcmin)
 * 7: PosAng (position angle)
 * 8: B-Mag (B magnitude)
 * 9: V-Mag (V magnitude)
 * 10: SurfBr (surface brightness)
 * 11: Hubble (Hubble type)
 * 12: Messier (Messier number)
 * 13: NGC (NGC cross-ref)
 * 14: IC (IC cross-ref)
 */
auto parseOpenNgcLine(const std::string& line) -> std::optional<OpenNgcObject> {
    if (line.empty() || line[0] == '#') {
        return std::nullopt;
    }

    auto fields = splitCSVLine(line);

    if (fields.size() < 10) {
        spdlog::debug("Invalid OpenNGC line: not enough fields");
        return std::nullopt;
    }

    OpenNgcObject obj;
    try {
        obj.name = trim(fields[0]);
        obj.type = trim(fields[1]);
        obj.raJ2000 = parseRA(fields[2]);
        obj.decJ2000 = parseDec(fields[3]);
        obj.constellation = trim(fields[4]);
        obj.majorAxis = parseDouble(fields[5]);
        obj.minorAxis = parseDouble(fields[6]);
        obj.positionAngle = parseDouble(fields[7]);
        obj.bMagnitude = parseDouble(fields[8]);
        obj.vMagnitude = parseDouble(fields[9]);

        if (fields.size() > 10) {
            obj.surfBrightness = parseDouble(fields[10]);
        }
        if (fields.size() > 11) {
            obj.hubbleType = trim(fields[11]);
        }
        if (fields.size() > 12) {
            obj.messier = trim(fields[12]);
        }
        if (fields.size() > 13) {
            obj.ngc = trim(fields[13]);
        }
        if (fields.size() > 14) {
            obj.ic = trim(fields[14]);
        }

        if (obj.name.empty()) {
            return std::nullopt;
        }

        return obj;
    } catch (const std::exception& e) {
        spdlog::debug("Failed to parse OpenNGC line: {}", e.what());
        return std::nullopt;
    }
}

/**
 * @brief Convert OpenNgcObject to CelestialObjectModel
 */
auto convertToCelestialModel(const OpenNgcObject& obj)
    -> CelestialObjectModel {
    CelestialObjectModel model;
    model.identifier = obj.name;
    model.type = obj.type;
    model.raJ2000 = std::to_string(obj.raJ2000);
    model.radJ2000 = obj.raJ2000;
    model.decJ2000 = std::to_string(obj.decJ2000);
    model.decDJ2000 = obj.decJ2000;
    model.constellationEn = obj.constellation;
    model.majorAxis = obj.majorAxis;
    model.minorAxis = obj.minorAxis;
    model.positionAngle = obj.positionAngle;
    model.visualMagnitudeV = obj.vMagnitude;
    model.photographicMagnitudeB = obj.bMagnitude;
    model.surfaceBrightness = obj.surfBrightness;
    model.morphology = obj.hubbleType;
    model.mIdentifier = obj.messier;

    // Build aliases from cross-references
    std::string aliases;
    if (!obj.messier.empty()) {
        aliases += "M" + obj.messier;
    }
    if (!obj.ngc.empty()) {
        if (!aliases.empty()) aliases += ",";
        aliases += "NGC" + obj.ngc;
    }
    if (!obj.ic.empty()) {
        if (!aliases.empty()) aliases += ",";
        aliases += "IC" + obj.ic;
    }
    model.aliases = aliases;

    return model;
}

/**
 * @brief Calculate angular distance between two celestial objects
 */
auto angularDistance(double ra1, double dec1, double ra2, double dec2)
    -> double {
    // Convert to radians
    auto toRad = [](double deg) { return deg * M_PI / 180.0; };

    double ra1Rad = toRad(ra1);
    double dec1Rad = toRad(dec1);
    double ra2Rad = toRad(ra2);
    double dec2Rad = toRad(dec2);

    // Haversine formula
    double dRa = ra2Rad - ra1Rad;
    double dDec = dec2Rad - dec1Rad;

    double a = std::sin(dDec / 2.0) * std::sin(dDec / 2.0) +
               std::cos(dec1Rad) * std::cos(dec2Rad) *
                   std::sin(dRa / 2.0) * std::sin(dRa / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    double distance = c * (180.0 / M_PI);  // Convert back to degrees

    return distance;
}

}  // namespace

/**
 * @brief OpenNgcProvider implementation
 */
class OpenNgcProvider::Impl {
public:
    Impl(std::shared_ptr<AsyncHttpClient> httpClient,
         std::shared_ptr<QueryCache> cache,
         const OpenNgcProviderConfig& config)
        : httpClient_(httpClient),
          cache_(cache),
          config_(config),
          catalogLoaded_(false),
          lastRefresh_(std::chrono::system_clock::now()) {
        // Initial catalog load
        loadCatalogData();
    }

    ~Impl() = default;

    auto query(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
        if (!catalogLoaded_) {
            return atom::type::Unexpected(OnlineQueryError{
                OnlineQueryError::Code::ServiceUnavailable,
                "OpenNGC catalog not loaded",
                "OpenNGC",
                std::nullopt,
                std::nullopt});
        }

        // Check cache
        if (config_.useCache && cache_) {
            std::string cacheKey =
                QueryCache::generateKey("OpenNGC", params);
            if (auto cached = cache_->get(cacheKey)) {
                cached->fromCache = true;
                return cached.value();
            }
        }

        auto result = queryInternal(params);
        if (result) {
            result->provider = "OpenNGC";
            result->queryTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - queryStart_);

            // Store in cache
            if (config_.useCache && cache_) {
                std::string cacheKey =
                    QueryCache::generateKey("OpenNGC", params);
                cache_->put(cacheKey, result.value(),
                            config_.cacheTTL);
            }

            return result.value();
        } else {
            return result.error();
        }
    }

    auto queryAsync(const OnlineQueryParams& params)
        -> std::future<atom::type::Expected<OnlineQueryResult,
                                            OnlineQueryError>> {
        return std::async(std::launch::async,
                          [this, params]() { return this->query(params); });
    }

    auto isAvailable() const -> bool {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return catalogLoaded_ && !objects_.empty();
    }

    auto refreshCatalog() -> atom::type::Expected<void, OnlineQueryError> {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return loadCatalogDataLocked();
    }

    auto getCatalogStats() const
        -> std::pair<size_t, std::chrono::system_clock::time_point> {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return {objects_.size(), lastRefresh_};
    }

    void setConfig(const OpenNgcProviderConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        config_ = config;
    }

    auto getConfig() const -> const OpenNgcProviderConfig& {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_;
    }

private:
    std::shared_ptr<AsyncHttpClient> httpClient_;
    std::shared_ptr<QueryCache> cache_;
    OpenNgcProviderConfig config_;

    std::vector<OpenNgcObject> objects_;
    std::unordered_map<std::string, size_t> nameIndex_;
    bool catalogLoaded_;
    std::chrono::system_clock::time_point lastRefresh_;
    std::chrono::steady_clock::time_point queryStart_;

    mutable std::shared_mutex mutex_;

    auto loadCatalogData() -> void {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto result = loadCatalogDataLocked();
        if (!result) {
            spdlog::warn("Failed to load OpenNGC catalog: {}",
                         result.error().message);
        }
    }

    auto loadCatalogDataLocked()
        -> atom::type::Expected<void, OnlineQueryError> {
        try {
            // Download CSV data
            HttpRequest req;
            req.url = config_.dataUrl;
            req.timeout = config_.timeout;
            req.method = "GET";

            auto response = httpClient_->get(req.url, req.timeout);
            if (!response) {
                return atom::type::Unexpected(OnlineQueryError{
                    OnlineQueryError::Code::NetworkError,
                    response.error(),
                    "OpenNGC",
                    std::nullopt,
                    std::nullopt});
            }

            if (response->statusCode != 200) {
                return atom::type::Unexpected(OnlineQueryError{
                    OnlineQueryError::Code::ServiceUnavailable,
                    "HTTP " + std::to_string(response->statusCode),
                    "OpenNGC",
                    std::nullopt,
                    std::nullopt});
            }

            // Parse CSV
            objects_.clear();
            nameIndex_.clear();

            std::istringstream stream(response->body);
            std::string line;
            size_t lineNum = 0;

            while (std::getline(stream, line)) {
                ++lineNum;

                // Skip header and comment lines
                if (lineNum == 1 || line.empty() || line[0] == '#') {
                    continue;
                }

                auto obj = parseOpenNgcLine(line);
                if (obj) {
                    size_t index = objects_.size();
                    nameIndex_[obj->name] = index;

                    // Also index Messier number if present
                    if (!obj->messier.empty()) {
                        nameIndex_["M" + obj->messier] = index;
                    }

                    objects_.push_back(obj.value());
                }
            }

            catalogLoaded_ = true;
            lastRefresh_ = std::chrono::system_clock::now();

            spdlog::info("Loaded OpenNGC catalog with {} objects",
                         objects_.size());
            return {};
        } catch (const std::exception& e) {
            catalogLoaded_ = false;
            return atom::type::Unexpected(OnlineQueryError{
                OnlineQueryError::Code::ParseError,
                std::string("Failed to parse OpenNGC CSV: ") + e.what(),
                "OpenNGC",
                std::nullopt,
                std::nullopt});
        }
    }

    auto queryInternal(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
        queryStart_ = std::chrono::steady_clock::now();

        std::shared_lock<std::shared_mutex> lock(mutex_);

        OnlineQueryResult result;
        result.provider = "OpenNGC";
        result.fromCache = false;

        try {
            switch (params.type) {
            case QueryType::ByName:
                return queryByName(params);
            case QueryType::ByCoordinates:
                return queryByCoordinates(params);
            case QueryType::Catalog:
                return queryCatalog(params);
            default:
                return atom::type::Unexpected(OnlineQueryError{
                    OnlineQueryError::Code::InvalidQuery,
                    "Unsupported query type",
                    "OpenNGC",
                    std::nullopt,
                    std::nullopt});
            }
        } catch (const std::exception& e) {
            spdlog::error("Query error: {}", e.what());
            return atom::type::Unexpected(OnlineQueryError{
                OnlineQueryError::Code::Unknown,
                e.what(),
                "OpenNGC",
                std::nullopt,
                std::nullopt});
        }
    }

    auto queryByName(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
        OnlineQueryResult result;
        result.provider = "OpenNGC";

        // Search by exact name and prefix matches
        std::string queryLower = params.query;
        std::transform(queryLower.begin(), queryLower.end(),
                       queryLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        size_t found = 0;

        for (size_t i = 0; i < objects_.size() && found < params.limit; ++i) {
            const auto& obj = objects_[i];
            std::string nameLower = obj.name;
            std::transform(nameLower.begin(), nameLower.end(),
                           nameLower.begin(), [](unsigned char c) {
                               return std::tolower(c);
                           });

            // Exact match or prefix match
            if (nameLower == queryLower ||
                nameLower.substr(0, queryLower.length()) == queryLower) {
                result.objects.push_back(convertToCelestialModel(obj));
                ++found;
            }
        }

        if (result.objects.empty()) {
            return atom::type::Unexpected(OnlineQueryError{
                OnlineQueryError::Code::NotFound,
                "No objects found matching: " + params.query,
                "OpenNGC",
                std::nullopt,
                std::nullopt});
        }

        return result;
    }

    auto queryByCoordinates(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
        OnlineQueryResult result;
        result.provider = "OpenNGC";

        if (!params.ra.has_value() || !params.dec.has_value() ||
            !params.radius.has_value()) {
            return atom::type::Unexpected(OnlineQueryError{
                OnlineQueryError::Code::InvalidQuery,
                "Coordinate search requires ra, dec, and radius",
                "OpenNGC",
                std::nullopt,
                std::nullopt});
        }

        double searchRA = params.ra.value();
        double searchDec = params.dec.value();
        double searchRadius = params.radius.value();

        std::vector<std::pair<double, size_t>> candidates;

        for (size_t i = 0; i < objects_.size(); ++i) {
            const auto& obj = objects_[i];
            double dist =
                angularDistance(searchRA, searchDec, obj.raJ2000, obj.decJ2000);

            if (dist <= searchRadius) {
                candidates.push_back({dist, i});
            }
        }

        // Sort by distance
        std::sort(candidates.begin(), candidates.end());

        size_t limit = std::min(static_cast<size_t>(params.limit), candidates.size());
        for (size_t i = 0; i < limit; ++i) {
            result.objects.push_back(
                convertToCelestialModel(objects_[candidates[i].second]));
        }

        if (result.objects.empty()) {
            return atom::type::Unexpected(OnlineQueryError{
                OnlineQueryError::Code::NotFound,
                "No objects found in radius",
                "OpenNGC",
                std::nullopt,
                std::nullopt});
        }

        result.totalAvailable = candidates.size();
        return result;
    }

    auto queryCatalog(const OnlineQueryParams& params)
        -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
        OnlineQueryResult result;
        result.provider = "OpenNGC";

        if (!params.catalog.has_value()) {
            return atom::type::Unexpected(OnlineQueryError{
                OnlineQueryError::Code::InvalidQuery,
                "Catalog search requires catalog parameter",
                "OpenNGC",
                std::nullopt,
                std::nullopt});
        }

        std::string catalogLower = params.catalog.value();
        std::transform(catalogLower.begin(), catalogLower.end(),
                       catalogLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        size_t found = 0;
        for (size_t i = 0; i < objects_.size() && found < params.limit; ++i) {
            const auto& obj = objects_[i];
            std::string nameLower = obj.name;
            std::transform(nameLower.begin(), nameLower.end(),
                           nameLower.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            // Match catalog prefix (NGC, IC, M)
            if (nameLower.substr(0, catalogLower.length()) == catalogLower) {
                result.objects.push_back(convertToCelestialModel(obj));
                ++found;
            }
        }

        if (result.objects.empty()) {
            return atom::type::Unexpected(OnlineQueryError{
                OnlineQueryError::Code::NotFound,
                "No objects found in catalog: " + params.catalog.value(),
                "OpenNGC",
                std::nullopt,
                std::nullopt});
        }

        return result;
    }
};

// Public interface implementation

OpenNgcProvider::OpenNgcProvider(
    std::shared_ptr<AsyncHttpClient> httpClient,
    std::shared_ptr<QueryCache> cache,
    const OpenNgcProviderConfig& config)
    : pImpl_(std::make_unique<Impl>(httpClient, cache, config)) {}

OpenNgcProvider::~OpenNgcProvider() = default;

OpenNgcProvider::OpenNgcProvider(OpenNgcProvider&&) noexcept = default;
OpenNgcProvider& OpenNgcProvider::operator=(OpenNgcProvider&&) noexcept =
    default;

auto OpenNgcProvider::query(const OnlineQueryParams& params)
    -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
    return pImpl_->query(params);
}

auto OpenNgcProvider::queryAsync(const OnlineQueryParams& params)
    -> std::future<atom::type::Expected<OnlineQueryResult,
                                        OnlineQueryError>> {
    return pImpl_->queryAsync(params);
}

auto OpenNgcProvider::isAvailable() const -> bool {
    return pImpl_->isAvailable();
}

auto OpenNgcProvider::refreshCatalog()
    -> atom::type::Expected<void, OnlineQueryError> {
    return pImpl_->refreshCatalog();
}

auto OpenNgcProvider::getCatalogStats() const
    -> std::pair<size_t, std::chrono::system_clock::time_point> {
    return pImpl_->getCatalogStats();
}

void OpenNgcProvider::setConfig(const OpenNgcProviderConfig& config) {
    pImpl_->setConfig(config);
}

auto OpenNgcProvider::getConfig() const -> const OpenNgcProviderConfig& {
    return pImpl_->getConfig();
}

}  // namespace lithium::target::online
