# FindMCPOpenSSL.cmake - Cross-platform OpenSSL resolver with Windows fallback
#
# Behavior:
# 1. On UNIX (Linux/macOS): Try to use system OpenSSL (version 3+ preferred)
# 2. On Windows: Fallback to local prebuilt libraries in project's /lib folder
# 3. Always copies required OpenSSL DLLs to the global runtime output directory
# 4. Ensures DLLs are included in CPack-generated installers
#
# Usage:
# include(FindMCPOpenSSL)
# target_link_libraries(your_target PRIVATE MCP::OpenSSL)
#
# Note: OpenSSL DLLs are automatically copied to CMAKE_RUNTIME_OUTPUT_DIRECTORY
#       regardless of which target is built. Suitable for install() and CPack.

include_guard(GLOBAL)

# --- Phase 1: Use system OpenSSL on UNIX systems ---
if(UNIX)
    find_package(OpenSSL QUIET)
    if(OPENSSL_FOUND)
        message(STATUS "Found system OpenSSL: ${OPENSSL_VERSION}")
        add_library(MCP::OpenSSL INTERFACE IMPORTED)
        target_link_libraries(MCP::OpenSSL INTERFACE OpenSSL::SSL OpenSSL::Crypto)
        return()
    endif()
endif()

# --- Phase 2: Windows-specific fallback to local libraries ---
if(WIN32)
    message(STATUS "Using project-local OpenSSL libraries")

    # Set root directory for local OpenSSL
    set(MCP_OPENSSL_ROOT "${CMAKE_SOURCE_DIR}/lib" CACHE PATH "Root directory of local OpenSSL libraries")

    # Choose library directory based on build type
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(OPENSSL_LIB_DIR "${MCP_OPENSSL_ROOT}/Debug")
    else()
        set(OPENSSL_LIB_DIR "${MCP_OPENSSL_ROOT}/Release")
    endif()

    # Define required static import libraries (.lib)
    set(REQUIRED_LIBS
        "${OPENSSL_LIB_DIR}/libssl.lib"
        "${OPENSSL_LIB_DIR}/libcrypto.lib"
    )

    # Define required runtime DLLs (.dll)
    set(REQUIRED_DLLS
        "${OPENSSL_LIB_DIR}/libssl-3-x64.dll"
        "${OPENSSL_LIB_DIR}/libcrypto-3-x64.dll"
    )

    # Verify all required .lib files exist
    foreach(LIB ${REQUIRED_LIBS})
        if(NOT EXISTS ${LIB})
            message(FATAL_ERROR "Missing OpenSSL import library: ${LIB}")
        endif()
    endforeach()

    # Verify all required .dll files exist
    foreach(DLL ${REQUIRED_DLLS})
        if(NOT EXISTS ${DLL})
            message(FATAL_ERROR "Missing OpenSSL runtime DLL: ${DLL}")
        endif()
    endforeach()

    # Create imported target for linking
    if(NOT TARGET MCP::OpenSSL)
        add_library(MCP::OpenSSL STATIC IMPORTED)
        set_target_properties(MCP::OpenSSL PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_LIB_DIR}/libssl.lib"
            INTERFACE_LINK_LIBRARIES "${OPENSSL_LIB_DIR}/libcrypto.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${MCP_OPENSSL_ROOT}/include"
        )
    endif()

    # Ensure runtime output directory is set
    if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE PATH "Single output directory for all executables")
    endif()

    # Make sure the output directory exists
    file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

    # Copy OpenSSL DLLs to the global runtime output directory
    # This runs independently of any specific target
    add_custom_command(
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libssl-3-x64.dll
               ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcrypto-3-x64.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${REQUIRED_DLLS}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMENT "Copying OpenSSL DLLs to runtime directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        VERBATIM
    )

    # Create a dummy target that depends on the DLLs and runs every time
    add_custom_target(CopyOpenSSLDLLs ALL
        DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libssl-3-x64.dll
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcrypto-3-x64.dll
    )

    # Export DLLs for installation and packaging (e.g., CPack)
    install(FILES ${REQUIRED_DLLS}
            DESTINATION bin
            COMPONENT Runtime
            CONFIGURATIONS Release Debug)

    message(STATUS "Configured local OpenSSL from: ${OPENSSL_LIB_DIR}")
    set(OPENSSL_FOUND TRUE)
    return()
endif()

# --- Phase 3: Fail if OpenSSL is missing on UNIX-like systems ---
if((UNIX OR APPLE) AND NOT OPENSSL_FOUND)
    message(FATAL_ERROR
        "OpenSSL is required but not found. Install with:\n"
        "  Linux: sudo apt install libssl-dev\n"
        "  macOS: brew install openssl"
    )
endif()

# --- Final fallback: Report failure if OpenSSL setup did not succeed ---
if(NOT OPENSSL_FOUND)
    message(FATAL_ERROR
        "OpenSSL setup failed. This is a workflow-critical dependency."
    )
endif()