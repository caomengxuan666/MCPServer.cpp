# plugins/sdk/CMakeLists.txt
set(SDK_SOURCES
    tool_info_parser.cpp
    tool_info_parser.h
)

add_library(mcp_plugin_sdk ${SDK_SOURCES})

target_include_directories(mcp_plugin_sdk PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mcp_plugin_sdk>
    ${PROJECT_SOURCE_DIR}/third_party
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/third_party/nlohmann
    ${PROJECT_SOURCE_DIR}/third_party/spdlog/include
)

target_compile_features(mcp_plugin_sdk PUBLIC cxx_std_20)

if(WIN32)
    if(MSVC)
        target_compile_options(mcp_plugin_sdk PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(mcp_plugin_sdk PRIVATE -Wall -Wextra)
endif()

# Link required libraries for the SDK
target_link_libraries(mcp_plugin_sdk PUBLIC
    mcp_core
)