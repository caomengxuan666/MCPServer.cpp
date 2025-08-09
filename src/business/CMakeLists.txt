# src/business/CMakeLists.txt
file(GLOB BUSINESS_SOURCES *.cpp)

add_library(mcp_business ${BUSINESS_SOURCES})

target_include_directories(mcp_business PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mcp/business>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/nlohmann>
)

target_compile_features(mcp_business PUBLIC cxx_std_20)

if(WIN32)
    if(MSVC)
        target_compile_options(mcp_business PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(mcp_business PRIVATE -Wall -Wextra)
endif()

target_link_libraries(mcp_business PUBLIC
    mcp_transport
)

# Conditionally install the library and headers
if(CPACK_INCLUDE_LIBS)
    install(TARGETS mcp_business
        EXPORT MCPServer++Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
        DESTINATION include/mcp/business
        FILES_MATCHING PATTERN "*.h"
    )
endif()