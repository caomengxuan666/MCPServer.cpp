#pragma once

#include "performance_metrics.h"
#include <functional>
#include <memory>
#include <string>

namespace mcp::metrics {

    /**
     * @brief Metrics manager for handling performance metrics callbacks.
     */
    class MetricsManager {
    public:
        using PerformanceCallback = std::function<void(
            const TrackedHttpRequest& request,
            const PerformanceMetrics& metrics,
            const std::string& session_id
        )>;
        
        using ErrorCallback = std::function<void(
            const std::string& error_message,
            const std::string& session_id
        )>;
        
        /**
         * @brief Get the singleton instance of MetricsManager.
         * @return Shared pointer to MetricsManager instance
         */
        static std::shared_ptr<MetricsManager> getInstance();
        
        /**
         * @brief Set callback for performance metrics reporting.
         * @param callback Function to call with performance metrics
         */
        void set_performance_callback(PerformanceCallback callback) {
            performance_callback_ = std::move(callback);
        }
        
        /**
         * @brief Set callback for error reporting.
         * @param callback Function to call on errors
         */
        void set_error_callback(ErrorCallback callback) {
            error_callback_ = std::move(callback);
        }
        
        /**
         * @brief Report performance metrics.
         * @param request The HTTP request being tracked
         * @param metrics Performance metrics for the request
         * @param session_id Session identifier
         */
        void report_performance(const TrackedHttpRequest& request, 
                              const PerformanceMetrics& metrics, 
                              const std::string& session_id) {
            if (performance_callback_) {
                performance_callback_(request, metrics, session_id);
            }
        }
        
        /**
         * @brief Report an error.
         * @param error_message Error message
         * @param session_id Session identifier
         */
        void report_error(const std::string& error_message, const std::string& session_id) {
            if (error_callback_) {
                error_callback_(error_message, session_id);
            }
        }
        
    private:
        /**
         * @brief Private constructor for singleton pattern.
         */
        MetricsManager() = default;
        
        PerformanceCallback performance_callback_;
        ErrorCallback error_callback_;
    };

}