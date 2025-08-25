#pragma once

#include "performance_metrics.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace mcp::metrics {

    /**
     * @brief Structure to hold rate limiting configuration
     */
    struct RateLimitConfig {
        size_t max_requests_per_second = 100;       ///< Maximum requests allowed per second
        size_t max_concurrent_requests = 1000;      ///< Maximum concurrent requests
        size_t max_request_size = 1024 * 1024;      ///< Maximum request size in bytes (1MB)
        size_t max_response_size = 10 * 1024 * 1024;///< Maximum response size in bytes (10MB)
    };

    /**
     * @brief Rate limiting decision result
     */
    enum class RateLimitDecision {
        ALLOW,       ///< Request is allowed
        RATE_LIMITED,///< Request is rate limited
        TOO_LARGE    ///< Request/response is too large
    };

    /**
     * @brief Rate limiter for controlling traffic flow
     */
    class RateLimiter {
    public:
        using RateLimitCallback = std::function<void(
                const std::string &session_id,
                RateLimitDecision decision)>;

        /**
         * @brief Get the singleton instance of RateLimiter.
         * @return Shared pointer to RateLimiter instance
         */
        static std::shared_ptr<RateLimiter> getInstance();

        /**
         * @brief Set rate limit configuration
         * @param config Rate limit configuration
         */
        void set_config(const RateLimitConfig &config) {
            config_ = config;
        }

        /**
         * @brief Get current rate limit configuration
         * @return Current rate limit configuration
         */
        const RateLimitConfig &get_config() const {
            return config_;
        }

        /**
         * @brief Set callback for rate limiting events
         * @param callback Function to call when rate limiting decisions are made
         */
        void set_rate_limit_callback(RateLimitCallback callback) {
            rate_limit_callback_ = std::move(callback);
        }

        /**
         * @brief Check if a request should be allowed based on rate limiting rules
         * @param request The HTTP request being checked
         * @param session_id Session identifier
         * @return RateLimitDecision indicating if the request is allowed
         */
        RateLimitDecision check_request_allowed(
                const TrackedHttpRequest &request,
                const std::string &session_id);

        /**
         * @brief Report completion of a request (to update counters)
         * @param session_id Session identifier
         */
        void report_request_completed(const std::string &session_id);

        /**
         * @brief Report start of a request (to update counters)
         * @param session_id Session identifier
         */
        void report_request_started(const std::string &session_id);

    private:
        /**
         * @brief Private constructor for singleton pattern.
         */
        RateLimiter() = default;

        RateLimitConfig config_;
        RateLimitCallback rate_limit_callback_;

        // Track concurrent requests
        std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> active_requests_;

        // Track request rates per second
        std::unordered_map<std::string, std::vector<std::chrono::high_resolution_clock::time_point>> request_timestamps_;
    };

}// namespace mcp::metrics