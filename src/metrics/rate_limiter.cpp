#include "rate_limiter.h"
#include "core/logger.h"
#include <algorithm>

namespace mcp::metrics {

    // static shared instance
    static std::shared_ptr<RateLimiter> instance = nullptr;

    std::shared_ptr<RateLimiter> RateLimiter::getInstance() {
        if (!instance) {
            instance = std::shared_ptr<RateLimiter>(new RateLimiter());
        }
        return instance;
    }

    RateLimitDecision RateLimiter::check_request_allowed(
            const TrackedHttpRequest &request,
            const std::string &session_id) {

        // Check request size
        if (request.body.size() > config_.max_request_size) {
            MCP_WARN("Request too large - Session: {}, Size: {}, Max: {}",
                     session_id, request.body.size(), config_.max_request_size);

            if (rate_limit_callback_) {
                rate_limit_callback_(session_id, RateLimitDecision::TOO_LARGE);
            }
            return RateLimitDecision::TOO_LARGE;
        }

        // Check concurrent requests
        if (active_requests_.size() >= config_.max_concurrent_requests) {
            MCP_WARN("Too many concurrent requests - Session: {}, Active: {}, Max: {}",
                     session_id, active_requests_.size(), config_.max_concurrent_requests);

            if (rate_limit_callback_) {
                rate_limit_callback_(session_id, RateLimitDecision::RATE_LIMITED);
            }
            return RateLimitDecision::RATE_LIMITED;
        }

        // Check requests per second
        auto now = std::chrono::high_resolution_clock::now();
        auto &timestamps = request_timestamps_[session_id];

        // Remove timestamps older than 1 second
        timestamps.erase(
                std::remove_if(timestamps.begin(), timestamps.end(),
                               [now](const auto &timestamp) {
                                   auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           now - timestamp);
                                   return diff.count() >= 1000;
                               }),
                timestamps.end());

        // Check if we're exceeding the rate limit
        if (timestamps.size() >= config_.max_requests_per_second) {
            MCP_WARN("Rate limit exceeded - Session: {}, Requests: {}, Max: {}",
                     session_id, timestamps.size(), config_.max_requests_per_second);

            if (rate_limit_callback_) {
                rate_limit_callback_(session_id, RateLimitDecision::RATE_LIMITED);
            }
            return RateLimitDecision::RATE_LIMITED;
        }

        // Request is allowed
        if (rate_limit_callback_) {
            rate_limit_callback_(session_id, RateLimitDecision::ALLOW);
        }
        return RateLimitDecision::ALLOW;
    }

    void RateLimiter::report_request_started(const std::string &session_id) {
        auto now = std::chrono::high_resolution_clock::now();
        active_requests_[session_id] = now;
        request_timestamps_[session_id].push_back(now);
    }

    void RateLimiter::report_request_completed(const std::string &session_id) {
        active_requests_.erase(session_id);

        // We keep the timestamp for rate limiting calculations
        // It will be cleaned up in check_request_allowed when it expires
    }

}// namespace mcp::metrics