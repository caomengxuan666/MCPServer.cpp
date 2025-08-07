#include "core/logger.h"
#include "nlohmann/json.hpp"
#include "transport/mcp_cache.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace mcp::cache;
using json = nlohmann::json;

class McpCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = McpCache::GetInstance();
        // Initialize with small values for testing
        cache->Init(10, 20, std::chrono::seconds(3600));
    }

    void TearDown() override { cache->CleanupExpiredData(); }

    McpCache *cache;

    // Simulate generating stream data (with event_id incrementing)
    json generate_stream_data(int event_id) {
        return json{
                {"jsonrpc", "2.0"},
                {"result", {{"event_id", event_id}, {"data", "stream_content_" + std::to_string(event_id)}, {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}}}};
    }
};

// Test basic session state save and restore
TEST_F(McpCacheTest, SessionStateSaveRestore) {
    // Prepare test session state
    SessionState initial_state;
    initial_state.session_id = "test_session_1";
    initial_state.tool_name = "test_tool";
    initial_state.last_event_id = 5;
    initial_state.is_active = true;
    initial_state.last_update = std::chrono::system_clock::now();

    // Save session state
    ASSERT_TRUE(cache->SaveSessionState(initial_state));

    // Retrieve session state
    auto restored_state = cache->GetSessionState("test_session_1");
    ASSERT_TRUE(restored_state.has_value());

    // Verify all fields
    EXPECT_EQ(restored_state->session_id, "test_session_1");
    EXPECT_EQ(restored_state->tool_name, "test_tool");
    EXPECT_EQ(restored_state->last_event_id, 5);
    EXPECT_TRUE(restored_state->is_active);
}

// Test streaming data caching and reconnection recovery
TEST_F(McpCacheTest, StreamDataCacheAndRecovery) {
    const std::string session_id = "stream_session_1";

    // Simulate session state
    SessionState initial_state;
    initial_state.session_id = session_id;
    initial_state.tool_name = "stream_tool";
    initial_state.last_event_id = 0;
    initial_state.is_active = true;
    initial_state.last_update = std::chrono::system_clock::now();
    ASSERT_TRUE(cache->SaveSessionState(initial_state));

    // Simulate sending 5 data items
    for (int i = 1; i <= 5; ++i) {
        auto data = generate_stream_data(i);
        ASSERT_TRUE(cache->CacheStreamData(session_id, i, data));

        // Update session state (event_id increment)
        ASSERT_TRUE(cache->UpdateSessionState(session_id, i));
    }

    // Verify session state update
    auto state = cache->GetSessionState(session_id);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->last_event_id, 5);

    // Simulate reconnection (assume client last received event_id is 2)
    auto reconnect_data = cache->GetReconnectData(session_id, 2);
    ASSERT_EQ(reconnect_data.size(), 3);// Should get 3 items (3, 4, 5)

    // Verify reconnection data content
    for (size_t i = 0; i < reconnect_data.size(); ++i) {
        int event_id = reconnect_data[i]["result"]["event_id"].get<int>();
        EXPECT_EQ(event_id, static_cast<int>(i) + 3);// Should be 3, 4, 5

        std::string data_content = reconnect_data[i]["result"]["data"];
        EXPECT_EQ(data_content, "stream_content_" + std::to_string(event_id));
    }

    // Verify session state after reconnection
    auto restored_state = cache->GetSessionState(session_id);
    ASSERT_TRUE(restored_state.has_value());
    EXPECT_EQ(restored_state->last_event_id, 5);
}

// Test reconnection data filtering
TEST_F(McpCacheTest, ReconnectDataFiltering) {
    const std::string session_id = "filter_session";

    // Initialize session
    SessionState state;
    state.session_id = session_id;
    state.tool_name = "filter_tool";
    state.last_event_id = 0;
    state.is_active = true;
    state.last_update = std::chrono::system_clock::now();
    ASSERT_TRUE(cache->SaveSessionState(state));

    // Cache data with non-sequential event IDs
    std::vector<int> event_numbers = {1, 3, 5, 7, 9};
    for (int event_id: event_numbers) {
        json data = {{"event_id", event_id}, {"content", "data_" + std::to_string(event_id)}};
        ASSERT_TRUE(cache->CacheStreamData(session_id, event_id, data));
    }

    // Request reconnection data with last_event_id = 5
    auto reconnect_data = cache->GetReconnectData(session_id, 5);

    // Should only get events 7 and 9
    ASSERT_EQ(reconnect_data.size(), 2);
    EXPECT_EQ(reconnect_data[0]["event_id"].get<int>(), 7);
    EXPECT_EQ(reconnect_data[1]["event_id"].get<int>(), 9);
}

