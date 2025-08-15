#include "metrics_manager.h"
#include "core/logger.h"

namespace mcp::metrics {
    
    // static shared instance
    static std::shared_ptr<MetricsManager> instance = nullptr;
    
    std::shared_ptr<MetricsManager> MetricsManager::getInstance() {
        if (!instance) {
            instance = std::shared_ptr<MetricsManager>(new MetricsManager());
            
            // default callback
            instance->set_performance_callback([](
                const TrackedHttpRequest& request,
                const PerformanceMetrics& metrics,
                const std::string& session_id) {
                    MCP_DEBUG("Performance - Session: {}, Method: {}, Target: {}, Duration: {:.2f}ms, RPS: {:.2f}",
                             session_id,
                             request.method,
                             request.target,
                             metrics.duration_ms(),
                             metrics.requests_per_second());
            });
            
            // default error callback
            instance->set_error_callback([](
                const std::string& error_message,
                const std::string& session_id) {
                    MCP_ERROR("Metrics Error - Session: {}, Message: {}", session_id, error_message);
            });
        }
        return instance;
    }
    
}