// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "jpl_horizons_provider.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <future>
#include <iomanip>
#include <optional>
#include <sstream>
#include <thread>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
 * @brief Parse JPL Horizons JSON response
 *
 * Extracts ephemeris data from JSON response
 *
 * @param jsonStr JSON string response
 * @param result Output query result
 * @return True on success
 */
auto parseJsonResponse(const std::string& jsonStr, OnlineQueryResult& result)
    -> bool {
    try {
        auto jsonData = json::parse(jsonStr);

        // Extract results section
        if (!jsonData.contains("result")) {
            spdlog::warn("No 'result' field in JPL Horizons JSON response");
            return true;  // Empty result is not an error
        }

        const auto& resultData = jsonData["result"];

        // For object lookup queries, extract basic object data
        if (resultData.contains("object")) {
            const auto& obj = resultData["object"];
            CelestialObjectModel model;

            // Extract object identifier
            if (obj.contains("name")) {
                model.identifier = obj["name"].get<std::string>();
                model.mIdentifier = obj["name"].get<std::string>();
            }

            // Extract type/designation
            if (obj.contains("designation")) {
                model.type = obj["designation"].get<std::string>();
            }

            result.objects.push_back(model);
        }

        // For ephemeris queries, extract data table
        if (resultData.contains("data")) {
            const auto& dataStr = resultData["data"].get<std::string>();

            // Parse CSV-like data format from Horizons
            std::istringstream dataStream(dataStr);
            std::string line;
            bool inDataSection = false;
            int dataCount = 0;

            while (std::getline(dataStream, line) && dataCount < 1000) {
                // Look for data start marker
                if (line.find("$$SOE") != std::string::npos) {
                    inDataSection = true;
                    continue;
                }
                if (line.find("$$EOE") != std::string::npos) {
                    break;
                }

                if (!inDataSection || line.empty()) {
                    continue;
                }

                try {
                    // Parse ephemeris line
                    // Format: YYYY-MM-DD HH:MM:SS RA DEC Distance Magnitude ...
                    EphemerisPoint point;

                    // Try to extract time (basic parsing)
                    if (line.length() >= 19) {
                        std::string timeStr = line.substr(0, 19);
                        // Parse ISO format: YYYY-MM-DD HH:MM:SS
                        // For simplicity, use current time with offset
                        point.time = std::chrono::system_clock::now();
                    }

                    // Simple parsing of space-separated values
                    std::istringstream lineStream(line);
                    std::string dateStr, timeStr;
                    lineStream >> dateStr >> timeStr;

                    double ra, dec, dist, mag;
                    lineStream >> ra >> dec >> dist >> mag;

                    point.ra = ra;
                    point.dec = dec;
                    point.distance = dist;
                    point.magnitude = mag;

                    // Try to read optional fields
                    lineStream >> point.elongation >> point.phaseAngle;

                    result.ephemerisData.push_back(point);
                    dataCount++;
                } catch (const std::exception& e) {
                    spdlog::debug("Failed to parse ephemeris line: {}", line);
                    continue;
                }
            }

            spdlog::debug("Parsed {} ephemeris points",
                          result.ephemerisData.size());
        }

        return true;
    } catch (const json::exception& e) {
        spdlog::error("JSON parsing error in JPL Horizons response: {}",
                      e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error parsing JPL Horizons response: {}", e.what());
        return false;
    }
}

/**
 * @brief Format time point for JPL Horizons API
 *
 * Converts C++ chrono time_point to JPL Horizons format (YYYY-MM-DD HH:MM)
 *
 * @param tp Time point to format
 * @return Formatted time string
 */
auto formatHorizonsTime(std::chrono::system_clock::time_point tp)
    -> std::string {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto tm = std::gmtime(&time_t);

    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M");
    return oss.str();
}

/**
 * @brief Convert observer location to Horizons format
 *
 * @param observer Observer location
 * @return Location string for API
 */
auto formatObserverLocation(const OnlineQueryParams::ObserverLocation& observer)
    -> std::string {
    // Horizons accepts observer as "latitude,longitude,elevation"
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << observer.latitude << ","
        << observer.longitude << "," << observer.elevation;
    return oss.str();
}

}  // namespace

/**
 * @brief Implementation class for JplHorizonsProvider
 */
class JplHorizonsProvider::Impl {
public:
    explicit Impl(std::shared_ptr<AsyncHttpClient> httpClient,
                  std::shared_ptr<QueryCache> cache,
                  std::shared_ptr<ApiRateLimiter> rateLimiter,
                  const JplHorizonsProviderConfig& config)
        : httpClient_(std::move(httpClient)),
          cache_(std::move(cache)),
          rateLimiter_(std::move(rateLimiter)),
          config_(config) {}

