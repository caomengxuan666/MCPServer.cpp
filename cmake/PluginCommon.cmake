if(MSVC)
add_compile_options(/utf-8)
endif()
# Define the plugins output directory
set(PLUGINS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
set(CONFIGS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/configs")

# Common function to configure a plugin
# args:
#   plugin_name
#   src_files
#   extra_include_dirs
#   extra_libs
function(configure_plugin plugin_name src_files)
    set(extra_args ${ARGN})
    list(LENGTH extra_args extra_args_count)
    
    set(extra_include_dirs "")
    set(extra_libs "")
    
    if(extra_args_count GREATER 0)
        list(GET extra_args 0 extra_include_dirs)
    endif()
    
    if(extra_args_count GREATER 1)
        list(GET extra_args 1 extra_libs)
    endif()
    
    add_library(${plugin_name} SHARED ${src_files})
    target_compile_definitions(${plugin_name} PRIVATE MCPSERVER_API_EXPORTS)
    
    set(base_include_dirs
        ${PROJECT_SOURCE_DIR}/third_party
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/third_party/nlohmann
        ${PROJECT_SOURCE_DIR}/third_party/spdlog/include
    )
    
    if(EXISTS ${PROJECT_SOURCE_DIR}/plugins/sdk)
        list(APPEND base_include_dirs ${PROJECT_SOURCE_DIR}/plugins/sdk)
    endif()
    
    if(extra_include_dirs)
        list(APPEND base_include_dirs ${extra_include_dirs})
    endif()
    
    target_include_directories(${plugin_name} PRIVATE ${base_include_dirs})
    
    target_link_libraries(${plugin_name} mcp_plugin_sdk)
    
    if(extra_libs)
        target_link_libraries(${plugin_name} ${extra_libs})
    endif()
    
    # Set output directory for plugins to bin/plugins
    set_target_properties(${plugin_name} PROPERTIES 
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${PLUGINS_OUTPUT_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${PLUGINS_OUTPUT_DIR}
    )
    
    if(WIN32)
        set_target_properties(${plugin_name} PROPERTIES SUFFIX ".dll")
    endif()
    
    install(TARGETS ${plugin_name}
        RUNTIME DESTINATION bin/plugins
        LIBRARY DESTINATION lib/plugins
    )
    
    # automatically generate tools.json
    set(json_source_file "${CMAKE_CURRENT_SOURCE_DIR}/tools.json")
    set(json_target_file "${plugin_name}_tools.json")
    
    if(EXISTS ${json_source_file})
        # Create configs directory if not exists
        file(MAKE_DIRECTORY ${CONFIGS_OUTPUT_DIR})
        
        configure_file(
            ${json_source_file}
            ${CONFIGS_OUTPUT_DIR}/${json_target_file}
            COPYONLY
        )
    
        # build when the plugin is built
        add_custom_command(
            OUTPUT ${CONFIGS_OUTPUT_DIR}/${json_target_file}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${json_source_file}
            ${CONFIGS_OUTPUT_DIR}/${json_target_file}
            DEPENDS ${json_source_file}
            COMMENT "Copying ${plugin_name} tools.json file to configs directory"
        )
        
        add_custom_target(${plugin_name}_json ALL
            DEPENDS ${CONFIGS_OUTPUT_DIR}/${json_target_file}
        )
        
        add_dependencies(${plugin_name} ${plugin_name}_json)
        
        
        install(FILES ${json_source_file}
            DESTINATION bin/configs
            RENAME ${json_target_file}
        )
    endif()
endfunction()