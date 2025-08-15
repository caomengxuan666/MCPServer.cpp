#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace mcp::metrics {

    /**
     * @brief HTTP request structure for performance tracking.
     */
    struct TrackedHttpRequest {
        std::string method;                                    ///< HTTP method (GET, POST, etc.)
        std::string target;                                    ///< Request target/URL
        std::string version;                                   ///< HTTP version
        std::unordered_map<std::string, std::string> headers;  ///< HTTP headers
        std::string body;                                      ///< Request body
    };

    /**
     * @brief Performance metrics structure for tracking request handling performance.
     */
    struct PerformanceMetrics {
        std::chrono::high_resolution_clock::time_point start_time;  ///< Start time of request handling
        std::chrono::high_resolution_clock::time_point end_time;    ///< End time of request handling
        size_t request_size;                                            ///< Size of the request in bytes
        size_t response_size;                                           ///< Size of the response in bytes
        
        /**
         * @brief Calculate the duration of request handling in milliseconds.
         * @return Duration in milliseconds
         */
        double duration_ms() const {
            return std::chrono::duration<double, std::milli>(end_time - start_time).count();
        }
        
        /**
         * @brief Calculate requests per second based on the duration.
         * @return Requests per second
         */
        double requests_per_second() const {
            if (duration_ms() > 0) {
                return 1000.0 / duration_ms();
            }
            return 0.0;
        }
    };

    /**
     * @brief Performance tracker for HTTP requests.
     */
    class PerformanceTracker {
    public:
        /**
         * @brief Start tracking a request.
         * @return PerformanceMetrics with start time set
         */
        static PerformanceMetrics start_tracking(size_t request_size = 0) {
            PerformanceMetrics metrics{};
            metrics.start_time = std::chrono::high_resolution_clock::now();
            metrics.request_size = request_size;
            metrics.response_size = 0;
            return metrics;
        }
        
        /**
         * @brief End tracking a request.
         * @param metrics PerformanceMetrics to update
         * @param response_size Size of the response in bytes
         */
        static void end_tracking(PerformanceMetrics& metrics, size_t response_size = 0) {
            metrics.end_time = std::chrono::high_resolution_clock::now();
            metrics.response_size = response_size;
        }
    };

}