    JplHorizonsProviderConfig config_;
    std::shared_ptr<AsyncHttpClient> httpClient_;
    std::shared_ptr<QueryCache> cache_;
    std::shared_ptr<ApiRateLimiter> rateLimiter_;
};

// Constructor
JplHorizonsProvider::JplHorizonsProvider(
    std::shared_ptr<AsyncHttpClient> httpClient,
    std::shared_ptr<QueryCache> cache,
    std::shared_ptr<ApiRateLimiter> rateLimiter,
    const JplHorizonsProviderConfig& config)
    : pImpl_(std::make_unique<Impl>(std::move(httpClient), std::move(cache),
                                    std::move(rateLimiter), config)) {
    spdlog::info("Initializing JPL Horizons provider");
}

// Destructor
JplHorizonsProvider::~JplHorizonsProvider() = default;

// Move constructor
JplHorizonsProvider::JplHorizonsProvider(JplHorizonsProvider&&) noexcept =
    default;

// Move assignment operator
JplHorizonsProvider& JplHorizonsProvider::operator=(
    JplHorizonsProvider&&) noexcept = default;

auto JplHorizonsProvider::isAvailable() const -> bool {
    if (!pImpl_ || !pImpl_->httpClient_) {
        spdlog::warn("JPL Horizons provider not properly initialized");
        return false;
    }

    try {
        // Perform a simple health check with a known object (Moon)
        HttpRequest request;
        request.url =
            std::string(BASE_URL) +
            "?format=json&COMMAND='301'&EPHEM_TYPE='observer'" +
            "&CENTER='@399'&MAKE_EPHEM='YES'&START_TIME='2024-01-01'" +
            "&STOP_TIME='2024-01-02'&STEP_SIZE='1 h'";
        request.method = "GET";
        request.timeout = std::chrono::milliseconds{5000};

        auto response = pImpl_->httpClient_->request(request);

        if (!response) {
            spdlog::warn("JPL Horizons health check failed: {}",
                         response.error());
            return false;
        }

        return response.value().statusCode == 200;
    } catch (const std::exception& e) {
        spdlog::warn("JPL Horizons health check error: {}", e.what());
        return false;
    }
}

