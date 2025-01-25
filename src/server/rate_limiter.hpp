#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

/**
 * @class RateLimiter
 * @brief A class to limit the rate of requests using a token bucket algorithm.
 */
class RateLimiter {
public:
    /**
     * @brief Constructs a RateLimiter with a specified maximum number of tokens
     * and refill interval.
     * @param max_tokens The maximum number of tokens.
     * @param refill_interval The interval at which tokens are refilled.
     */
    RateLimiter(int max_tokens, std::chrono::milliseconds refill_interval);

    /**
     * @brief Checks if a request is allowed based on the current token count.
     * @return True if the request is allowed, false otherwise.
     */
    bool allow_request();

    /**
     * @brief Gets the number of requests made within a specified time window.
     * @param window The time window to count requests.
     * @return The number of requests made within the specified time window.
     */
    int get_request_count(std::chrono::seconds window);

    /**
     * @brief Sets a new refill rate for the tokens.
     * @param new_refill_interval The new interval at which tokens are refilled.
     */
    void set_refill_rate(std::chrono::milliseconds new_refill_interval);

    /**
     * @brief Gets the remaining number of tokens.
     * @return The remaining number of tokens.
     */
    int get_remaining_tokens();

    /**
     * @brief Checks if a request from a specific IP address is allowed based on
     * the IP rate limiter.
     * @param ip The IP address to check.
     * @return True if the request is allowed, false otherwise.
     */
    bool allow_request_for_ip(const std::string& ip);

    /**
     * @brief Checks if a request from a specific user is allowed based on the
     * user rate limiter.
     * @param user_id The user ID to check.
     * @param max_requests_per_minute The maximum number of requests allowed per
     * minute.
     * @return True if the request is allowed, false otherwise.
     */
    bool allow_request_with_limit(const std::string& user_id,
                                  int max_requests_per_minute);

private:
    int max_tokens_;           ///< The maximum number of tokens.
    std::atomic<int> tokens_;  ///< The current number of tokens.
    std::chrono::milliseconds
        refill_interval_;  ///< The interval at which tokens are refilled.
    std::chrono::steady_clock::time_point
        last_refill_time_;  ///< The last time tokens were refilled.
    std::mutex mutex_;      ///< Mutex to synchronize access to tokens.
    std::deque<std::chrono::steady_clock::time_point>
        request_times_;  ///< Record of request times.

    /**
     * @struct IpRateLimiter
     * @brief A structure to limit the rate of requests based on IP address.
     */
    struct IpRateLimiter {
        std::deque<std::chrono::steady_clock::time_point>
            request_times;  ///< Record of request times.
        int max_requests_per_second =
            5;  ///< The maximum number of requests allowed per second.

        /**
         * @brief Checks if a request is allowed based on the current request
         * times.
         * @return True if the request is allowed, false otherwise.
         */
        bool allow_request();
    };

    std::unordered_map<std::string, IpRateLimiter>
        ip_limiters_;      ///< Map to store rate limiters for each IP address.
    std::mutex ip_mutex_;  ///< Mutex to synchronize access to IP limiters.

    /**
     * @struct UserRateLimiter
     * @brief A structure to limit the rate of requests based on user ID.
     */
    struct UserRateLimiter {
        std::deque<std::chrono::steady_clock::time_point>
            request_times;  ///< Record of request times.
    };

    std::unordered_map<std::string, UserRateLimiter>
        user_limiters_;      ///< Map to store rate limiters for each user ID.
    std::mutex user_mutex_;  ///< Mutex to synchronize access to user limiters.

    /**
     * @brief Records the time of a request.
     * @param now The current time.
     */
    void record_request_time(std::chrono::steady_clock::time_point now);
};

#endif  // RATE_LIMITER_H
