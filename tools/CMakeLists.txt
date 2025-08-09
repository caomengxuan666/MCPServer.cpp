add_executable(plugin_ctl plugin_ctl.cpp)

install(TARGETS plugin_ctl
    RUNTIME DESTINATION bin
)