add_library(mcp_plugin_hub pluginhub.cpp)

target_include_directories(mcp_plugin_hub PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(mcp_plugin_hub PRIVATE ${CMAKE_SOURCE_DIR}/third_party/spdlog/include)
target_include_directories(mcp_plugin_hub PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_include_directories(mcp_plugin_hub PRIVATE ${CMAKE_SOURCE_DIR})