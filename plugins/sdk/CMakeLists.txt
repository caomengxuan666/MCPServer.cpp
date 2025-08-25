set(TARGET_NAME mcp_plugin_sdk)

# Source files
set(SDK_SOURCES
    tool_info_parser.cpp
    tool_info_parser.h
)

set(HEADERS
    mcp_plugin.h
    tool_info_parser.h
)

add_library(${TARGET_NAME} ${SDK_SOURCES} ${HEADERS})


target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/mcp/plugin_sdk>
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/nlohmann>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/spdlog/include>
)

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)

# Link required libraries
target_link_libraries(${TARGET_NAME} 
    PUBLIC
        mcp_core
        mcp_protocol
)

target_compile_definitions(${TARGET_NAME} PUBLIC MCPSERVER_API_EXPORTS)

# Conditionally install the library and headers
if(CPACK_INCLUDE_LIBS)
    install(TARGETS ${TARGET_NAME}
        EXPORT MCPServer++Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
    
    install(FILES ${HEADERS} 
        DESTINATION include/mcp/plugin_sdk
    )
endif()