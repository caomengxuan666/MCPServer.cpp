# src/protocol/CMakeLists.txt
set(PROTOCOL_SOURCES
    json_rpc.cpp
    tool.h
)

add_library(mcp_protocol ${PROTOCOL_SOURCES})

target_include_directories(mcp_protocol PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mcp/protocol>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/nlohmann>
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

# Conditionally install the library and headers
if(CPACK_INCLUDE_LIBS)
    install(TARGETS mcp_protocol
        EXPORT MCPServer++Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )

    install(FILES tool.h
        DESTINATION include/mcp/protocol
    )
endif()