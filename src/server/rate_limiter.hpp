#ifndef RATE_LIMITER_HPP
#define RATE_LIMITER_HPP

#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>


/**
 * @brief High-performance token bucket rate limiter with IP and user-based
 * limiting
 *
 * This class implements a **thread-safe rate limiter** using the token bucket
 * algorithm, with additional **IP-based** and **user-based rate limiting**
 * capabilities. All operations are **lock-optimized** for maximum performance.
 */
class RateLimiter {
public:
    /**
     * @brief Constructs a RateLimiter with specified parameters
     * @param max_tokens **Maximum token capacity** in the bucket
     * @param refill_interval **Time interval** between token refills
     */
    explicit RateLimiter(int max_tokens,
                         std::chrono::milliseconds refill_interval);

    /**
     * @brief Attempts to consume a token for request processing
     * @return **true** if request is allowed, **false** if rate limited
     * @note **Thread-safe** and **high-performance** implementation
     */
    bool allow_request();

    /**
     * @brief Gets request count within specified time window
     * @param window **Time window** for counting requests
     * @return **Number of requests** made within the window
     */
    int get_request_count(std::chrono::seconds window);

    /**
     * @brief Updates the token refill rate dynamically
     * @param new_refill_interval **New refill interval** to set
     */
    void set_refill_rate(std::chrono::milliseconds new_refill_interval);

    /**
     * @brief Gets current available token count
     * @return **Remaining tokens** available for consumption
     * @note Automatically triggers refill calculation
     */
    int get_remaining_tokens();

    /**
     * @brief IP-based rate limiting with per-second throttling
     * @param ip **IP address** to check for rate limiting
     * @return **true** if IP request is allowed, **false** if throttled
     */
    bool allow_request_for_ip(const std::string& ip);

    /**
     * @brief User-based rate limiting with configurable per-minute limits
     * @param user_id **User identifier** for rate limiting
     * @param max_requests_per_minute **Maximum requests** allowed per minute
     * @return **true** if user request is allowed, **false** if exceeded limit
     */
    bool allow_request_with_limit(const std::string& user_id,
                                  int max_requests_per_minute);

private:
    const int max_tokens_;     ///< **Maximum token capacity**
    std::atomic<int> tokens_;  ///< **Current available tokens**
    std::atomic<std::chrono::milliseconds>
        refill_interval_;  ///< **Token refill interval**
    std::atomic<std::chrono::steady_clock::time_point>
        last_refill_time_;  ///< **Last refill timestamp**

    mutable std::mutex token_mutex_;  ///< **Token access synchronization**
    std::deque<std::chrono::steady_clock::time_point>
        request_timestamps_;  ///< **Request time tracking**

    /**
     * @brief **High-performance IP rate limiter** with sliding window
     */
    struct IpRateLimiter {
        std::deque<std::chrono::steady_clock::time_point>
            request_timestamps;  ///< **IP request timestamps**
        static constexpr int MAX_REQUESTS_PER_SECOND =
            5;  ///< **Default IP rate limit**

        /**
         * @brief Checks if IP request is within rate limits
         * @return **true** if allowed, **false** if rate limited
         */
        bool allow_request();

        /**
         * @brief **Optimized cleanup** of expired timestamps
         * @param current_time **Current timestamp** for comparison
         */
        void cleanup_expired_requests(
            std::chrono::steady_clock::time_point current_time);
    };

    /**
     * @brief **User-specific rate limiter** with sliding window
     */
    struct UserRateLimiter {
        std::deque<std::chrono::steady_clock::time_point>
            request_timestamps;  ///< **User request timestamps**

        /**
         * @brief **Optimized cleanup** of expired requests
         * @param current_time **Current timestamp** for comparison
         */
        void cleanup_expired_requests(
            std::chrono::steady_clock::time_point current_time);
    };

    std::unordered_map<std::string, IpRateLimiter>
        ip_rate_limiters_;  ///< **IP-based rate limiters**
    std::unordered_map<std::string, UserRateLimiter>
        user_rate_limiters_;  ///< **User-based rate limiters**

    mutable std::mutex ip_limiters_mutex_;  ///< **IP limiters synchronization**
    mutable std::mutex
        user_limiters_mutex_;  ///< **User limiters synchronization**

    /**
     * @brief **Records request timestamp** for tracking
     * @param timestamp **Current request timestamp**
     */
    void record_request_timestamp(
        std::chrono::steady_clock::time_point timestamp);

    /**
     * @brief **Optimized token refill** calculation
     * @param current_time **Current timestamp** for refill calculation
     * @return **Number of tokens** added during refill
     */
    int calculate_token_refill(
        std::chrono::steady_clock::time_point current_time);
};

#endif  // RATE_LIMITER_HPP
