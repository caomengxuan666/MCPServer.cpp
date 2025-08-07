#pragma once
#include "nlohmann/json.hpp"
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Forward declaration of LRUCache
namespace Astra::datastructures {
    template<typename Key, typename Value>
    class LRUCache;
}

namespace mcp::cache {

    /**
     * @brief Session state structure for recording key information before disconnection
     * 
     * This structure maintains critical session information that allows for 
     * seamless reconnection and data recovery when a client reconnects.
     */
    struct SessionState {
        std::string session_id;                           ///< Unique session identifier
        std::string tool_name;                            ///< Associated tool name
        int last_event_id = 0;                            ///< Last sent event ID
        bool is_active = true;                            ///< Session active status
        std::chrono::system_clock::time_point last_update;///< Last update timestamp

        /**
         * @brief Serialize to JSON for local storage
         * @return JSON representation of the session state
         */
        nlohmann::json to_json() const;

        /**
         * @brief Deserialize from JSON
         * @param j JSON object to deserialize from
         * @return Deserialized SessionState object
         */
        static SessionState from_json(const nlohmann::json &j);
    };

    /**
     * @brief Core cache class based on LRUCache implementation
     * 
     * Provides local caching support for disconnection and reconnection 
     * data recovery, enabling seamless user experience even when network 
     * interruptions occur.
     */
    class McpCache {
    public:
        /**
         * @brief Singleton pattern: Global unique cache instance
         * @return Pointer to the singleton instance
         */
        static McpCache *GetInstance();

        /**
         * @brief Initialize cache (set capacity and expiration time)
         * @param max_session_count Maximum number of sessions
         * @param max_data_per_session Maximum data items per session
         * @param ttl Time to live for cached items
         */
        void Init(size_t max_session_count = 1000,
                  size_t max_data_per_session = 500,
                  std::chrono::seconds ttl = std::chrono::hours(24));

        /**
         * @brief Save session state (called before disconnection)
         * @param state Session state to save
         * @return true if successful, false otherwise
         */
        bool SaveSessionState(const SessionState &state);

        /**
         * @brief Get session state (called during reconnection)
         * @param session_id Session identifier
         * @return Optional containing session state if found
         */
        std::optional<SessionState> GetSessionState(const std::string &session_id);

        /**
         * @brief Update session state (called after sending data)
         * @param session_id Session identifier
         * @param event_id Last sent event identifier
         * @return true if successful, false otherwise
         */
        bool UpdateSessionState(const std::string &session_id, int event_id);

        /**
         * @brief Cache streaming data (synchronously cached during real-time sending)
         * @param session_id Session identifier
         * @param event_id Event identifier for this data chunk
         * @param data Data to cache
         * @return true if successful, false otherwise
         */
        bool CacheStreamData(const std::string &session_id, int event_id, const nlohmann::json &data);

        /**
         * @brief Get reconnection data (called during recovery after disconnection)
         * @param session_id Session identifier
         * @param last_event_id Last received event identifier from client
         * @return Vector of JSON objects containing reconnection data
         */
        std::vector<nlohmann::json> GetReconnectData(
                const std::string &session_id,
                int last_event_id);

        /**
         * @brief Clean up session cache (called when session ends normally)
         * @param session_id Session identifier
         * @return true if successful, false otherwise
         */
        bool CleanupSession(const std::string &session_id);

        /**
         * @brief Actively clean up all expired data (alternative to timer tasks)
         */
        void CleanupExpiredData();

        /**
         * @brief Check if cache is successfully initialized
         * @return true if initialized, false otherwise
         */
        bool IsInitialized() const { return is_initialized_; }

    private:
        McpCache() = default;///< Prevent external instantiation
        ~McpCache() = default;

        /**
         * @brief Generate cache key for session state
         * @param session_id Session identifier
         * @return Generated cache key
         */
        std::string GetSessionKey(const std::string &session_id) const;

        /**
         * @brief Generate data cache key using event ID
         * @param session_id Session identifier
         * @param event_id Event identifier
         * @return Generated cache key
         */
        std::string GetDataKey(const std::string &session_id, int event_id) const;

        /**
         * @brief Generate event list cache key
         * @param session_id Session identifier
         * @return Generated cache key
         */
        std::string GetEventListKey(const std::string &session_id) const;

        std::mutex mtx_;             ///< Thread safety mutex
        bool is_initialized_ = false;///< Initialization status
        std::chrono::seconds ttl_;   ///< Default expiration time
        size_t max_session_count_;   ///< Maximum number of sessions
        size_t max_data_per_session_;///< Maximum data items per session

        /// Underlying cache storage: key is string, value is JSON string
        using CacheType = Astra::datastructures::LRUCache<std::string, std::string>;
        std::unique_ptr<CacheType> session_cache_;   ///< Stores session states
        std::unique_ptr<CacheType> data_cache_;      ///< Stores streaming data
        std::unique_ptr<CacheType> event_list_cache_;///< Stores event ID lists per session
    };

}// namespace mcp::cache