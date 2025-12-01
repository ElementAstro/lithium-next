// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "ned_provider.hpp"

#include <chrono>
#include <sstream>
#include <thread>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>
#include <future>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace lithium::target::online {

namespace {

/**
 * @brief URL encode a string for HTTP transmission
 *
 * @param input String to encode
 * @return URL-encoded string
 */
auto urlEncode(const std::string& input) -> std::string {
    std::ostringstream encoded;
    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else if (c == ' ') {
            encoded << "%20";
        } else {
            encoded << '%' << std::hex << std::setfill('0') << std::setw(2)
                    << static_cast<int>(c);
        }
    }
    return encoded.str();
}

/**
 * @brief Normalize object name for NED query
 *
 * @param name Object name
 * @return Normalized name
 */
auto normalizeObjectName(std::string name) -> std::string {
    // Convert to uppercase
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return name;
}

/**
 * @brief Parse NED VOTable response
 *
 * Extracts NED fields from VOTable XML response
 *
 * @param xml VOTable XML string
 * @param result Output query result
 * @return True on success
 */
auto parseVotableResponse(const std::string& xml, OnlineQueryResult& result) -> bool {
    try {
        // Find the TABLEDATA section
        size_t tableStart = xml.find("<TABLEDATA>");
        size_t tableEnd = xml.find("</TABLEDATA>");

        if (tableStart == std::string::npos || tableEnd == std::string::npos) {
            spdlog::warn("No TABLEDATA found in NED response");
            return true;  // Return empty result, not an error
        }

        std::string tableContent = xml.substr(tableStart + 11, tableEnd - tableStart - 11);

        // Find all TR (table row) elements
        size_t pos = 0;
        while ((pos = tableContent.find("<TR>", pos)) != std::string::npos) {
            size_t rowEnd = tableContent.find("</TR>", pos);
            if (rowEnd == std::string::npos) {
                break;
            }

            std::string rowContent = tableContent.substr(pos + 4, rowEnd - pos - 4);

            // Extract TD (table data) values
            std::vector<std::string> fields;
            size_t tdPos = 0;
            while ((tdPos = rowContent.find("<TD>", tdPos)) != std::string::npos) {
                size_t tdEnd = rowContent.find("</TD>", tdPos);
                if (tdEnd == std::string::npos) {
                    break;
                }

                std::string field =
                    rowContent.substr(tdPos + 4, tdEnd - tdPos - 4);

                // Trim whitespace
                field.erase(0, field.find_first_not_of(" \t\n\r"));
                field.erase(field.find_last_not_of(" \t\n\r") + 1);

                fields.push_back(field);
                tdPos = tdEnd + 5;
            }

            if (!fields.empty()) {
                CelestialObjectModel obj;

                // Map NED fields to CelestialObjectModel
                // Expected field order (from ADQL query):
                // 0: name
                // 1: ra
                // 2: dec
                // 3: object_type
                // 4: morphology
                // 5: redshift (z)
                // 6: photometry magnitude
                // 7: distance modulus

                if (fields.size() > 0) {
                    obj.identifier = fields[0];
                    obj.mIdentifier = fields[0];
                }

                if (fields.size() > 1) {
                    try {
                        obj.radJ2000 = std::stod(fields[1]);
                        obj.raJ2000 = fields[1];
                    } catch (const std::exception& e) {
                        spdlog::warn("Failed to parse RA: {}", fields[1]);
                    }
                }

                if (fields.size() > 2) {
                    try {
                        obj.decDJ2000 = std::stod(fields[2]);
                        obj.decJ2000 = fields[2];
                    } catch (const std::exception& e) {
                        spdlog::warn("Failed to parse Dec: {}", fields[2]);
                    }
                }

                if (fields.size() > 3) {
                    obj.type = fields[3];  // Object type
                }

                if (fields.size() > 4) {
                    obj.morphology = fields[4];  // Morphological type
                }

                if (fields.size() > 5) {
                    // Redshift - store in aliases for now
                    obj.aliases = "z=" + fields[5];
                }

                if (fields.size() > 6) {
                    try {
                        obj.visualMagnitudeV = std::stod(fields[6]);
                    } catch (const std::exception& e) {
                        spdlog::debug("Failed to parse magnitude: {}", fields[6]);
                    }
                }

                if (fields.size() > 7) {
                    try {
                        obj.surfaceBrightness = std::stod(fields[7]);
                    } catch (const std::exception& e) {
                        spdlog::debug("Failed to parse surface brightness: {}", fields[7]);
                    }
                }

                result.objects.push_back(obj);
            }

            pos = rowEnd + 5;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error parsing NED VOTable response: {}", e.what());
        return false;
    }
}

}  // namespace

/**
 * @brief Implementation class for NedProvider
 */
