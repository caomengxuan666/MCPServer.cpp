#pragma once

namespace mcp {
    namespace config {
        struct GlobalConfig;
        class ConfigObserver {
        public:
            virtual ~ConfigObserver() = default;
            virtual void onConfigReloaded(const mcp::config::GlobalConfig &newConfig) = 0;
        };
    }// namespace config
}// namespace mcp