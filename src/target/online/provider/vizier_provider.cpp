// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "vizier_provider.hpp"

#include <chrono>
#include <sstream>
#include <thread>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>

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
        } else if (c == '+') {
            encoded << "%2B";
        } else {
            encoded << '%' << std::hex << std::setfill('0') << std::setw(2)
                    << static_cast<int>(c);
        }
    }
    return encoded.str();
}

/**
 * @brief Trim whitespace from string
 *
 * @param str String to trim
 * @return Trimmed string
 */
auto trim(const std::string& str) -> std::string {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    return str.substr(start, end - start + 1);
}

/**
 * @brief Extract text between XML tags
 *
 * @param xml XML string
 * @param openTag Opening tag (without brackets)
 * @param closeTag Closing tag (without brackets)
 * @return Extracted text or empty string if not found
 */
auto extractXmlText(const std::string& xml, const std::string& openTag,
                    const std::string& closeTag) -> std::string {
    std::string open = "<" + openTag + ">";
    std::string close = "</" + closeTag + ">";

    size_t start = xml.find(open);
    if (start == std::string::npos) {
        return "";
    }

    start += open.length();
    size_t end = xml.find(close, start);
    if (end == std::string::npos) {
        return "";
    }

    return trim(xml.substr(start, end - start));
}

/**
 * @brief Parse VizieR VOTable response
 *
 * Extracts celestial object data from VOTable XML response.
 * Handles multiple RESOURCE elements and TABLE elements.
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
            spdlog::warn("No TABLEDATA found in VizieR response");
            return true;  // Return empty result, not an error
        }

        std::string tableContent =
            xml.substr(tableStart + 11, tableEnd - tableStart - 11);

        // Find FIELD elements to determine column names/order
        size_t fieldsStart = xml.find("<FIELDS>");
        size_t fieldsEnd = xml.find("</FIELDS>");
        std::vector<std::string> fieldNames;

        if (fieldsStart != std::string::npos && fieldsEnd != std::string::npos) {
            std::string fieldsContent =
                xml.substr(fieldsStart + 8, fieldsEnd - fieldsStart - 8);
            size_t fieldPos = 0;

            while ((fieldPos = fieldsContent.find("<FIELD", fieldPos)) !=
                   std::string::npos) {
                size_t nameStart = fieldsContent.find("name=\"", fieldPos);
                if (nameStart == std::string::npos) {
                    break;
                }
                nameStart += 6;

                size_t nameEnd = fieldsContent.find("\"", nameStart);
                if (nameEnd == std::string::npos) {
                    break;
                }

                std::string fieldName =
                    fieldsContent.substr(nameStart, nameEnd - nameStart);
                fieldNames.push_back(fieldName);

                fieldPos = nameEnd;
            }
        }

        // Parse table rows
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
                field = trim(field);

                fields.push_back(field);
                tdPos = tdEnd + 5;
            }

            if (!fields.empty()) {
                CelestialObjectModel obj;

                // Map VizieR fields to CelestialObjectModel
                // Expected field order from query varies by catalog, but typically:
                // 0: Catalog identifier or name
                // 1: RA (degrees)
                // 2: Dec (degrees)
                // 3: Magnitude V
                // 4: Additional fields (varies by catalog)

                // Determine field mapping from field names if available
                int raIdx = -1, decIdx = -1, magIdx = -1, idIdx = 0;

                for (size_t i = 0; i < fieldNames.size(); ++i) {
                    const auto& fname = fieldNames[i];
                    if (fname.find("RA") != std::string::npos ||
                        fname.find("_RA") != std::string::npos) {
                        raIdx = i;
                    } else if (fname.find("DE") != std::string::npos ||
                               fname.find("_DE") != std::string::npos) {
                        decIdx = i;
                    } else if (fname.find("Vmag") != std::string::npos ||
                               fname.find("Mag") != std::string::npos) {
                        magIdx = i;
                    }
                }

                // Fallback to default positions if field matching failed
                if (raIdx < 0) raIdx = 1;
                if (decIdx < 0) decIdx = 2;
                if (magIdx < 0) magIdx = 3;

                // Set identifier (first field or combine with catalog)
                if (fields.size() > idIdx) {
                    obj.identifier = fields[idIdx];
                    obj.mIdentifier = fields[idIdx];
                }

                // Set RA
                if (raIdx >= 0 && fields.size() > static_cast<size_t>(raIdx)) {
                    try {
                        double ra = std::stod(fields[raIdx]);
                        obj.radJ2000 = ra;
                        obj.raJ2000 = fields[raIdx];
                    } catch (const std::exception& e) {
                        spdlog::debug("Failed to parse RA: {}", fields[raIdx]);
                    }
                }

                // Set Dec
                if (decIdx >= 0 && fields.size() > static_cast<size_t>(decIdx)) {
                    try {
                        double dec = std::stod(fields[decIdx]);
                        obj.decDJ2000 = dec;
                        obj.decJ2000 = fields[decIdx];
                    } catch (const std::exception& e) {
                        spdlog::debug("Failed to parse Dec: {}", fields[decIdx]);
                    }
                }

                // Set visual magnitude
                if (magIdx >= 0 && fields.size() > static_cast<size_t>(magIdx)) {
                    if (!fields[magIdx].empty()) {
                        try {
                            obj.visualMagnitudeV = std::stod(fields[magIdx]);
                        } catch (const std::exception& e) {
                            spdlog::debug("Failed to parse magnitude: {}",
                                         fields[magIdx]);
                        }
                    }
                }

                // Store additional fields as aliases if available
                if (fields.size() > 4) {
                    std::ostringstream ss;
                    for (size_t i = 4; i < fields.size() && i < 8; ++i) {
                        if (i > 4) ss << ", ";
                        ss << fields[i];
                    }
                    if (!ss.str().empty()) {
                        obj.aliases = ss.str();
                    }
                }

                result.objects.push_back(obj);
            }

            pos = rowEnd + 5;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error parsing VizieR VOTable response: {}", e.what());
        return false;
    }
}

}  // namespace

/**
 * @brief Implementation class for VizierProvider
 */
