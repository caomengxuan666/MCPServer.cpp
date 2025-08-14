add_library(mcp_utils STATIC auth_utils.cpp)
target_include_directories(mcp_utils PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include/mcp/transport>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/asio/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/asio/standalone/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/spdlog/include>
)
