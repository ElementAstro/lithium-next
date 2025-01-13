#include "rate_limiter.hpp"
#include <algorithm>
#include <chrono>

#include "atom/log/loguru.hpp"

RateLimiter::RateLimiter(int max_tokens,
                         std::chrono::milliseconds refill_interval)
    : max_tokens_(max_tokens),
      tokens_(max_tokens),
      refill_interval_(refill_interval),
      last_refill_time_(std::chrono::steady_clock::now()) {}

bool RateLimiter::allow_request() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_refill_time_);

    if (elapsed_time >= refill_interval_) {
        int new_tokens =
            static_cast<int>(elapsed_time.count() / refill_interval_.count());
        tokens_ = std::min(max_tokens_, tokens_ + new_tokens);
        LOG_F(INFO, "Tokens refilled: added %d tokens, current tokens: %d",
              new_tokens, tokens_);
        last_refill_time_ = now;
    }

    if (tokens_ > 0) {
        tokens_--;
        record_request_time(now);
        LOG_F(INFO, "Request allowed, remaining tokens: %d", tokens_);
        return true;
    } else {
        LOG_F(WARNING, "Request denied, no tokens available");
        return false;
    }
}

void RateLimiter::set_refill_rate(
    std::chrono::milliseconds new_refill_interval) {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_F(INFO, "Refill rate changed from %lld ms to %lld ms",
          refill_interval_.count(), new_refill_interval.count());
    refill_interval_ = new_refill_interval;
}

bool RateLimiter::allow_request_for_ip(const std::string& ip) {
    std::lock_guard<std::mutex> lock(ip_mutex_);
    auto& ip_limiter = ip_limiters_[ip];
    bool allowed = ip_limiter.allow_request();
    if (allowed) {
        LOG_F(INFO, "IP request allowed for {}", ip);
    } else {
        LOG_F(WARNING, "IP request denied for {} - rate limit exceeded", ip);
    }
    return allowed;
}

bool RateLimiter::allow_request_with_limit(const std::string& user_id,
                                           int max_requests_per_minute) {
    std::lock_guard<std::mutex> lock(user_mutex_);
    auto& user_limiter = user_limiters_[user_id];

    auto now = std::chrono::steady_clock::now();
    user_limiter.request_times.push_back(now);

    user_limiter.request_times.erase(
        std::remove_if(
            user_limiter.request_times.begin(),
            user_limiter.request_times.end(),
            [now](const auto& t) {
                return std::chrono::duration_cast<std::chrono::minutes>(now - t)
                           .count() >= 1;
            }),
        user_limiter.request_times.end());

    bool allowed = static_cast<int>(user_limiter.request_times.size()) <
                   max_requests_per_minute;
    if (allowed) {
        LOG_F(INFO,
              "User request allowed for {} (requests in last minute: %zu/%d)",
              user_id, user_limiter.request_times.size(),
              max_requests_per_minute);
    } else {
        LOG_F(WARNING,
              "User request denied for {} - rate limit exceeded (%zu/%d)",
              user_id, user_limiter.request_times.size(),
              max_requests_per_minute);
    }
    return allowed;
}

bool RateLimiter::IpRateLimiter::allow_request() {
    auto now = std::chrono::steady_clock::now();
    request_times.push_back(now);

    request_times.erase(
        std::remove_if(
            request_times.begin(), request_times.end(),
            [now](const auto& t) {
                return std::chrono::duration_cast<std::chrono::seconds>(now - t)
                           .count() >= 1;
            }),
        request_times.end());

    return static_cast<int>(request_times.size()) <= max_requests_per_second;
}