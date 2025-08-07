# get all the .cc into a list
file(GLOB_RECURSE SOURCES "*.cc")

# for each .cpp file,create a target
foreach(source ${SOURCES})
    get_filename_component(name ${source} NAME_WE)
    add_executable(${name} ${source})
    target_link_libraries(${name} mcp_transport mcp_core)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src/transport)
endforeach()