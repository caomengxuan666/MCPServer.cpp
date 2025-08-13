cmake_minimum_required(VERSION 3.23)
project(MCPServer.cpp LANGUAGES CXX VERSION 1.0.3.9)

if(UNIX AND NOT APPLE)
    set(CMAKE_CXX_VISIBILITY_PRESET default)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/PluginCommon.cmake)
include(cmake/CopyConfig.cmake)
include(cmake/FindMCPOpenSSL.cmake)
copy_ini_config()
copy_certs()

set(PROJECT_VERSION_MAJOR ${${PROJECT_NAME}_VERSION_MAJOR} CACHE STRING "Major version number")
set(PROJECT_VERSION_MINOR ${${PROJECT_NAME}_VERSION_MINOR} CACHE STRING "Minor version number")
set(PROJECT_VERSION_PATCH ${${PROJECT_NAME}_VERSION_PATCH} CACHE STRING "Patch version number")
set(PROJECT_VERSION_TWEAK ${${PROJECT_NAME}_VERSION_TWEAK} CACHE STRING "Tweak version number")
set(PROJECT_VERSION_SUFFIX "" CACHE STRING "Version suffix (e.g., -beta, -rc1)")

set(PROJECT_FULL_VERSION
    "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
)

if(PROJECT_VERSION_TWEAK GREATER 0)
    set(PROJECT_FULL_VERSION "${PROJECT_FULL_VERSION}.${PROJECT_VERSION_TWEAK}")
endif()

if(PROJECT_VERSION_SUFFIX)
    set(PROJECT_FULL_VERSION "${PROJECT_FULL_VERSION}${PROJECT_VERSION_SUFFIX}")
endif()

string(TIMESTAMP CMAKE_BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S" UTC)

message(STATUS "Building ${PROJECT_NAME} version: ${PROJECT_FULL_VERSION}")

include_directories(${CMAKE_BINARY_DIR}/version)

add_subdirectory(version)

# main file
add_executable(mcp-server++ src/main.cpp)

add_compile_definitions(NOMINMAX)
add_compile_definitions(WIN32_LEAN_AND_MEAN)

include_directories(include)

# header directories
target_include_directories(mcp-server++ PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/asio/include)

if(WIN32)
    add_compile_definitions(_WIN32_WINDOWS)
endif()

# link mimalloc
# target_link_libraries(mcp-server++ PRIVATE ${LINKED_LIBRARIES})
if(MSVC)
    target_compile_options(mcp-server++ PRIVATE /W4 /permissive-)
    add_compile_options(/wd4100)
else()
    target_compile_options(mcp-server++ PRIVATE -Wall -Wextra -O3)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Add subdirectories
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(tools)

# turn off mimalloc tests and examples
set(MI_BUILD_TESTS OFF CACHE BOOL "Build mimalloc tests" FORCE)
set(MI_BUILD_EXAMPLES OFF CACHE BOOL "Build mimalloc examples" FORCE)

set(DISABLE_SYSTEM_MIMALLOC ON CACHE BOOL "Disable system mimalloc")

# Link all required libraries to the main executable
if(UNIX)
    target_link_libraries(mcp-server++ PRIVATE
        -Wl,--start-group
        mcp_business
        mcp_core
        mcp_transport
        mcp_protocol
        -Wl,--end-group
        mcp_plugin_sdk
    )
else()
    target_link_libraries(mcp-server++ PRIVATE
        mcp_business
        mcp_core
        mcp_transport
        mcp_protocol
        mcp_plugin_sdk
    )
endif()

# Install the main executable
install(TARGETS mcp-server++
    RUNTIME DESTINATION bin
)

# Install config file
install(FILES config.ini.example
    DESTINATION bin
    RENAME config.ini
)

# Install documentation
install(FILES
    README.md
    README_zh.md
    LICENSE
    DESTINATION share/doc/mcp-server++
)

# Install docs directory
install(DIRECTORY docs/
    DESTINATION share/doc/mcp-server++
    FILES_MATCHING
    PATTERN "*.md"
    PATTERN "*.png"
)

# Conditionally install headers for development (only when including libs)
if(CPACK_INCLUDE_LIBS)
    # Install headers for development
    install(DIRECTORY src/
        DESTINATION include/mcp
        FILES_MATCHING PATTERN "*.h"
    )

    # Install router headers
    install(DIRECTORY src/routers/
        DESTINATION include/mcp/routers
        FILES_MATCHING PATTERN "*.hpp"
    )

    # Create and install CMake config files
    include(CMakePackageConfigHelpers)

    # Generate the config file
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MCPConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/MCPServer++Config.cmake
        INSTALL_DESTINATION lib/cmake/MCPServer++
    )

    # Generate the version file
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/MCPServer++ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install the config files
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/MCPServer++Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/MCPServer++ConfigVersion.cmake
        DESTINATION lib/cmake/MCPServer++
    )

    # Export the targets
    install(EXPORT MCPServer++Targets
        FILE MCPServer++Targets.cmake
        NAMESPACE MCPServer++::
        DESTINATION lib/cmake/MCPServer++
    )
endif()

# CPack support for packaging
include(cmake/CPackConfig.cmake)

# testing
# Only build tests when including libs
if(CPACK_INCLUDE_LIBS)
    option(BUILD_TESTS "Build tests" ON)
else()
    option(BUILD_TESTS "Build tests" OFF)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(third_party/googletest)
    add_subdirectory(tests)
endif()

# example
# Only build examples when including libs
if(CPACK_INCLUDE_LIBS)
    option(BUILD_EXAMPLES "Build examples" ON)
else()
    option(BUILD_EXAMPLES "Build examples" OFF)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()