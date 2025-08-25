# CMakeLists.txt for Python Example Plugin
cmake_minimum_required(VERSION 3.23)

# Use the common function to configure the Python plugin
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/PluginCommon.cmake)

configure_python_plugin(python_example_plugin ${CMAKE_CURRENT_SOURCE_DIR}/python_example_plugin.py)