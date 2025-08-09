# this cmake file is used to copy the ini config file to the cmake runtime output directory
function(copy_ini_config)
    file(COPY ${PROJECT_SOURCE_DIR}/config.ini DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    message(STATUS "Copied ini config file to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endfunction()