class NedProvider::Impl {
public:
    explicit Impl(std::shared_ptr<AsyncHttpClient> httpClient,
                  std::shared_ptr<QueryCache> cache,
                  std::shared_ptr<ApiRateLimiter> rateLimiter,
                  const NedProviderConfig& config)
        : httpClient_(std::move(httpClient)),
          cache_(std::move(cache)),
          rateLimiter_(std::move(rateLimiter)),
          config_(config) {}

    NedProviderConfig config_;
    std::shared_ptr<AsyncHttpClient> httpClient_;
    std::shared_ptr<QueryCache> cache_;
    std::shared_ptr<ApiRateLimiter> rateLimiter_;
};

// Constructor
NedProvider::NedProvider(
    std::shared_ptr<AsyncHttpClient> httpClient,
    std::shared_ptr<QueryCache> cache,
    std::shared_ptr<ApiRateLimiter> rateLimiter,
    const NedProviderConfig& config)
    : pImpl_(std::make_unique<Impl>(std::move(httpClient), std::move(cache),
                                   std::move(rateLimiter), config)) {
    spdlog::info("Initializing NED provider");
}

// Destructor
NedProvider::~NedProvider() = default;

// Move constructor
NedProvider::NedProvider(NedProvider&&) noexcept = default;

// Move assignment operator
NedProvider& NedProvider::operator=(NedProvider&&) noexcept = default;

auto NedProvider::isAvailable() const -> bool {
    if (!pImpl_ || !pImpl_->httpClient_) {
        spdlog::warn("NED provider not properly initialized");
        return false;
    }

    try {
        // Perform a simple health check with a known object
        OnlineQueryParams params;
        params.type = QueryType::ByName;
        params.query = "M31";  // Andromeda Galaxy - should always be found
        params.limit = 1;

        auto adql = buildAdqlQuery(params);
        HttpRequest request;
        request.url = std::string(BASE_URL) + "?request=doQuery&lang=adql&format=votable&query=" +
                      urlEncode(adql);
        request.method = "GET";
        request.timeout = std::chrono::milliseconds{5000};

        auto response = pImpl_->httpClient_->request(request);

        if (!response) {
            spdlog::warn("NED health check failed: {}", response.error());
            return false;
        }

        return response.value().statusCode == 200;
    } catch (const std::exception& e) {
        spdlog::warn("NED health check error: {}", e.what());
        return false;
    }
}

auto NedProvider::buildAdqlQuery(const OnlineQueryParams& params) const -> std::string {
    std::ostringstream query;

    // Base SELECT clause with NED columns
    // NED TAP service provides: name, ra, dec, obj_type, morphology, redshift, mag, mag_err, etc.
    query << "SELECT name, ra, dec, obj_type, morphology, z, "
             "mag, mag_err FROM ned_objects ";

    switch (params.type) {
    case QueryType::ByName: {
        if (params.query.empty()) {
            throw std::invalid_argument("Query string required for ByName search");
        }

        // Normalize the name for NED
        auto normalizedName = normalizeObjectName(params.query);

        // NED supports prefix matching and exact matching
        query << "WHERE name LIKE '" << normalizedName << "%'";

        if (params.limit > 0) {
            query << " LIMIT " << params.limit;
        }
        break;
    }

    case QueryType::ByCoordinates: {
        if (!params.ra.has_value() || !params.dec.has_value()) {
            throw std::invalid_argument(
                "RA and Dec required for ByCoordinates search");
        }

        double radius = params.radius.value_or(0.5);  // Default 0.5 degrees

        // Use NED's CONTAINS function for cone search
        query << "WHERE CONTAINS(POINT('ICRS', ra, dec), "
              << "CIRCLE('ICRS', " << std::fixed << std::setprecision(6)
              << params.ra.value() << ", " << params.dec.value() << ", "
              << radius << ")) = 1";

        // Optional magnitude filtering
        if (params.minMagnitude.has_value() || params.maxMagnitude.has_value()) {
            if (params.minMagnitude.has_value()) {
                query << " AND mag >= " << std::fixed << std::setprecision(2)
                      << params.minMagnitude.value();
            }
            if (params.maxMagnitude.has_value()) {
                query << " AND mag <= " << std::fixed << std::setprecision(2)
                      << params.maxMagnitude.value();
            }
        }

        // Optional object type filtering
        if (params.objectType.has_value() && !params.objectType.value().empty()) {
            query << " AND obj_type = '" << params.objectType.value() << "'";
        }

        if (params.limit > 0) {
            query << " LIMIT " << params.limit;
        }
        break;
    }

    case QueryType::Catalog:
    case QueryType::ByConstellation:
    case QueryType::Ephemeris:
    default:
        throw std::invalid_argument(
            "Query type not supported by NED provider");
    }

    return query.str();
}

