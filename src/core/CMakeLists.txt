# src/core/CMakeLists.txt
file(GLOB CORE_SOURCES
    *.cpp
)
add_library(mcp_core ${CORE_SOURCES})

target_include_directories(mcp_core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mcp/core>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/spdlog/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/asio/include>
)

target_compile_features(mcp_core PUBLIC cxx_std_20)

if(WIN32)
    if(MSVC)
        target_compile_options(mcp_core PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(mcp_core PRIVATE -Wall -Wextra)
endif()

target_link_libraries(mcp_core PUBLIC 
    mcp_plugin_sdk
)

if(WIN32)
    list(APPEND CORE_LINKED_LIBRARIES mimalloc-static)
else()
    list(APPEND CORE_LINKED_LIBRARIES mimalloc)
endif()

#target_link_libraries(mcp_core PRIVATE ${CORE_LINKED_LIBRARIES})

# Conditionally install the library and headers
if(CPACK_INCLUDE_LIBS)
    install(TARGETS mcp_core
        EXPORT MCPServer++Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/mcp/core
        FILES_MATCHING PATTERN "*.h"
    )
endif()