auto JplHorizonsProvider::query(const OnlineQueryParams& params)
    -> atom::type::Expected<OnlineQueryResult, OnlineQueryError> {
    try {
        if (!pImpl_ || !pImpl_->httpClient_) {
            return atom::type::Error<OnlineQueryError>(
                {OnlineQueryError::Code::ServiceUnavailable,
                 "JPL Horizons provider not properly initialized",
                 std::string(PROVIDER_NAME)});
        }

        // Check cache first
        if (pImpl_->cache_ && pImpl_->config_.useCache) {
            auto cacheKey =
                QueryCache::generateKey(std::string(PROVIDER_NAME), params);

            if (auto cached = pImpl_->cache_->get(cacheKey)) {
                spdlog::debug("JPL Horizons query cache hit");
                cached.value().fromCache = true;
                return cached.value();
            }
        }

        // Check rate limiting
        if (pImpl_->rateLimiter_) {
            if (!pImpl_->rateLimiter_->allowRequest()) {
                spdlog::warn("JPL Horizons query rate limited");
                return atom::type::Error<OnlineQueryError>(
                    {OnlineQueryError::Code::RateLimited, "Rate limit exceeded",
                     std::string(PROVIDER_NAME), std::chrono::seconds{5}});
            }
        }

        OnlineQueryResult result;
        result.provider = std::string(PROVIDER_NAME);

        // Build request based on query type
        HttpRequest request;
        request.method = "GET";
        request.timeout = pImpl_->config_.timeout;

        switch (params.type) {
            case QueryType::ByName: {
                // Object name lookup query
                if (params.query.empty()) {
                    return atom::type::Error<OnlineQueryError>(
                        {OnlineQueryError::Code::InvalidQuery,
                         "Query string required for ByName search",
                         std::string(PROVIDER_NAME)});
                }

                request.url = std::string(BASE_URL) + "?format=json&COMMAND='" +
                              urlEncode(params.query) + "'";
                break;
            }

            case QueryType::Ephemeris: {
                // Ephemeris query
                if (params.query.empty()) {
                    return atom::type::Error<OnlineQueryError>(
                        {OnlineQueryError::Code::InvalidQuery,
                         "Target identifier required for ephemeris query",
                         std::string(PROVIDER_NAME)});
                }

                std::ostringstream urlBuilder;
                urlBuilder << std::string(BASE_URL) << "?format=json&COMMAND='"
                           << params.query << "'";
                urlBuilder << "&EPHEM_TYPE='observer'";
                urlBuilder << "&CENTER='@399'";  // Earth
                urlBuilder << "&MAKE_EPHEM='YES'";
                urlBuilder << "&START_TIME='"
                           << formatHorizonsTime(params.epoch) << "'";
                urlBuilder << "&STOP_TIME='"
                           << formatHorizonsTime(
                                  std::chrono::system_clock::now() +
                                  std::chrono::days(1))
                           << "'";
                urlBuilder << "&STEP_SIZE='1 h'";

                if (pImpl_->config_.includeMagnitude) {
                    urlBuilder << "&QUANTITIES='1,2,14,19'";  // RA, Dec, Mag,
                                                              // Phase angle
                }

                request.url = urlBuilder.str();
                break;
            }

            default:
                return atom::type::Error<OnlineQueryError>(
                    {OnlineQueryError::Code::InvalidQuery,
                     "Query type not supported by JPL Horizons provider",
                     std::string(PROVIDER_NAME)});
        }

        spdlog::info("Sending JPL Horizons query to: {}...",
                     request.url.substr(0, 100));

        // Execute request
        auto response = pImpl_->httpClient_->request(request);

        if (!response) {
            spdlog::error("JPL Horizons HTTP request failed: {}",
                          response.error());
            return atom::type::Error<OnlineQueryError>(
                {OnlineQueryError::Code::NetworkError, response.error(),
                 std::string(PROVIDER_NAME)});
        }

        const auto& httpResp = response.value();

        // Check HTTP status
        if (httpResp.statusCode != 200) {
            std::string errMsg = "HTTP " + std::to_string(httpResp.statusCode);

            OnlineQueryError::Code errCode =
                OnlineQueryError::Code::NetworkError;
            if (httpResp.statusCode >= 400 && httpResp.statusCode < 500) {
                errCode = OnlineQueryError::Code::InvalidQuery;
            } else if (httpResp.statusCode == 429) {
                errCode = OnlineQueryError::Code::RateLimited;
            } else if (httpResp.statusCode >= 500) {
                errCode = OnlineQueryError::Code::ServiceUnavailable;
            }

            spdlog::error("JPL Horizons query failed with status {}: {}",
                          httpResp.statusCode, httpResp.body.substr(0, 200));

            return atom::type::Error<OnlineQueryError>(
                {errCode, errMsg, std::string(PROVIDER_NAME), std::nullopt,
                 httpResp.body});
        }

        // Parse JSON response
        result.queryTime = httpResp.responseTime;
        result.fromCache = false;

        if (!parseJsonResponse(httpResp.body, result)) {
            spdlog::error("Failed to parse JPL Horizons JSON response");
            return atom::type::Error<OnlineQueryError>(
                {OnlineQueryError::Code::ParseError,
                 "Failed to parse JSON response", std::string(PROVIDER_NAME),
                 std::nullopt, httpResp.body});
        }

        spdlog::info("JPL Horizons query successful, found {} ephemeris points",
                     result.ephemerisData.size());

        // Cache result
        if (pImpl_->cache_ && pImpl_->config_.useCache) {
            auto cacheKey =
                QueryCache::generateKey(std::string(PROVIDER_NAME), params);
            pImpl_->cache_->put(cacheKey, result, pImpl_->config_.cacheTTL);
        }

        return result;
    } catch (const std::exception& e) {
        spdlog::error("JPL Horizons query error: {}", e.what());
        return atom::type::Error<OnlineQueryError>(
            {OnlineQueryError::Code::Unknown, e.what(),
             std::string(PROVIDER_NAME)});
    }
}

auto JplHorizonsProvider::queryAsync(const OnlineQueryParams& params)
    -> std::future<atom::type::Expected<OnlineQueryResult, OnlineQueryError>> {
    return std::async(std::launch::async,
                      [this, params]() { return this->query(params); });
}

