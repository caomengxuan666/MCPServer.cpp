# CMakeLists.txt for Python Example Plugin
cmake_minimum_required(VERSION 3.23)
project(python_example_plugin)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the path to MCPServer++ root directory
set(MCP_SERVER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE STRING "Path to MCPServer++ root directory")

# Find required packages
find_package(Python COMPONENTS Interpreter Development REQUIRED)

# Add the plugin library
add_library(${PROJECT_NAME} SHARED
    # The pybind wrapper that exposes Python functions as C interface
    ${MCP_SERVER_ROOT}/plugins/sdk/pybind_module_plugin.cpp
)


# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # MCPServer++ include directories
    ${MCP_SERVER_ROOT}/plugins/sdk
    ${MCP_SERVER_ROOT}/include
    ${MCP_SERVER_ROOT}/third_party/nlohmann
    ${MCP_SERVER_ROOT}/third_party/pybind11/include
)

# Add preprocessor definition for DLL export
target_compile_definitions(${PROJECT_NAME} PRIVATE MCPSERVER_API_EXPORTS)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    pybind11::embed
)

# Ensure the Python plugin file is available
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/python_example_plugin.py 
               ${CMAKE_CURRENT_BINARY_DIR}/python_example_plugin.py 
               COPYONLY)

# Copy the Python file to the output directory after build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/python_example_plugin.py
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/python_example_plugin.py
)