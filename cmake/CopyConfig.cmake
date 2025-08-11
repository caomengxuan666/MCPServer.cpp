# this cmake file is used to copy the config file to the cmake runtime output directory

include_guard(GLOBAL)

# copy the ini config file to the cmake runtime output directory
function(copy_ini_config)
    # Check if config.ini exists in source directory
    if(EXISTS ${CMAKE_SOURCE_DIR}/config.ini)
        # Compare timestamps to see if we need to copy
        if(NOT EXISTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.ini OR 
           ${CMAKE_SOURCE_DIR}/config.ini IS_NEWER_THAN ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.ini)
            file(COPY ${CMAKE_SOURCE_DIR}/config.ini DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
            message(STATUS "Copied ini config file to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        else()
            message(STATUS "Ini config file is up to date")
        endif()
    else()
        message(WARNING "config.ini not found in source directory")
    endif()
endfunction()

# copy the certs's directory to the cmake runtime output directory/certs
function(copy_certs)
    # Check if certs directory exists in source directory
    if(EXISTS ${CMAKE_SOURCE_DIR}/certs)
        # Compare timestamps to see if we need to copy
        if(NOT EXISTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/certs OR 
           ${CMAKE_SOURCE_DIR}/certs IS_NEWER_THAN ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/certs)
            file(COPY ${CMAKE_SOURCE_DIR}/certs DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
            message(STATUS "Copied certs directory to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        else()
            message(STATUS "Certs directory is up to date")
        endif()
    else()
        message(WARNING "certs directory not found in source directory")
    endif()
endfunction()