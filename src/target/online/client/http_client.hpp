// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_ONLINE_CLIENT_HTTP_CLIENT_HPP
#define LITHIUM_TARGET_ONLINE_CLIENT_HTTP_CLIENT_HPP

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/expected.hpp"

namespace lithium::target::online {

/**
 * @brief HTTP request configuration
 */
struct HttpRequest {
    std::string url;
    std::string method = "GET";
    std::unordered_map<std::string, std::string> headers;
    std::optional<std::string> body;
    std::chrono::milliseconds timeout{30000};
    bool followRedirects = true;
    bool verifySSL = true;
};

/**
 * @brief HTTP response data
 */
struct HttpResponse {
    int statusCode = 0;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::chrono::milliseconds responseTime{0};
    std::string effectiveUrl;  // Final URL after redirects
};

/**
 * @brief Async HTTP client configuration
 */
struct HttpClientConfig {
    size_t connectionPoolSize = 4;
    std::chrono::milliseconds defaultTimeout{30000};
    std::string userAgent = "Lithium-Next/1.0";
    std::optional<std::string> proxyUrl;
    size_t maxRetries = 3;
    std::chrono::milliseconds retryDelay{1000};
};

/**
 * @brief Asynchronous HTTP client wrapper
 *
 * Provides async HTTP operations with connection pooling,
 * retry logic, and timeout handling.
 *
 * Thread-safe for concurrent requests.
 */
class AsyncHttpClient {
public:
    explicit AsyncHttpClient(const HttpClientConfig& config = {});
    ~AsyncHttpClient();

    // Non-copyable, movable
    AsyncHttpClient(const AsyncHttpClient&) = delete;
    AsyncHttpClient& operator=(const AsyncHttpClient&) = delete;
    AsyncHttpClient(AsyncHttpClient&&) noexcept;
    AsyncHttpClient& operator=(AsyncHttpClient&&) noexcept;

    /**
     * @brief Execute async HTTP request
     */
    [[nodiscard]] auto requestAsync(const HttpRequest& request)
        -> std::future<atom::type::Expected<HttpResponse, std::string>>;

    /**
     * @brief Execute multiple requests in parallel
     */
    [[nodiscard]] auto requestBatch(const std::vector<HttpRequest>& requests)
        -> std::vector<std::future<atom::type::Expected<HttpResponse, std::string>>>;

    /**
     * @brief Execute sync HTTP request (blocking)
     */
    [[nodiscard]] auto request(const HttpRequest& request)
        -> atom::type::Expected<HttpResponse, std::string>;

    /**
     * @brief Convenience GET request
     */
    [[nodiscard]] auto get(const std::string& url,
                           std::chrono::milliseconds timeout = std::chrono::milliseconds{30000})
        -> atom::type::Expected<HttpResponse, std::string>;

    /**
     * @brief Convenience POST request
     */
    [[nodiscard]] auto post(const std::string& url,
                            const std::string& body,
                            const std::string& contentType = "application/json")
        -> atom::type::Expected<HttpResponse, std::string>;

    /**
     * @brief Set default timeout
     */
    void setDefaultTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Set proxy URL
     */
    void setProxy(const std::string& proxyUrl);

    /**
     * @brief Set user agent
     */
    void setUserAgent(const std::string& userAgent);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_CLIENT_HTTP_CLIENT_HPP