class VizierProvider::Impl {
public:
    explicit Impl(std::shared_ptr<AsyncHttpClient> httpClient,
                  std::shared_ptr<QueryCache> cache,
                  std::shared_ptr<ApiRateLimiter> rateLimiter,
                  const VizierProviderConfig& config)
        : httpClient_(std::move(httpClient)),
          cache_(std::move(cache)),
          rateLimiter_(std::move(rateLimiter)),
          config_(config) {}

    VizierProviderConfig config_;
    std::shared_ptr<AsyncHttpClient> httpClient_;
    std::shared_ptr<QueryCache> cache_;
    std::shared_ptr<ApiRateLimiter> rateLimiter_;
};

// Constructor
VizierProvider::VizierProvider(
    std::shared_ptr<AsyncHttpClient> httpClient,
    std::shared_ptr<QueryCache> cache,
    std::shared_ptr<ApiRateLimiter> rateLimiter,
    const VizierProviderConfig& config)
    : pImpl_(std::make_unique<Impl>(std::move(httpClient), std::move(cache),
                                   std::move(rateLimiter), config)) {
    spdlog::info("Initializing VizieR provider");
}

// Destructor
VizierProvider::~VizierProvider() = default;

// Move constructor
VizierProvider::VizierProvider(VizierProvider&&) noexcept = default;

// Move assignment operator
VizierProvider& VizierProvider::operator=(VizierProvider&&) noexcept = default;

auto VizierProvider::isAvailable() const -> bool {
    if (!pImpl_ || !pImpl_->httpClient_) {
        spdlog::warn("VizieR provider not properly initialized");
        return false;
    }

    try {
        // Perform a simple health check with a cone search
        // Use a known bright object (M31) at low resolution
        OnlineQueryParams params;
        params.type = QueryType::ByCoordinates;
        params.ra = 10.6847;       // M31 RA
        params.dec = 41.2689;      // M31 Dec
        params.radius = 0.1;       // Small search radius
        params.limit = 1;

        HttpRequest request;
        request.url = buildQueryUrl(params);
        request.method = "GET";
        request.timeout = std::chrono::milliseconds{5000};

        auto response = pImpl_->httpClient_->request(request);

        if (!response) {
            spdlog::warn("VizieR health check failed: {}", response.error());
            return false;
        }

        return response.value().statusCode == 200;
    } catch (const std::exception& e) {
        spdlog::warn("VizieR health check error: {}", e.what());
        return false;
    }
}

