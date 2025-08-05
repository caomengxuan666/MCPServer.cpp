cmake_minimum_required(VERSION 3.23)
project(MCPserver++ LANGUAGES CXX VERSION 1.0.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/PluginCommon.cmake)

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


add_subdirectory(version)

# main file
add_executable(mcp-server++ src/main.cpp)

add_compile_definitions(NOMINMAX)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
# header directories
target_include_directories(mcp-server++ PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/asio/include)

if(WIN32)
    list(APPEND LINKED_LIBRARIES mimalloc-static)
else()
    list(APPEND LINKED_LIBRARIES mimalloc)
endif()

# link mimalloc
target_link_libraries(mcp-server++ PRIVATE ${LINKED_LIBRARIES})

if(MSVC)
    target_compile_options(mcp-server++ PRIVATE /W4 /permissive-)
else()
    target_compile_options(mcp-server++ PRIVATE -Wall -Wextra -O3)
endif()

target_compile_options(mcp-server++ PRIVATE
    /W4 /permissive-
    /utf-8
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Add subdirectories
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(tools)

# turn off mimalloc tests and examples
set(MI_BUILD_TESTS OFF CACHE BOOL "Build mimalloc tests" FORCE)
set(MI_BUILD_EXAMPLES OFF CACHE BOOL "Build mimalloc examples" FORCE)
add_subdirectory(third_party/mimalloc)

# Link all required libraries to the main executable
target_link_libraries(mcp-server++ PRIVATE 
    mcp_business
    mcp_transport
    mcp_protocol
    mcp_core
    mcp_plugin_sdk
)

# testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(third_party/googletest)
    add_subdirectory(tests)
endif()