// Test session cleanup
TEST_F(McpCacheTest, SessionCleanup) {
    const std::string session_id = "cleanup_session";

    // Initialize session
    SessionState state;
    state.session_id = session_id;
    state.tool_name = "cleanup_tool";
    state.last_event_id = 0;
    state.is_active = true;
    state.last_update = std::chrono::system_clock::now();
    ASSERT_TRUE(cache->SaveSessionState(state));

    // Cache some data
    json data = {{"event_id", 1}, {"content", "test_data"}};
    ASSERT_TRUE(cache->CacheStreamData(session_id, 1, data));

    // Verify data exists
    ASSERT_TRUE(cache->GetSessionState(session_id).has_value());

    // Cleanup session
    ASSERT_TRUE(cache->CleanupSession(session_id));

    // Verify data is gone
    EXPECT_FALSE(cache->GetSessionState(session_id).has_value());
    auto reconnect_data = cache->GetReconnectData(session_id, 0);
    EXPECT_TRUE(reconnect_data.empty());
}

// Test data expiration
TEST_F(McpCacheTest, DataExpiration) {
    const std::string session_id = "expire_session";

    // Initialize with short TTL
    cache->Init(10, 20, std::chrono::seconds(1));

    SessionState state;
    state.session_id = session_id;
    state.tool_name = "expire_tool";
    state.last_event_id = 0;
    state.is_active = true;
    state.last_update = std::chrono::system_clock::now();
    ASSERT_TRUE(cache->SaveSessionState(state));

    // Cache data
    json data = {{"event_id", 1}, {"content", "expire_data"}};
    ASSERT_TRUE(cache->CacheStreamData(session_id, 1, data));

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Force cleanup
    cache->CleanupExpiredData();

    // Verify data is gone
    auto reconnect_data = cache->GetReconnectData(session_id, 0);
    EXPECT_TRUE(reconnect_data.empty());
}

// Test basic session management and streaming data caching
TEST_F(McpCacheTest, BasicStreaming) {
    // Simulate new session creation
    std::string session_id = "session_001";

    // Save initial session state
    mcp::cache::SessionState initial_state;
    initial_state.session_id = session_id;
    initial_state.tool_name = "example_stream_tool";
    initial_state.last_event_id = 0;
    initial_state.last_update = std::chrono::system_clock::now();

    EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->SaveSessionState(initial_state));

    // Simulate real-time data sending and caching (send 5 data items)
    for (int i = 1; i <= 5; ++i) {
        auto data = generate_stream_data(i);
        // Cache data
        EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->CacheStreamData(session_id, i, data));
        // Update session state (event_id increment)
        EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->UpdateSessionState(session_id, i));
    }

    // Simulate reconnection (assume client last received event_id is 2)
    int last_received_event_id = 2;

    // Get unreceived data after disconnection (should return event_id 3-5)
    auto reconnect_data = mcp::cache::McpCache::GetInstance()->GetReconnectData(session_id, last_received_event_id);
    EXPECT_EQ(reconnect_data.size(), 3);// Should have 3 items (event_id 3, 4, 5)

    for (size_t i = 0; i < reconnect_data.size(); ++i) {
        int event_id = reconnect_data[i]["result"]["event_id"].get<int>();
        std::string content = reconnect_data[i]["result"]["data"].get<std::string>();
        EXPECT_EQ(event_id, static_cast<int>(i + 3));// Should be 3, 4, 5
        EXPECT_EQ(content, "stream_content_" + std::to_string(i + 3));
    }

    // Verify session state is correctly restored
    auto restored_state = mcp::cache::McpCache::GetInstance()->GetSessionState(session_id);
    EXPECT_TRUE(restored_state.has_value());
    if (restored_state.has_value()) {
        EXPECT_EQ(restored_state->last_event_id, 5);
    }

    // Simulate session end, clean up cache
    EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->CleanupSession(session_id));

    // Verify cleanup results
    auto after_cleanup_state = mcp::cache::McpCache::GetInstance()->GetSessionState(session_id);
    EXPECT_FALSE(after_cleanup_state.has_value());
}

// Test multiple sessions with different tools
TEST_F(McpCacheTest, MultipleSessions) {
    // Create multiple sessions with different tools
    std::vector<std::string> session_ids = {"session_A", "session_B", "session_C"};
    std::vector<std::string> tool_names = {"tool_calculator", "tool_search", "tool_translator"};

    for (size_t i = 0; i < session_ids.size(); ++i) {
        std::string session_id = session_ids[i];
        std::string tool_name = tool_names[i];

        // Save session state
        mcp::cache::SessionState state;
        state.session_id = session_id;
        state.tool_name = tool_name;
        state.last_event_id = 0;
        state.last_update = std::chrono::system_clock::now();
        EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->SaveSessionState(state));

        // Generate and cache some data for each session
        for (int j = 1; j <= 3; ++j) {
            auto data = generate_stream_data(j);
            EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->CacheStreamData(session_id, j, data));
            EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->UpdateSessionState(session_id, j));
        }
    }

    // Check data for each session
    for (size_t i = 0; i < session_ids.size(); ++i) {
        const std::string &session_id = session_ids[i];
        const std::string &tool_name = tool_names[i];

        auto state = mcp::cache::McpCache::GetInstance()->GetSessionState(session_id);
        EXPECT_TRUE(state.has_value());
        if (state.has_value()) {
            EXPECT_EQ(state->tool_name, tool_name);
            EXPECT_EQ(state->last_event_id, 3);
        }
    }

    // Clean up all sessions
    for (const auto &session_id: session_ids) {
        EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->CleanupSession(session_id));
    }
}