auto VizierProvider::buildQueryUrl(const OnlineQueryParams& params) const
    -> std::string {
    std::ostringstream url;
    url << pImpl_->config_.baseUrl;
    url << "?-out.max=" << pImpl_->config_.maxRows;
    url << "&-out=";

    // Build output fields based on configuration
    bool first = true;
    if (pImpl_->config_.includeCoordinates) {
        url << "RAJ2000,DEJ2000";
        first = false;
    }
    if (pImpl_->config_.includeMagnitudes) {
        if (!first) url << ",";
        url << "Vmag";
        first = false;
    }

    switch (params.type) {
    case QueryType::ByCoordinates: {
        if (!params.ra.has_value() || !params.dec.has_value()) {
            throw std::invalid_argument(
                "RA and Dec required for ByCoordinates search");
        }

        double radius = params.radius.value_or(0.5);  // Default 0.5 degrees
        // Convert degrees to arcseconds for VizieR
        double radiusArcsec = radius * 3600.0;

        // Query each default catalog
        if (pImpl_->config_.defaultCatalogs.empty()) {
            throw std::invalid_argument(
                "At least one catalog must be configured");
        }

        url << "&-source=" << urlEncode(pImpl_->config_.defaultCatalogs[0]);
        url << "&-c=" << std::fixed << std::setprecision(6)
            << params.ra.value() << "+" << params.dec.value();
        url << "&-c.rs=" << std::fixed << std::setprecision(2)
            << radiusArcsec;

        // Optional magnitude filtering
        if (params.minMagnitude.has_value()) {
            url << "&Vmag=>" << std::fixed << std::setprecision(2)
                << params.minMagnitude.value();
        }
        if (params.maxMagnitude.has_value()) {
            url << "&Vmag=<" << std::fixed << std::setprecision(2)
                << params.maxMagnitude.value();
        }

        break;
    }

    case QueryType::Catalog: {
        if (!params.catalog.has_value() || params.catalog.value().empty()) {
            throw std::invalid_argument("Catalog identifier required for Catalog query");
        }

        url << "&-source=" << urlEncode(params.catalog.value());

        // If coordinates are provided, do a cone search
        if (params.ra.has_value() && params.dec.has_value()) {
            double radius = params.radius.value_or(0.5);
            double radiusArcsec = radius * 3600.0;

            url << "&-c=" << std::fixed << std::setprecision(6)
                << params.ra.value() << "+" << params.dec.value();
            url << "&-c.rs=" << std::fixed << std::setprecision(2)
                << radiusArcsec;
        }

        // Optional magnitude filtering
        if (params.minMagnitude.has_value()) {
            url << "&Vmag=>" << std::fixed << std::setprecision(2)
                << params.minMagnitude.value();
        }
        if (params.maxMagnitude.has_value()) {
            url << "&Vmag=<" << std::fixed << std::setprecision(2)
                << params.maxMagnitude.value();
        }

        break;
    }

    case QueryType::ByName:
    case QueryType::ByConstellation:
    case QueryType::Ephemeris:
    default:
        throw std::invalid_argument(
            "Query type not supported by VizieR provider. "
            "Supported: ByCoordinates, Catalog");
    }

    return url.str();
}

