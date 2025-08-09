# FindMCPOpenSSL.cmake - Cross-platform OpenSSL resolver with Windows fallback
#
# Behavior:
# 1. On UNIX: Use system OpenSSL (version 3+ preferred)
# 2. On Windows: Fallback to project-local prebuilt libraries
# 3. Does NOT require lib/include to exist
# 4. Copies DLLs to CMAKE_RUNTIME_OUTPUT_DIRECTORY (e.g., bin/)
#
# Usage:
# include(FindMCPOpenSSL)
# target_link_libraries(your_target PRIVATE MCP::OpenSSL)
# Optionally: target_include_directories(your_target PRIVATE path/to/openssl/headers)

include_guard(GLOBAL)

# --- Phase 1: Use system OpenSSL on UNIX ---
if(UNIX)
    find_package(OpenSSL QUIET)
    if(OPENSSL_FOUND)
        message(STATUS "Found system OpenSSL: ${OPENSSL_VERSION}")
        add_library(MCP::OpenSSL INTERFACE IMPORTED)
        target_link_libraries(MCP::OpenSSL INTERFACE OpenSSL::SSL OpenSSL::Crypto)
        return()
    endif()
endif()

# --- Phase 2: Windows fallback ---
if(WIN32)
    message(STATUS "Using project-local OpenSSL libraries")

    set(MCP_OPENSSL_ROOT "${CMAKE_SOURCE_DIR}/lib")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(OPENSSL_LIB_DIR "${MCP_OPENSSL_ROOT}/Debug")
    else()
        set(OPENSSL_LIB_DIR "${MCP_OPENSSL_ROOT}/Release")
    endif()

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

    # Create imported target WITHOUT INTERFACE_INCLUDE_DIRECTORIES
    add_library(MCP::OpenSSL STATIC IMPORTED)
    set_target_properties(MCP::OpenSSL PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_LIB_DIR}/libssl.lib"
        INTERFACE_LINK_LIBRARIES "${OPENSSL_LIB_DIR}/libcrypto.lib"
        # Do NOT set INTERFACE_INCLUDE_DIRECTORIES here
    )

    # Copy DLLs to global runtime output directory (e.g., build/bin)
    if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endif()

    add_custom_command(
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libssl-3-x64.dll
               ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcrypto-3-x64.dll
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${REQUIRED_DLLS}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMENT "Copying OpenSSL DLLs to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        VERBATIM
    )

    add_custom_target(CopyOpenSSLDLLs ALL
        DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libssl-3-x64.dll
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcrypto-3-x64.dll
    )

    # Optional: Install DLLs for CPack
    install(FILES ${REQUIRED_DLLS} DESTINATION bin COMPONENT Runtime)

    message(STATUS "Using OpenSSL libraries from: ${OPENSSL_LIB_DIR}")
    set(OPENSSL_FOUND TRUE)
    return()
endif()

# --- Phase 3: Fail on UNIX if not found ---
if((UNIX OR APPLE) AND NOT OPENSSL_FOUND)
    message(FATAL_ERROR
        "OpenSSL is required but not found. Install with:\n"
        "  Linux: sudo apt install libssl-dev\n"
        "  macOS: brew install openssl"
    )
endif()

# --- Final error ---
if(NOT OPENSSL_FOUND)
    message(FATAL_ERROR "OpenSSL setup failed. This is a workflow-critical dependency.")
endif()