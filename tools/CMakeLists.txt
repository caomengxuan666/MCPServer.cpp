add_executable(plugin_ctl plugin_ctl.cpp)
add_executable(generate_cert generate_cert.cpp)

target_link_libraries(generate_cert MCP::OpenSSL)

# Conditionally install the tools based on CPACK_INCLUDE_LIBS setting
if(CPACK_INCLUDE_LIBS)
    install(TARGETS plugin_ctl generate_cert
        RUNTIME DESTINATION bin
    )
endif()