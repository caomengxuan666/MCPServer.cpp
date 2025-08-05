# src/protocol/CMakeLists.txt
set(PROTOCOL_SOURCES
    json_rpc.cpp
    tool.h
)

add_library(mcp_protocol ${PROTOCOL_SOURCES})

target_include_directories(mcp_protocol PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mcp_protocol>
    ${PROJECT_SOURCE_DIR}/third_party
    ${PROJECT_SOURCE_DIR}/third_party/nlohmann
)

target_compile_features(mcp_protocol PUBLIC cxx_std_20)

if(WIN32)
    if(MSVC)
        target_compile_options(mcp_protocol PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(mcp_protocol PRIVATE -Wall -Wextra)
endif()

target_link_libraries(mcp_protocol PUBLIC 
    mcp_core
)