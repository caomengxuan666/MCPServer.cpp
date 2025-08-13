add_executable(plugin_ctl plugin_ctl.cpp)
add_executable(generate_cert generate_cert.cpp)

target_link_libraries(generate_cert MCP::OpenSSL)

# Install the tools - always install them regardless of CPACK_INCLUDE_LIBS setting
install(TARGETS plugin_ctl generate_cert
    RUNTIME DESTINATION bin
)

# After build copy the plugin_ctl executable to the plugin directory
add_custom_command(TARGET plugin_ctl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:plugin_ctl>
    ${PROJECT_SOURCE_DIR}/plugins/
)