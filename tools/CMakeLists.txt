add_executable(plugin_ctl plugin_ctl.cpp)
add_executable(generate_cert generate_cert.cpp)

target_include_directories(plugin_ctl PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(plugin_ctl PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(plugin_ctl PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_include_directories(plugin_ctl PRIVATE ${CMAKE_SOURCE_DIR}/third_party/spdlog/include)

target_link_libraries(plugin_ctl PRIVATE mcp_plugin_sdk mcp_core mcp_plugin_hub)

target_link_libraries(generate_cert PRIVATE MCP::OpenSSL mcp_metrics)
target_include_directories(generate_cert PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

# Install the tools - always install them regardless of CPACK_INCLUDE_LIBS setting
install(TARGETS plugin_ctl generate_cert
    RUNTIME DESTINATION bin
)