auto VizierProvider::query(const OnlineQueryParams& params)
    -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
    try {
        if (!pImpl_ || !pImpl_->httpClient_) {
            return atom::type::Error<OnlineQueryError>({
                OnlineQueryError::Code::ServiceUnavailable,
                "VizieR provider not properly initialized",
                std::string(PROVIDER_NAME)
            });
        }

        // Check cache first
        if (pImpl_->cache_ && pImpl_->config_.useCache) {
            auto cacheKey = QueryCache::generateKey(std::string(PROVIDER_NAME), params);

            if (auto cached = pImpl_->cache_->get(cacheKey)) {
                spdlog::debug("VizieR query cache hit");
                cached.value().fromCache = true;
                return cached.value();
            }
        }

        // Check rate limiting
        if (pImpl_->rateLimiter_) {
            if (!pImpl_->rateLimiter_->allowRequest()) {
                spdlog::warn("VizieR query rate limited");
                return atom::type::Error<OnlineQueryError>({
                    OnlineQueryError::Code::RateLimited,
                    "Rate limit exceeded",
                    std::string(PROVIDER_NAME),
                    std::chrono::seconds{1}
                });
            }
        }

        // Build query URL
        auto queryUrl = buildQueryUrl(params);
        spdlog::debug("VizieR query URL: {}", queryUrl.substr(0, 150) << "...");

        // Prepare HTTP request
        HttpRequest request;
        request.url = queryUrl;
        request.method = "GET";
        request.timeout = pImpl_->config_.timeout;

        spdlog::info("Sending VizieR query to catalog");

        // Execute request
        auto response = pImpl_->httpClient_->request(request);

        if (!response) {
            spdlog::error("VizieR HTTP request failed: {}", response.error());
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

            spdlog::error("VizieR query failed with status {}", httpResp.statusCode);

            return atom::type::Error<OnlineQueryError>({
                errCode,
                errMsg,
                std::string(PROVIDER_NAME),
                std::nullopt,
                httpResp.body.substr(0, 500)
            });
        }

        // Parse VOTable response
        OnlineQueryResult result;
        result.provider = std::string(PROVIDER_NAME);
        result.queryTime = httpResp.responseTime;
        result.fromCache = false;

        if (!parseVotableResponse(httpResp.body, result)) {
            spdlog::error("Failed to parse VizieR VOTable response");
            return atom::type::Error<OnlineQueryError>({
                OnlineQueryError::Code::ParseError,
                "Failed to parse VOTable response",
                std::string(PROVIDER_NAME),
                std::nullopt,
                httpResp.body.substr(0, 500)
            });
        }

        spdlog::info("VizieR query successful, found {} objects",
                    result.objects.size());

        // Cache result
        if (pImpl_->cache_ && pImpl_->config_.useCache) {
            auto cacheKey = QueryCache::generateKey(std::string(PROVIDER_NAME), params);
            pImpl_->cache_->put(cacheKey, result, pImpl_->config_.cacheTTL);
        }

        return result;
    } catch (const std::invalid_argument& e) {
        spdlog::error("VizieR invalid query parameters: {}", e.what());
        return atom::type::Error<OnlineQueryError>({
            OnlineQueryError::Code::InvalidQuery,
            e.what(),
            std::string(PROVIDER_NAME)
        });
    } catch (const std::exception& e) {
        spdlog::error("VizieR query error: {}", e.what());
        return atom::type::Error<OnlineQueryError>({
            OnlineQueryError::Code::Unknown,
            e.what(),
            std::string(PROVIDER_NAME)
        });
    }
}

auto VizierProvider::queryAsync(const OnlineQueryParams& params)
    -> std::future<atom::type::Expected<OnlineQueryResult, OnlineQueryError>> {
    return std::async(std::launch::async, [this, params]() { return query(params); });
}

auto VizierProvider::queryCatalog(const std::string& catalog,
                                   const OnlineQueryParams& params)
    -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
    OnlineQueryParams modifiedParams = params;
    modifiedParams.type = QueryType::Catalog;
    modifiedParams.catalog = catalog;
    return query(modifiedParams);
}

void VizierProvider::setConfig(const VizierProviderConfig& config) {
    if (pImpl_) {
        pImpl_->config_ = config;
        spdlog::debug("VizieR provider configuration updated");
    }
}

auto VizierProvider::getConfig() const -> const VizierProviderConfig& {
    return pImpl_->config_;
}

}  // namespace lithium::target::online