// Test cache expiration
TEST_F(McpCacheTest, CacheExpiration) {
    // Create a new cache instance for this test
    auto cache = mcp::cache::McpCache::GetInstance();
    // Reinitialize cache with short expiration time (2 seconds)
    cache->Init(100, 50, std::chrono::seconds(2));

    std::string session_id = "expiring_session";

    // Save session state with a 2-second TTL
    mcp::cache::SessionState state;
    state.session_id = session_id;
    state.tool_name = "expiring_tool";
    state.last_event_id = 0;
    state.last_update = std::chrono::system_clock::now();
    EXPECT_TRUE(cache->SaveSessionState(state));

    // Add some data
    auto data = generate_stream_data(1);
    EXPECT_TRUE(cache->CacheStreamData(session_id, 1, data));
    EXPECT_TRUE(cache->UpdateSessionState(session_id, 1));

    // Verify initial state
    auto state_before = cache->GetSessionState(session_id);
    EXPECT_TRUE(state_before.has_value()) << "Session should exist before expiration";

    // Verify data exists before expiration
    auto initial_data = cache->GetReconnectData(session_id, 0);
    EXPECT_EQ(initial_data.size(), 1) << "Should have 1 data item before expiration";

    // Wait for cache to expire (3 seconds > 2 seconds TTL)
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Manually trigger cleanup since automatic cleanup happens in the background
    cache->CleanupExpiredData();

    // Try to get expired session - should not exist anymore
    auto expired_state = cache->GetSessionState(session_id);
    EXPECT_FALSE(expired_state.has_value()) << "Session should have expired";

    // Check expired data - should be gone
    auto expired_data = cache->GetReconnectData(session_id, 0);
    EXPECT_TRUE(expired_data.empty()) << "Data should have expired";

    // Force cleanup again and check
    cache->CleanupExpiredData();
    auto state_after_cleanup = cache->GetSessionState(session_id);
    EXPECT_FALSE(state_after_cleanup.has_value()) << "Session should still be expired after forced cleanup";

    // Verify no data remains
    auto final_data = cache->GetReconnectData(session_id, 0);
    EXPECT_TRUE(final_data.empty()) << "No data should remain after cleanup";
}

// Test GetReconnectData with various scenarios
TEST_F(McpCacheTest, ReconnectDataRetrieval) {
    std::string session_id = "reconnect_test_session";

    // Save initial session state
    mcp::cache::SessionState initial_state;
    initial_state.session_id = session_id;
    initial_state.tool_name = "reconnect_test_tool";
    initial_state.last_event_id = 0;
    initial_state.last_update = std::chrono::system_clock::now();
    EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->SaveSessionState(initial_state));

    // Cache stream data with non-sequential event IDs
    std::vector<int> event_ids = {1, 3, 5, 7, 9};
    for (int event_id: event_ids) {
        auto data = generate_stream_data(event_id);
        EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->CacheStreamData(session_id, event_id, data));
        EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->UpdateSessionState(session_id, event_id));
    }

    // Test getting data after event_id 5 - should return event_id 7 and 9
    auto reconnect_data = mcp::cache::McpCache::GetInstance()->GetReconnectData(session_id, 5);
    EXPECT_EQ(reconnect_data.size(), 2);

    if (reconnect_data.size() >= 2) {
        EXPECT_EQ(reconnect_data[0]["result"]["event_id"].get<int>(), 7);
        EXPECT_EQ(reconnect_data[1]["result"]["event_id"].get<int>(), 9);
    }

    // Test getting data after event_id 9 - should return empty
    reconnect_data = mcp::cache::McpCache::GetInstance()->GetReconnectData(session_id, 9);
    EXPECT_EQ(reconnect_data.size(), 0);

    // Test getting data after event_id 0 - should return all
    reconnect_data = mcp::cache::McpCache::GetInstance()->GetReconnectData(session_id, 0);
    EXPECT_EQ(reconnect_data.size(), 5);
}

// Test edge cases
TEST_F(McpCacheTest, EdgeCases) {
    // Test with empty session ID
    std::string empty_session_id = "";

    mcp::cache::SessionState state;
    state.session_id = empty_session_id;
    state.tool_name = "empty_session_tool";
    state.last_event_id = 0;
    state.last_update = std::chrono::system_clock::now();

    EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->SaveSessionState(state));

    auto retrieved_state = mcp::cache::McpCache::GetInstance()->GetSessionState(empty_session_id);
    EXPECT_TRUE(retrieved_state.has_value());
    if (retrieved_state.has_value()) {
        EXPECT_EQ(retrieved_state->tool_name, "empty_session_tool");
    }

    EXPECT_TRUE(mcp::cache::McpCache::GetInstance()->CleanupSession(empty_session_id));
}