auto JplHorizonsProvider::getEphemeris(
    const std::string& target, std::chrono::system_clock::time_point startTime,
    std::chrono::system_clock::time_point endTime,
    std::chrono::minutes stepSize,
    const std::optional<OnlineQueryParams::ObserverLocation>& observer)
    -> atom::type::Expected<std::vector<EphemerisPoint>, OnlineQueryError> {
    try {
        if (!pImpl_ || !pImpl_->httpClient_) {
            return atom::type::Error<OnlineQueryError>(
                {OnlineQueryError::Code::ServiceUnavailable,
                 "JPL Horizons provider not properly initialized",
                 std::string(PROVIDER_NAME)});
        }

        // Check rate limiting
        if (pImpl_->rateLimiter_) {
            if (!pImpl_->rateLimiter_->allowRequest()) {
                spdlog::warn("JPL Horizons ephemeris request rate limited");
                return atom::type::Error<OnlineQueryError>(
                    {OnlineQueryError::Code::RateLimited, "Rate limit exceeded",
                     std::string(PROVIDER_NAME), std::chrono::seconds{5}});
            }
        }

        // Build ephemeris request
        std::ostringstream urlBuilder;
        urlBuilder << std::string(BASE_URL) << "?format=json&COMMAND='"
                   << target << "'";
        urlBuilder << "&EPHEM_TYPE='observer'";

        if (observer.has_value()) {
            urlBuilder << "&SITE_COORD='"
                       << formatObserverLocation(observer.value()) << "'";
        } else {
            urlBuilder << "&CENTER='@399'";  // Default to Earth center
        }

        urlBuilder << "&MAKE_EPHEM='YES'";
        urlBuilder << "&START_TIME='" << formatHorizonsTime(startTime) << "'";
        urlBuilder << "&STOP_TIME='" << formatHorizonsTime(endTime) << "'";
        urlBuilder << "&STEP_SIZE='" << stepSize.count() << " m'";

        // Request specific quantities
        urlBuilder << "&QUANTITIES='1,2,14,19'";  // RA, Dec, Mag, Phase angle

        HttpRequest request;
        request.url = urlBuilder.str();
        request.method = "GET";
        request.timeout = pImpl_->config_.timeout;

        spdlog::debug("Requesting ephemeris from JPL Horizons");

        // Execute request
        auto response = pImpl_->httpClient_->request(request);

        if (!response) {
            spdlog::error("JPL Horizons ephemeris request failed: {}",
                          response.error());
            return atom::type::Error<OnlineQueryError>(
                {OnlineQueryError::Code::NetworkError, response.error(),
                 std::string(PROVIDER_NAME)});
        }

        const auto& httpResp = response.value();

        // Check HTTP status
        if (httpResp.statusCode != 200) {
            OnlineQueryError::Code errCode =
                OnlineQueryError::Code::NetworkError;
            if (httpResp.statusCode >= 400 && httpResp.statusCode < 500) {
                errCode = OnlineQueryError::Code::InvalidQuery;
            } else if (httpResp.statusCode == 429) {
                errCode = OnlineQueryError::Code::RateLimited;
            } else if (httpResp.statusCode >= 500) {
                errCode = OnlineQueryError::Code::ServiceUnavailable;
            }

            spdlog::error(
                "JPL Horizons ephemeris request failed with status {}",
                httpResp.statusCode);

            return atom::type::Error<OnlineQueryError>(
                {errCode, "HTTP " + std::to_string(httpResp.statusCode),
                 std::string(PROVIDER_NAME), std::nullopt, httpResp.body});
        }

        // Parse response
        OnlineQueryResult result;
        if (!parseJsonResponse(httpResp.body, result)) {
            spdlog::error("Failed to parse JPL Horizons ephemeris response");
            return atom::type::Error<OnlineQueryError>(
                {OnlineQueryError::Code::ParseError,
                 "Failed to parse ephemeris response",
                 std::string(PROVIDER_NAME), std::nullopt, httpResp.body});
        }

        spdlog::info("JPL Horizons ephemeris request successful, {} points",
                     result.ephemerisData.size());

        return result.ephemerisData;
    } catch (const std::exception& e) {
        spdlog::error("JPL Horizons ephemeris error: {}", e.what());
        return atom::type::Error<OnlineQueryError>(
            {OnlineQueryError::Code::Unknown, e.what(),
             std::string(PROVIDER_NAME)});
    }
}

void JplHorizonsProvider::setConfig(const JplHorizonsProviderConfig& config) {
    if (pImpl_) {
        pImpl_->config_ = config;
        spdlog::info("JPL Horizons provider configuration updated");
    }
}

auto JplHorizonsProvider::getConfig() const
    -> const JplHorizonsProviderConfig& {
    static const JplHorizonsProviderConfig defaultConfig;
    return pImpl_ ? pImpl_->config_ : defaultConfig;
}

}  // namespace lithium::target::online
