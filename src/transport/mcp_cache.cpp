#include "mcp_cache.h"
#include "LRUCache.hpp"
#include "core/logger.h"
#include <algorithm>


namespace mcp::cache {

    ////////////////////////////////////////////////////////////////////////////////
    /// SessionState serialization/deserialization implementation
    ////////////////////////////////////////////////////////////////////////////////

    nlohmann::json SessionState::to_json() const {
        return {
                {"session_id", session_id},
                {"tool_name", tool_name},
                {"last_event_id", last_event_id},
                {"is_active", is_active},
                {"last_update", std::chrono::duration_cast<std::chrono::seconds>(
                                        last_update.time_since_epoch())
                                        .count()}};
    }

    SessionState SessionState::from_json(const nlohmann::json &j) {
        SessionState state;
        state.session_id = j["session_id"];
        state.tool_name = j["tool_name"];
        state.last_event_id = j["last_event_id"];
        state.is_active = j["is_active"];
        state.last_update = std::chrono::system_clock::time_point(
                std::chrono::seconds(j["last_update"]));
        return state;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// McpCache instance acquisition and initialization
    ////////////////////////////////////////////////////////////////////////////////

    // McpCache singleton implementation
    McpCache *McpCache::GetInstance() {
        static McpCache instance;
        return &instance;
    }

    // Initialize cache configuration
    void McpCache::Init(size_t max_session_count, size_t max_data_per_session, std::chrono::seconds ttl) {
        std::lock_guard<std::mutex> lock(mtx_);
        // Allow re-initialization to support testing
        if (is_initialized_) {
            if (session_cache_) session_cache_->StopCleanupThread();
            if (data_cache_) data_cache_->StopCleanupThread();
            if (event_list_cache_) event_list_cache_->StopCleanupThread();
        }

        // Initialize underlying LRU cache
        max_session_count_ = max_session_count;
        max_data_per_session_ = max_data_per_session;
        ttl_ = ttl;

        // Session cache capacity: max_session_count
        session_cache_ = std::make_unique<CacheType>(max_session_count, 10, ttl);
        // Data cache capacity: max_session_count * max_data_per_session (with redundancy)
        data_cache_ = std::make_unique<CacheType>(max_session_count * max_data_per_session * 2, 10, ttl);
        // Event list cache capacity: max_session_count
        event_list_cache_ = std::make_unique<CacheType>(max_session_count, 10, ttl);

        // Start automatic cleanup thread (runs every 30 seconds)
        session_cache_->StartCleanupThread(std::chrono::seconds(30));
        data_cache_->StartCleanupThread(std::chrono::seconds(30));
        event_list_cache_->StartCleanupThread(std::chrono::seconds(30));

        is_initialized_ = true;
        MCP_INFO("McpCache initialized (max sessions: {}, max data per session: {}, ttl: {}s)",
                 max_session_count, max_data_per_session, ttl.count());
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Cache key generation methods
    ////////////////////////////////////////////////////////////////////////////////

    std::string McpCache::GetSessionKey(const std::string &session_id) const {
        return "session:" + session_id;
    }

    std::string McpCache::GetDataKey(const std::string &session_id, int event_id) const {
        return "data:" + session_id + ":" + std::to_string(event_id);
    }

    std::string McpCache::GetEventListKey(const std::string &session_id) const {
        return "event_list:" + session_id;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Session state management
    ////////////////////////////////////////////////////////////////////////////////

    bool McpCache::SaveSessionState(const SessionState &state) {
        if (!IsInitialized()) {
            MCP_ERROR("McpCache not initialized - SaveSessionState failed");
            return false;
        }

        std::lock_guard<std::mutex> lock(mtx_);
        try {
            std::string key = GetSessionKey(state.session_id);
            std::string value = state.to_json().dump();
            session_cache_->Put(key, value, ttl_);
            MCP_DEBUG("Saved session state - session: {}", state.session_id);
            return true;
        } catch (const std::exception &e) {
            MCP_ERROR("SaveSessionState failed: {}", e.what());
            return false;
        }
    }

    std::optional<SessionState> McpCache::GetSessionState(const std::string &session_id) {
        if (!IsInitialized()) {
            MCP_ERROR("McpCache not initialized - GetSessionState failed");
            return std::nullopt;
        }

        std::lock_guard<std::mutex> lock(mtx_);
        try {
            std::string key = GetSessionKey(session_id);
            auto value_opt = session_cache_->Get(key);
            if (!value_opt.has_value()) {
                MCP_DEBUG("No session state found - session: {}", session_id);
                return std::nullopt;
            }

            return SessionState::from_json(nlohmann::json::parse(value_opt.value()));
        } catch (const std::exception &e) {
            MCP_ERROR("GetSessionState failed: {}", e.what());
            return std::nullopt;
        }
    }

    bool McpCache::UpdateSessionState(const std::string &session_id, int event_id) {
        auto state_opt = GetSessionState(session_id);
        if (!state_opt.has_value()) {
            MCP_WARN("UpdateSessionState failed: session not found - {}", session_id);
            return false;
        }

        SessionState state = state_opt.value();
        state.last_event_id = event_id;
        state.last_update = std::chrono::system_clock::now();
        return SaveSessionState(state);
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Stream data cache management
    ////////////////////////////////////////////////////////////////////////////////

    bool McpCache::CacheStreamData(const std::string &session_id, int event_id, const nlohmann::json &data) {
        if (!IsInitialized()) {
            MCP_ERROR("McpCache not initialized - CacheStreamData failed");
            return false;
        }

        std::lock_guard<std::mutex> lock(mtx_);
        try {
            // 1. Store data using event_id as key
            std::string data_key = GetDataKey(session_id, event_id);
            data_cache_->Put(data_key, data.dump(), ttl_);

            // 2. Update event ID list for this session
            std::string list_key = GetEventListKey(session_id);
            std::vector<int> event_list;

            // Read existing event list if available
            auto list_opt = event_list_cache_->Get(list_key);
            if (list_opt.has_value()) {
                event_list = nlohmann::json::parse(list_opt.value()).get<std::vector<int>>();
            }

            // Add new event_id with deduplication
            if (std::find(event_list.begin(), event_list.end(), event_id) == event_list.end()) {
                event_list.push_back(event_id);
                // Limit list length to max_data_per_session_
                if (event_list.size() > max_data_per_session_) {
                    event_list.erase(event_list.begin(),
                                     event_list.begin() + (event_list.size() - max_data_per_session_));
                }
                // Save updated event list
                event_list_cache_->Put(list_key, nlohmann::json(event_list).dump(), ttl_);
            }

            MCP_DEBUG("Cached stream data - session: {}, event: {}", session_id, event_id);
            return true;
        } catch (const std::exception &e) {
            MCP_ERROR("CacheStreamData failed: {}", e.what());
            return false;
        }
    }

    std::vector<nlohmann::json> McpCache::GetReconnectData(
            const std::string &session_id,
            int last_event_id) {
        std::vector<nlohmann::json> result;
        if (!IsInitialized()) {
            MCP_ERROR("McpCache not initialized - GetReconnectData failed");
            return result;
        }

        std::lock_guard<std::mutex> lock(mtx_);
        try {
            // 1. Clean up expired data first
            CleanupExpiredData();

            // 2. Get event list for this session
            std::string list_key = GetEventListKey(session_id);
            auto list_opt = event_list_cache_->Get(list_key);
            if (!list_opt.has_value()) {
                MCP_DEBUG("No event list found - session: {}", session_id);
                return result;
            }

            // 3. Filter events newer than last received event
            std::vector<int> event_list = nlohmann::json::parse(list_opt.value()).get<std::vector<int>>();
            std::vector<int> target_events;

            for (int event: event_list) {
                if (event > last_event_id) {
                    target_events.push_back(event);
                }
            }
            std::sort(target_events.begin(), target_events.end());

            // 4. Retrieve cached data for target events
            for (int event: target_events) {
                std::string data_key = GetDataKey(session_id, event);
                auto data_opt = data_cache_->Get(data_key);
                if (data_opt.has_value()) {
                    result.push_back(nlohmann::json::parse(data_opt.value()));
                }
            }

            MCP_DEBUG("Found {} reconnect data items - session: {}", result.size(), session_id);
            return result;
        } catch (const std::exception &e) {
            MCP_ERROR("GetReconnectData failed: {}", e.what());
            return result;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Session cleanup operations
    ////////////////////////////////////////////////////////////////////////////////

    bool McpCache::CleanupSession(const std::string &session_id) {
        if (!IsInitialized()) {
            MCP_ERROR("McpCache not initialized - CleanupSession failed");
            return false;
        }

        std::lock_guard<std::mutex> lock(mtx_);
        try {
            // 1. Delete session state
            std::string session_key = GetSessionKey(session_id);
            session_cache_->Remove(session_key);

            // 2. Delete event list and associated data
            std::string list_key = GetEventListKey(session_id);
            auto list_opt = event_list_cache_->Get(list_key);
            event_list_cache_->Remove(list_key);

            // 3. Delete all cached data for this session
            if (list_opt.has_value()) {
                std::vector<int> event_list = nlohmann::json::parse(list_opt.value()).get<std::vector<int>>();
                for (int event: event_list) {
                    std::string data_key = GetDataKey(session_id, event);
                    data_cache_->Remove(data_key);
                }
            }

            MCP_DEBUG("Cleaned up session cache - session: {}", session_id);
            return true;
        } catch (const std::exception &e) {
            MCP_ERROR("CleanupSession failed: {}", e.what());
            return false;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// Expired data cleanup
    ////////////////////////////////////////////////////////////////////////////////

    void McpCache::CleanupExpiredData() {
        if (!IsInitialized()) return;

        // Manually trigger LRU cache expiration cleanup
        session_cache_->CleanUpExpiredItems();
        data_cache_->CleanUpExpiredItems();
        event_list_cache_->CleanUpExpiredItems();

        MCP_DEBUG("McpCache cleanup completed - sessions: {}, data: {}, event_lists: {}",
                  session_cache_->Size(),
                  data_cache_->Size(),
                  event_list_cache_->Size());
    }

}// namespace mcp::cache