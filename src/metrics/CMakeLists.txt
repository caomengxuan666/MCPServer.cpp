set(METRICS_SOURCES
    metrics_manager.cpp
    rate_limiter.cpp
)

set(METRICS_HEADERS
    metrics_manager.h
    performance_metrics.h
    rate_limiter.h
)

add_library(mcp_metrics STATIC ${METRICS_SOURCES} ${METRICS_HEADERS})
target_include_directories(mcp_metrics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(mcp_metrics PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(mcp_metrics PUBLIC ${CMAKE_SOURCE_DIR}/third_party/spdlog/include)