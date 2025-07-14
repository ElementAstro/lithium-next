/*
 * optimized_alpaca_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-31

Description: Modern C++20 Optimized ASCOM Alpaca REST Client - API Version 3
Only Features:
- C++20 coroutines and concepts
- Connection pooling and reuse
- Latest Alpaca API v3 support only
- Performance optimizations
- Modern error handling with std::expected
- Reduced code duplication
- Better memory management

**************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
// #include <unordered_map> // Not used directly
#include <span>
#include <vector>

#include <spdlog/spdlog.h>

// #include "atom/async/pool.hpp" // Not used directly
#include "atom/type/json.hpp"

// --- Added missing includes for Boost types used below ---
#include <boost/asio/io_context.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/json.hpp>

namespace lithium::device::ascom {

// Use the existing JSON library
using json = nlohmann::json;

// Modern error handling
enum class AlpacaError {
    Success = 0,
    InvalidValue = 0x401,
    ValueNotSet = 0x402,
    NotConnected = 0x407,
    InvalidWhileParked = 0x408,
    InvalidWhileSlaved = 0x409,
    InvalidOperation = 0x40B,
    ActionNotImplemented = 0x40C,
    UnspecifiedError = 0x500,
    NetworkError,
    ParseError,
    TimeoutError
};

// Device types - only commonly used ones
enum class DeviceType : std::uint8_t {
    Camera,
    Telescope,
    Focuser,
    FilterWheel,
    Dome,
    Rotator
};

// HTTP methods enum
enum class HttpMethod { GET, POST, PUT, DELETE, HEAD, OPTIONS };

// JSON convertible concept
template <typename T>
concept JsonConvertible = requires(T t) {
    { json(t) } -> std::convertible_to<json>;
};

// Forward declarations
class ConnectionPool;
struct AlpacaResponse;

// Modern Alpaca device info (simplified)
struct DeviceInfo {
    std::string name;
    DeviceType type;
    int number;
    std::string unique_id;
    std::string host;
    std::uint16_t port;
    bool ssl_enabled = false;

    constexpr bool operator==(const DeviceInfo&) const = default;
};

// Optimized response structure
struct AlpacaResponse {
    json data;
    int client_transaction_id;
    int server_transaction_id;
    std::chrono::steady_clock::time_point timestamp;

    template <typename T>
    std::expected<T, AlpacaError> extract() const;

    bool has_error() const noexcept;
    AlpacaError get_error() const noexcept;
};

// Awaitable result type
template <typename T>
using AlpacaResult = std::expected<T, AlpacaError>;

// --- Define AlpacaAwaitable as a coroutine type for API consistency ---
template <typename T>
struct AlpacaAwaitable {
    struct promise_type {
        std::optional<AlpacaResult<T>> value_;
        AlpacaAwaitable get_return_object() {
            return AlpacaAwaitable{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(AlpacaResult<T> value) { value_ = std::move(value); }
        void unhandled_exception() {
            value_ = std::unexpected(AlpacaError::UnspecifiedError);
        }
    };
    std::coroutine_handle<promise_type> handle_;
    AlpacaAwaitable(std::coroutine_handle<promise_type> h) : handle_(h) {}
    ~AlpacaAwaitable() {
        if (handle_)
            handle_.destroy();
    }
    AlpacaAwaitable(const AlpacaAwaitable&) = delete;
    AlpacaAwaitable& operator=(const AlpacaAwaitable&) = delete;
    AlpacaAwaitable(AlpacaAwaitable&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    AlpacaAwaitable& operator=(AlpacaAwaitable&& other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    bool await_ready() const noexcept { return false; }
    template<class Handle>
    void await_suspend(Handle h) const noexcept {}
    AlpacaResult<T> await_resume() {
        return std::move(handle_.promise().value_.value());
    }
};

// Modern HTTP client with connection pooling
class OptimizedAlpacaClient {
public:
    struct Config {
        std::chrono::seconds timeout{30};
        std::chrono::seconds keep_alive{60};
        std::size_t max_connections{10};
        std::size_t max_retries{3};
        bool enable_compression{true};
        bool enable_ssl_verification{true};
        std::string user_agent{"LithiumNext/1.0 AlpacaClient"};

        Config()
            : timeout(30),
              keep_alive(60),
              max_connections(10),
              max_retries(3),
              enable_compression(true),
              enable_ssl_verification(true) {}
    };

    explicit OptimizedAlpacaClient(boost::asio::io_context& ioc,
                                   Config config = {});
    ~OptimizedAlpacaClient();

    // Device discovery with coroutines
    boost::asio::awaitable<std::expected<std::vector<DeviceInfo>, AlpacaError>> discover_devices(
        std::string_view network_range = "192.168.1.0/24");

    // Connection management
    boost::asio::awaitable<std::expected<void, AlpacaError>> connect(const DeviceInfo& device);
    void disconnect();
    bool is_connected() const noexcept;

    // Type-safe property operations
    template <JsonConvertible T>
    boost::asio::awaitable<std::expected<T, AlpacaError>> get_property(std::string_view property);

    template <JsonConvertible T>
    boost::asio::awaitable<std::expected<void, AlpacaError>> set_property(std::string_view property,
                                       const T& value);

    // Method invocation
    template <JsonConvertible... Args>
    boost::asio::awaitable<std::expected<nlohmann::json, AlpacaError>> invoke_method(std::string_view method,
                                                  Args&&... args);

    // ImageBytes operations (optimized for performance)
    boost::asio::awaitable<std::expected<std::span<const std::uint8_t>, AlpacaError>> get_image_bytes();

    template <std::integral T>
    boost::asio::awaitable<std::expected<std::vector<T>, AlpacaError>> get_image_array();

    // Statistics
    struct Stats {
        std::atomic<std::uint64_t> requests_sent{0};
        std::atomic<std::uint64_t> requests_successful{0};
        std::atomic<std::uint64_t> bytes_sent{0};
        std::atomic<std::uint64_t> bytes_received{0};
        std::atomic<std::uint64_t> average_response_time_ms{0};
        std::atomic<std::uint64_t> connections_created{0};
        std::atomic<std::uint64_t> connections_reused{0};
    };

    const Stats& get_stats() const noexcept { return stats_; }
    void reset_stats() noexcept;

private:
    boost::asio::io_context& io_context_;
    Config config_;
    std::unique_ptr<ConnectionPool> connection_pool_;
    DeviceInfo current_device_;
    std::atomic<int> transaction_id_{1};
    Stats stats_;
    std::shared_ptr<spdlog::logger> logger_;

protected:
    // Internal operations - made protected so derived classes can access them
    boost::asio::awaitable<std::expected<AlpacaResponse, AlpacaError>> perform_request(
        boost::beast::http::verb method, std::string_view endpoint,
        const nlohmann::json& params = {});

    // Helper method to convert awaitable to AlpacaAwaitable
    template<typename T>
    AlpacaAwaitable<T> wrap_awaitable(boost::asio::awaitable<std::expected<T, AlpacaError>> awaitable);

    std::string build_url(std::string_view endpoint) const;
    nlohmann::json build_transaction_params() const;
    std::string build_form_data(const nlohmann::json& params) const;

    // Helper method for device discovery
    std::optional<DeviceInfo> discover_device_at_host(std::string_view host,
                                                      std::uint16_t port);

    int generate_transaction_id() const noexcept;
    void update_stats(bool success,
                      std::chrono::milliseconds response_time) noexcept;
};

// Device-specific clients (using CRTP for performance)
template <DeviceType Type>
class DeviceClient : public OptimizedAlpacaClient {
public:
    explicit DeviceClient(boost::asio::io_context& ioc, Config config = {})
        : OptimizedAlpacaClient(ioc, config) {}

    static constexpr DeviceType device_type() noexcept { return Type; }
};

// Specialized clients
using CameraClient = DeviceClient<DeviceType::Camera>;
using TelescopeClient = DeviceClient<DeviceType::Telescope>;
using FocuserClient = DeviceClient<DeviceType::Focuser>;

// Camera-specific operations
template <>
class DeviceClient<DeviceType::Camera> : public OptimizedAlpacaClient {
public:
    explicit DeviceClient(boost::asio::io_context& ioc, Config config = {})
        : OptimizedAlpacaClient(ioc, config) {}

    // Camera-specific methods
    boost::asio::awaitable<std::expected<double, AlpacaError>> get_ccd_temperature();
    boost::asio::awaitable<std::expected<void, AlpacaError>> set_ccd_temperature(double temperature);
    boost::asio::awaitable<std::expected<bool, AlpacaError>> get_cooler_on();
    boost::asio::awaitable<std::expected<void, AlpacaError>> set_cooler_on(bool on);
    boost::asio::awaitable<std::expected<void, AlpacaError>> start_exposure(double duration, bool light = true);
    boost::asio::awaitable<std::expected<void, AlpacaError>> abort_exposure();
    boost::asio::awaitable<std::expected<bool, AlpacaError>> get_image_ready();

    // High-performance image retrieval
    boost::asio::awaitable<std::expected<std::vector<std::uint16_t>, AlpacaError>> get_image_array_uint16();
    boost::asio::awaitable<std::expected<std::vector<std::uint32_t>, AlpacaError>> get_image_array_uint32();
};

// Telescope-specific operations
template <>
class DeviceClient<DeviceType::Telescope> : public OptimizedAlpacaClient {
public:
    explicit DeviceClient(boost::asio::io_context& ioc, Config config = {})
        : OptimizedAlpacaClient(ioc, config) {}

    // Telescope-specific methods
    boost::asio::awaitable<std::expected<double, AlpacaError>> get_right_ascension();
    boost::asio::awaitable<std::expected<double, AlpacaError>> get_declination();
    boost::asio::awaitable<std::expected<void, AlpacaError>> slew_to_coordinates(double ra, double dec);
    boost::asio::awaitable<std::expected<void, AlpacaError>> abort_slew();
    boost::asio::awaitable<std::expected<bool, AlpacaError>> get_slewing();
    boost::asio::awaitable<std::expected<void, AlpacaError>> park();
    boost::asio::awaitable<std::expected<void, AlpacaError>> unpark();
};

// Utility functions
namespace utils {
constexpr std::string_view device_type_to_string(DeviceType type) noexcept;
constexpr DeviceType string_to_device_type(std::string_view str) noexcept;

std::string encode_url(std::string_view str);
boost::json::object merge_params(const boost::json::object& base,
                                 const boost::json::object& additional);

// Error conversion
constexpr AlpacaError http_status_to_alpaca_error(unsigned status) noexcept;
std::string_view alpaca_error_to_string(AlpacaError error) noexcept;
}  // namespace utils

// Template implementations
template <typename T>
std::expected<T, AlpacaError> AlpacaResponse::extract() const {
    if (has_error()) {
        return std::unexpected(get_error());
    }

    try {
        if constexpr (std::same_as<T, nlohmann::json>) {
            return data["Value"];
        } else if constexpr (std::same_as<T, std::string>) {
            return data["Value"].get<std::string>();
        } else if constexpr (std::integral<T>) {
            return data["Value"].get<T>();
        } else if constexpr (std::floating_point<T>) {
            return data["Value"].get<T>();
        } else if constexpr (std::same_as<T, bool>) {
            return data["Value"].get<bool>();
        } else {
            // For complex types, use nlohmann::json conversion
            return data["Value"].get<T>();
        }
    } catch (...) {
        return std::unexpected(AlpacaError::ParseError);
    }
}

template <JsonConvertible T>
boost::asio::awaitable<std::expected<T, AlpacaError>> OptimizedAlpacaClient::get_property(
    std::string_view property) {
    auto response =
        co_await perform_request(boost::beast::http::verb::get, property);

    if (!response) {
        co_return std::unexpected(response.error());
    }

    co_return response->template extract<T>();
}

template <JsonConvertible... Args>
boost::asio::awaitable<std::expected<nlohmann::json, AlpacaError>> OptimizedAlpacaClient::invoke_method(
    std::string_view method, Args&&... args) {
    nlohmann::json params = build_transaction_params();

    // Use a helper function to add parameters
    auto add_param = [&params](auto&& arg) {
        if constexpr (requires {
                          arg.key;
                          arg.value;
                      }) {
            params[std::string(arg.key)] = arg.value;
        } else {
            // Handle other parameter types as needed
        }
    };

    // Add parameters using fold expression
    (add_param(std::forward<Args>(args)), ...);

    auto response =
        co_await perform_request(boost::beast::http::verb::put, method, params);

    if (!response) {
        co_return std::unexpected(response.error());
    }

    co_return response->template extract<nlohmann::json>();
}

}  // namespace lithium::device::ascom
