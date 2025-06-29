#include "rate_limiter.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>


RateLimiter::RateLimiter(int max_tokens,
                         std::chrono::milliseconds refill_interval)
    : max_tokens_(max_tokens),
      tokens_(max_tokens),
      refill_interval_(refill_interval),
      last_refill_time_(std::chrono::steady_clock::now()) {
    spdlog::info("RateLimiter initialized: max_tokens={}, refill_interval={}ms",
                 max_tokens, refill_interval.count());
}

bool RateLimiter::allow_request() {
    const auto current_time = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(token_mutex_);

        const int refilled_tokens = calculate_token_refill(current_time);
        if (refilled_tokens > 0) {
            spdlog::debug("Tokens refilled: added={}, current={}",
                          refilled_tokens, tokens_.load());
        }

        if (tokens_.load() > 0) {
            tokens_.fetch_sub(1);
            record_request_timestamp(current_time);
            spdlog::debug("Request allowed: remaining_tokens={}",
                          tokens_.load());
            return true;
        }
    }

    spdlog::warn("Request denied: no tokens available");
    return false;
}

int RateLimiter::calculate_token_refill(
    std::chrono::steady_clock::time_point current_time) {
    const auto elapsed_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - last_refill_time_.load());

    if (elapsed_time >= refill_interval_.load()) {
        const int tokens_to_add = static_cast<int>(
            elapsed_time.count() / refill_interval_.load().count());

        const int current_tokens = tokens_.load();
        const int new_token_count =
            std::min(max_tokens_, current_tokens + tokens_to_add);

        tokens_.store(new_token_count);
        last_refill_time_.store(current_time);

        return tokens_to_add;
    }

    return 0;
}

void RateLimiter::set_refill_rate(
    std::chrono::milliseconds new_refill_interval) {
    const auto old_interval = refill_interval_.exchange(new_refill_interval);
    spdlog::info("Refill rate updated: old={}ms, new={}ms",
                 old_interval.count(), new_refill_interval.count());
}

bool RateLimiter::allow_request_for_ip(const std::string& ip) {
    std::lock_guard<std::mutex> lock(ip_limiters_mutex_);

    auto& ip_limiter = ip_rate_limiters_[ip];
    const bool request_allowed = ip_limiter.allow_request();

    if (request_allowed) {
        spdlog::debug("IP request allowed: ip={}", ip);
    } else {
        spdlog::warn("IP request denied: ip={}, rate_limit_exceeded", ip);
    }

    return request_allowed;
}

bool RateLimiter::allow_request_with_limit(const std::string& user_id,
                                           int max_requests_per_minute) {
    const auto current_time = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(user_limiters_mutex_);

    auto& user_limiter = user_rate_limiters_[user_id];
    user_limiter.request_timestamps.push_back(current_time);
    user_limiter.cleanup_expired_requests(current_time);

    const auto current_request_count =
        static_cast<int>(user_limiter.request_timestamps.size());
    const bool request_allowed =
        current_request_count <= max_requests_per_minute;

    if (request_allowed) {
        spdlog::debug("User request allowed: user_id={}, requests_count={}/{}",
                      user_id, current_request_count, max_requests_per_minute);
    } else {
        spdlog::warn(
            "User request denied: user_id={}, rate_limit_exceeded ({}/{})",
            user_id, current_request_count, max_requests_per_minute);
    }

    return request_allowed;
}

int RateLimiter::get_request_count(std::chrono::seconds window) {
    const auto current_time = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(token_mutex_);

    request_timestamps_.erase(
        std::remove_if(
            request_timestamps_.begin(), request_timestamps_.end(),
            [current_time, window](const auto& timestamp) {
                return std::chrono::duration_cast<std::chrono::seconds>(
                           current_time - timestamp) >= window;
            }),
        request_timestamps_.end());

    return static_cast<int>(request_timestamps_.size());
}

int RateLimiter::get_remaining_tokens() {
    const auto current_time = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(token_mutex_);
    calculate_token_refill(current_time);

    return tokens_.load();
}

void RateLimiter::record_request_timestamp(
    std::chrono::steady_clock::time_point timestamp) {
    request_timestamps_.push_back(timestamp);
}

bool RateLimiter::IpRateLimiter::allow_request() {
    const auto current_time = std::chrono::steady_clock::now();

    request_timestamps.push_back(current_time);
    cleanup_expired_requests(current_time);

    return static_cast<int>(request_timestamps.size()) <=
           MAX_REQUESTS_PER_SECOND;
}

void RateLimiter::IpRateLimiter::cleanup_expired_requests(
    std::chrono::steady_clock::time_point current_time) {
    request_timestamps.erase(
        std::remove_if(
            request_timestamps.begin(), request_timestamps.end(),
            [current_time](const auto& timestamp) {
                return std::chrono::duration_cast<std::chrono::seconds>(
                           current_time - timestamp)
                           .count() >= 1;
            }),
        request_timestamps.end());
}

void RateLimiter::UserRateLimiter::cleanup_expired_requests(
    std::chrono::steady_clock::time_point current_time) {
    request_timestamps.erase(
        std::remove_if(
            request_timestamps.begin(), request_timestamps.end(),
            [current_time](const auto& timestamp) {
                return std::chrono::duration_cast<std::chrono::minutes>(
                           current_time - timestamp)
                           .count() >= 1;
            }),
        request_timestamps.end());
}
