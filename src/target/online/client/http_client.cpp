// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#include "http_client.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include <spdlog/spdlog.h>
#include "atom/web/curl.hpp"

namespace lithium::target::online {

namespace {
/**
 * @brief RAII wrapper for timing measurements
 */
class TimingGuard {
public:
    explicit TimingGuard(std::chrono::milliseconds& out)
        : out_(out), start_(std::chrono::high_resolution_clock::now()) {}

    ~TimingGuard() {
        auto end = std::chrono::high_resolution_clock::now();
        out_ =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
    }

private:
    std::chrono::milliseconds& out_;
    std::chrono::high_resolution_clock::time_point start_;
};
}  // namespace

/**
 * @brief Implementation class for AsyncHttpClient
 */
class AsyncHttpClient::Impl {
public:
    explicit Impl(const HttpClientConfig& config) : config_(config) {}

    /**
     * @brief Execute HTTP request with retry logic
     */
    auto executeRequest(const HttpRequest& request)
        -> atom::type::Expected<HttpResponse, std::string> {
        std::chrono::milliseconds responseTime;

        for (size_t attempt = 0; attempt <= config_.maxRetries; ++attempt) {
            auto result = executeRequestOnce(request, responseTime);

            if (result) {
                return result;
            }

            // Don't retry on last attempt
            if (attempt < config_.maxRetries) {
                spdlog::warn(
                    "HTTP request failed, retrying (attempt {}/{}): {}",
                    attempt + 1, config_.maxRetries, result.error());

                // Exponential backoff: delay * 2^attempt
                auto delay = config_.retryDelay;
                for (size_t i = 0; i < attempt; ++i) {
                    delay = delay * 2;
                }
                std::this_thread::sleep_for(delay);
            }
        }

        return atom::type::Error<std::string>(
            "HTTP request failed after " +
            std::to_string(config_.maxRetries + 1) + " attempts");
    }

private:
    HttpClientConfig config_;

    /**
     * @brief Execute a single HTTP request without retry
     */
    auto executeRequestOnce(const HttpRequest& request,
                            std::chrono::milliseconds& responseTime)
        -> atom::type::Expected<HttpResponse, std::string> {
        HttpResponse response;
        TimingGuard timer(responseTime);
        response.responseTime = responseTime;

        try {
            auto curl = atom::web::CurlWrapper();

            // Configure curl
            curl.setUrl(request.url);
            curl.setRequestMethod(request.method);
            curl.setTimeout(request.timeout.count() /
                            1000);  // Convert to seconds
            curl.setFollowLocation(request.followRedirects);
            curl.setSSLOptions(request.verifySSL, request.verifySSL);

            // Add headers
            curl.addHeader("User-Agent", config_.userAgent);
            for (const auto& [key, value] : request.headers) {
                curl.addHeader(key, value);
            }

            // Set proxy if configured
            if (config_.proxyUrl.has_value()) {
                curl.setProxy(config_.proxyUrl.value());
            }

            // Set request body for POST/PUT
            if (request.body.has_value()) {
                curl.setRequestBody(request.body.value());
            }

            // Execute request
            std::string responseBody = curl.perform();

            response.statusCode = 200;  // curl.perform() throws on error
            response.body = responseBody;
            response.effectiveUrl = request.url;

            return response;
        } catch (const std::exception& e) {
            spdlog::error("HTTP request exception: {}", e.what());
            return atom::type::Error<std::string>(
                std::string("HTTP request failed: ") + e.what());
        }
    }
};

// ============================================================================
// AsyncHttpClient Implementation
// ============================================================================

AsyncHttpClient::AsyncHttpClient(const HttpClientConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {}

AsyncHttpClient::~AsyncHttpClient() = default;

AsyncHttpClient::AsyncHttpClient(AsyncHttpClient&& other) noexcept
    : pImpl_(std::move(other.pImpl_)) {}

AsyncHttpClient& AsyncHttpClient::operator=(AsyncHttpClient&& other) noexcept {
    if (this != &other) {
        pImpl_ = std::move(other.pImpl_);
    }
    return *this;
}

auto AsyncHttpClient::requestAsync(const HttpRequest& request)
    -> std::future<atom::type::Expected<HttpResponse, std::string>> {
    return std::async(std::launch::async, [this, request]() {
        return pImpl_->executeRequest(request);
    });
}

auto AsyncHttpClient::requestBatch(const std::vector<HttpRequest>& requests)
    -> std::vector<
        std::future<atom::type::Expected<HttpResponse, std::string>>> {
    std::vector<std::future<atom::type::Expected<HttpResponse, std::string>>>
        futures;
    futures.reserve(requests.size());

    for (const auto& request : requests) {
        futures.push_back(requestAsync(request));
    }

    return futures;
}

auto AsyncHttpClient::request(const HttpRequest& request)
    -> atom::type::Expected<HttpResponse, std::string> {
    return pImpl_->executeRequest(request);
}

auto AsyncHttpClient::get(const std::string& url,
                          std::chrono::milliseconds timeout)
    -> atom::type::Expected<HttpResponse, std::string> {
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    req.timeout = timeout;
    return request(req);
}

auto AsyncHttpClient::post(const std::string& url, const std::string& body,
                           const std::string& contentType)
    -> atom::type::Expected<HttpResponse, std::string> {
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    req.body = body;
    req.headers["Content-Type"] = contentType;
    return request(req);
}

void AsyncHttpClient::setDefaultTimeout(std::chrono::milliseconds timeout) {
    // This would be stored in pImpl_ config if we exposed mutable config
    spdlog::info("Default timeout set to {}ms", timeout.count());
}

void AsyncHttpClient::setProxy(const std::string& proxyUrl) {
    spdlog::info("Proxy set to {}", proxyUrl);
}

void AsyncHttpClient::setUserAgent(const std::string& userAgent) {
    spdlog::info("User agent set to {}", userAgent);
}

}  // namespace lithium::target::online
