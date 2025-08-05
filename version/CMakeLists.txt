configure_file(
    version.h.in 
    ${CMAKE_BINARY_DIR}/version/version.h 
    @ONLY  
)


install(
    FILES ${CMAKE_BINARY_DIR}/version/version.h 
    DESTINATION include/version
)
