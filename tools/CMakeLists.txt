add_executable(plugin_ctl plugin_ctl.cpp)
add_executable(generate_cert generate_cert.cpp)

target_link_libraries(generate_cert MCP::OpenSSL)

# Install the tools - always install them regardless of CPACK_INCLUDE_LIBS setting
install(TARGETS plugin_ctl generate_cert
    RUNTIME DESTINATION bin
)