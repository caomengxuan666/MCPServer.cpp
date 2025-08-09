add_executable(plugin_ctl plugin_ctl.cpp)

# Conditionally install the tool based on CPACK_INCLUDE_LIBS setting
if(CPACK_INCLUDE_LIBS)
    install(TARGETS plugin_ctl
        RUNTIME DESTINATION bin
    )
endif()