auto NedProvider::query(const OnlineQueryParams& params)
    -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
    try {
        if (!pImpl_ || !pImpl_->httpClient_) {
            return atom::type::Error<OnlineQueryError>({
                OnlineQueryError::Code::ServiceUnavailable,
                "NED provider not properly initialized",
                std::string(PROVIDER_NAME)
            });
        }

        // Check cache first
        if (pImpl_->cache_ && pImpl_->config_.useCache) {
            auto cacheKey = QueryCache::generateKey(std::string(PROVIDER_NAME), params);

            if (auto cached = pImpl_->cache_->get(cacheKey)) {
                spdlog::debug("NED query cache hit for: {}", params.query);
                cached.value().fromCache = true;
                return cached.value();
            }
        }

        // Check rate limiting
        if (pImpl_->rateLimiter_) {
            if (!pImpl_->rateLimiter_->allowRequest()) {
                spdlog::warn("NED query rate limited");
                return atom::type::Error<OnlineQueryError>({
                    OnlineQueryError::Code::RateLimited,
                    "Rate limit exceeded",
                    std::string(PROVIDER_NAME),
                    std::chrono::seconds{1}
                });
            }
        }

        // Build ADQL query
        auto adql = buildAdqlQuery(params);
        spdlog::debug("NED ADQL query: {}", adql);

        // Prepare HTTP request
        HttpRequest request;
        request.url = std::string(BASE_URL) + "?request=doQuery&lang=adql&format=votable&query=" +
                      urlEncode(adql);
        request.method = "GET";
        request.timeout = pImpl_->config_.timeout;

        spdlog::info("Sending NED query to: {}...", request.url.substr(0, 100));

        // Execute request
        auto response = pImpl_->httpClient_->request(request);

        if (!response) {
            spdlog::error("NED HTTP request failed: {}", response.error());
            return atom::type::Error<OnlineQueryError>({
                OnlineQueryError::Code::NetworkError,
                response.error(),
                std::string(PROVIDER_NAME)
            });
        }

        const auto& httpResp = response.value();

        // Check HTTP status
        if (httpResp.statusCode != 200) {
            std::string errMsg = "HTTP " + std::to_string(httpResp.statusCode);

            OnlineQueryError::Code errCode = OnlineQueryError::Code::NetworkError;
            if (httpResp.statusCode >= 400 && httpResp.statusCode < 500) {
                errCode = OnlineQueryError::Code::InvalidQuery;
            } else if (httpResp.statusCode == 429) {
                errCode = OnlineQueryError::Code::RateLimited;
            } else if (httpResp.statusCode >= 500) {
                errCode = OnlineQueryError::Code::ServiceUnavailable;
            }

            spdlog::error("NED query failed with status {}: {}", httpResp.statusCode,
                         httpResp.body.substr(0, 200));

            return atom::type::Error<OnlineQueryError>({
                errCode,
                errMsg,
                std::string(PROVIDER_NAME),
                std::nullopt,
                httpResp.body
            });
        }

        // Parse VOTable response
        OnlineQueryResult result;
        result.provider = std::string(PROVIDER_NAME);
        result.queryTime = httpResp.responseTime;
        result.fromCache = false;

        if (!parseVotableResponse(httpResp.body, result)) {
            spdlog::error("Failed to parse NED VOTable response");
            return atom::type::Error<OnlineQueryError>({
                OnlineQueryError::Code::ParseError,
                "Failed to parse VOTable response",
                std::string(PROVIDER_NAME),
                std::nullopt,
                httpResp.body
            });
        }

        spdlog::info("NED query successful, found {} objects", result.objects.size());

        // Cache result
        if (pImpl_->cache_ && pImpl_->config_.useCache) {
            auto cacheKey = QueryCache::generateKey(std::string(PROVIDER_NAME), params);
            pImpl_->cache_->put(cacheKey, result, pImpl_->config_.cacheTTL);
        }

        return result;
    } catch (const std::invalid_argument& e) {
        spdlog::error("NED invalid query parameters: {}", e.what());
        return atom::type::Error<OnlineQueryError>({
            OnlineQueryError::Code::InvalidQuery,
            e.what(),
            std::string(PROVIDER_NAME)
        });
    } catch (const std::exception& e) {
        spdlog::error("NED query error: {}", e.what());
        return atom::type::Error<OnlineQueryError>({
            OnlineQueryError::Code::Unknown,
            e.what(),
            std::string(PROVIDER_NAME)
        });
    }
}

auto NedProvider::queryAsync(const OnlineQueryParams& params)
    -> std::future<atom::type::Expected<OnlineQueryResult, OnlineQueryError>> {
    return std::async(std::launch::async, [this, params]() {
        return this->query(params);
    });
}

void NedProvider::setConfig(const NedProviderConfig& config) {
    if (pImpl_) {
        pImpl_->config_ = config;
        spdlog::info("NED provider configuration updated");
    }
}

auto NedProvider::getConfig() const -> const NedProviderConfig& {
    static const NedProviderConfig defaultConfig;
    return pImpl_ ? pImpl_->config_ : defaultConfig;
}

}  // namespace lithium::target::online
