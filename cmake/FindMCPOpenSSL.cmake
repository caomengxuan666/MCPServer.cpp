# FindMCPOpenSSL.cmake - Cross-platform OpenSSL resolver with Windows fallback
#
# Behavior:
# 1. First tries system/vcpkg OpenSSL via find_package
# 2. On Windows only, falls back to project-local libraries if not found
# 3. Fails immediately if OpenSSL cannot be resolved
#
# Usage:
# include(FindMCPOpenSSL)
# target_link_libraries(your_target PRIVATE MCP::OpenSSL)

include_guard(GLOBAL)

# --- Phase 1: Primary detection via find_package ---
find_package(OpenSSL QUIET)

if(OPENSSL_FOUND)
    message(STATUS "Found system OpenSSL: ${OPENSSL_VERSION}")
    add_library(MCP::OpenSSL INTERFACE IMPORTED)
    target_link_libraries(MCP::OpenSSL INTERFACE OpenSSL::SSL OpenSSL::Crypto)
    return()
endif()

# --- Phase 2: Windows-specific fallback to local libraries ---
if(WIN32 AND NOT OPENSSL_FOUND)
    message(STATUS "Using project-local OpenSSL libraries")

    # Configure paths based on build type
    set(MCP_OPENSSL_ROOT "${CMAKE_SOURCE_DIR}/lib")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(OPENSSL_LIB_DIR "${MCP_OPENSSL_ROOT}/Debug")
    else()
        set(OPENSSL_LIB_DIR "${MCP_OPENSSL_ROOT}/Release")
    endif()

    # Verify files exist (both .lib and .dll)
    set(REQUIRED_LIBS
        "${OPENSSL_LIB_DIR}/libssl.lib"
        "${OPENSSL_LIB_DIR}/libcrypto.lib"
    )
    set(REQUIRED_DLLS
        "${OPENSSL_LIB_DIR}/libssl-3-x64.dll"
        "${OPENSSL_LIB_DIR}/libcrypto-3-x64.dll"
    )

    foreach(LIB ${REQUIRED_LIBS})
        if(NOT EXISTS ${LIB})
            message(FATAL_ERROR "Missing OpenSSL library: ${LIB}")
        endif()
    endforeach()

    foreach(DLL ${REQUIRED_DLLS})
        if(NOT EXISTS ${DLL})
            message(FATAL_ERROR "Missing OpenSSL DLL: ${DLL}")
        endif()
    endforeach()

    # Create imported target for static linking
    add_library(MCP::OpenSSL STATIC IMPORTED)
    set_target_properties(MCP::OpenSSL PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_LIB_DIR}/libssl.lib"
        INTERFACE_LINK_LIBRARIES "${OPENSSL_LIB_DIR}/libcrypto.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${MCP_OPENSSL_ROOT}/include"
    )

    # Copy DLLs to output directory
    add_custom_command(TARGET your_target POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${REQUIRED_DLLS}
        $<TARGET_FILE_DIR:your_target>
        COMMENT "Copying OpenSSL DLLs to output directory"
    )

    message(STATUS "Using OpenSSL libraries from: ${OPENSSL_LIB_DIR}")
    set(OPENSSL_FOUND TRUE)
    return()
endif()

# --- Phase 3: Non-Windows systems must have OpenSSL ---
if((UNIX OR APPLE) AND NOT OPENSSL_FOUND)
    message(FATAL_ERROR
        "OpenSSL is required but not found. Install with:\n"
        "  Linux: sudo apt install libssl-dev\n"
        "  macOS: brew install openssl"
    )
endif()

# --- Final fallthrough error ---
if(NOT OPENSSL_FOUND)
    message(FATAL_ERROR
        "OpenSSL setup failed. This is a workflow-critical dependency."
    )
endif()