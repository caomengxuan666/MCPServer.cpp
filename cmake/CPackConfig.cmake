# CPack configuration for MCPServer++
# This file defines the settings for creating packages for multiple platforms

# Option to control whether to include library files in the package
option(CPACK_INCLUDE_LIBS "Include library files in the package" ON)

# General CPack settings
set(CPACK_PACKAGE_NAME "mcp-server++")
set(CPACK_PACKAGE_VENDOR "MCPServer++")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A C++ implementation of the Model Context Protocol server")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_FULL_VERSION})

set(CPACK_PACKAGE_CONTACT "MCPServer++ Team")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/caomengxuan666/MCPServer.cpp")

# Package file name - differentiate between full and no-lib versions
# Include distribution information for Linux platforms
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Get distribution information
    find_program(LSB_RELEASE_EXEC lsb_release)

    if(LSB_RELEASE_EXEC)
        execute_process(COMMAND ${LSB_RELEASE_EXEC} -cs
            OUTPUT_VARIABLE LSB_RELEASE_CODENAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    if(LSB_RELEASE_CODENAME)
        set(DIST_SUFFIX "-${LSB_RELEASE_CODENAME}")
    else()
        set(DIST_SUFFIX "-${CMAKE_SYSTEM_NAME}")
    endif()
else()
    set(DIST_SUFFIX "-${CMAKE_SYSTEM_NAME}")
endif()

if(CPACK_INCLUDE_LIBS)
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${DIST_SUFFIX}-${CMAKE_SYSTEM_PROCESSOR}")
else()
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}${DIST_SUFFIX}-${CMAKE_SYSTEM_PROCESSOR}-no-libs")
endif()

# Source package settings
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-Source")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    /.git
    /.github
    /.vs
    /.vscode
    /build.*
    /cmake-build.*
    /out
    /\\.cache
    /\\.idea
    /\\.lingma
    /logs
    /mcp-server.log
    /.*\\.user
    /.*\\.sln
    /.*\\.sdf
    /.*\\.opensdf
    /.*\\.vcxproj.*
    CMakeLists.txt.user
    cmake_install.cmake
    CMakeCache.txt
    Makefile
    /_CPack_Packages
    /certs
)

# Platform-specific settings
if(WIN32)
    # Windows settings
    # Always include TGZ and ZIP
    set(CPACK_GENERATOR "TGZ;ZIP")

    # Check if NSIS is available before enabling it
    # Try multiple common locations for NSIS
    find_program(NSIS_PROGRAM
        NAMES makensis
        PATHS
        "C:/Program Files (x86)/NSIS"
        "C:/Program Files/NSIS"
        "$ENV{ProgramFiles}/NSIS"
        "$ENV{ProgramFiles\(x86\)}/NSIS"
        PATH_SUFFIXES Bin
    )

    # If not found in specific paths, try general system path
    if(NOT NSIS_PROGRAM)
        find_program(NSIS_PROGRAM makensis)
    endif()

    if(NSIS_PROGRAM)
        message(STATUS "Found NSIS: ${NSIS_PROGRAM}")
        list(APPEND CPACK_GENERATOR "NSIS")

        # NSIS settings
        set(CPACK_NSIS_DISPLAY_NAME "MCPServer++")
        set(CPACK_NSIS_PACKAGE_NAME "MCPServer++")
        set(CPACK_NSIS_CONTACT ${CPACK_PACKAGE_CONTACT})
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")

        # Add start menu links
        set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\mcp-server++.exe")
        set(CPACK_NSIS_HELP_LINK "https://github.com/your-repo/MCPServerPlusPlus")
        set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/your-repo/MCPServerPlusPlus")
    else()
        message(STATUS "NSIS not found, only creating TGZ and ZIP packages")
    endif()

elseif(UNIX)
    # Linux/Unix settings
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

    # Default generators for Unix (always include TGZ and ZIP)
    set(CPACK_GENERATOR "TGZ;ZIP")

    # Determine Linux distribution for specific packaging
    if(EXISTS "/etc/os-release")
        file(READ "/etc/os-release" OS_RELEASE_CONTENT)

        # Extract ID from /etc/os-release
        if(OS_RELEASE_CONTENT MATCHES "ID=([a-z]*)")
            set(LINUX_ID ${CMAKE_MATCH_1})
        endif()
        
        # Also check for Ubuntu specifically (as it may not always match the ID pattern correctly)
        if(OS_RELEASE_CONTENT MATCHES "ID_LIKE=([a-z]*)")
            set(LINUX_ID_LIKE ${CMAKE_MATCH_1})
        endif()

        # Check for Ubuntu/Debian
        if(LINUX_ID STREQUAL "ubuntu" OR LINUX_ID STREQUAL "debian" OR 
           LINUX_ID_LIKE STREQUAL "ubuntu" OR LINUX_ID_LIKE STREQUAL "debian" OR
           OS_RELEASE_CONTENT MATCHES "Ubuntu" OR OS_RELEASE_CONTENT MATCHES "Debian")
            find_program(DPKG_PROGRAM dpkg)

            if(DPKG_PROGRAM)
                message(STATUS "Found dpkg: ${DPKG_PROGRAM}, enabling DEB package generation")
                list(APPEND CPACK_GENERATOR "DEB")
                set(CPACK_DEBIAN_PACKAGE_MAINTAINER ${CPACK_PACKAGE_CONTACT})
                set(CPACK_DEBIAN_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION_SUMMARY})
                set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
                set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
                set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${CPACK_PACKAGE_HOMEPAGE_URL})
                set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
                set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
            else()
                message(STATUS "dpkg not found, DEB package generation disabled")
            endif()
        endif()
        
        # Check for CentOS/RHEL/Fedora
        if(LINUX_ID STREQUAL "centos" OR LINUX_ID STREQUAL "rhel" OR LINUX_ID STREQUAL "fedora" OR
           LINUX_ID_LIKE STREQUAL "rhel" OR LINUX_ID_LIKE STREQUAL "fedora" OR
           OS_RELEASE_CONTENT MATCHES "CentOS" OR OS_RELEASE_CONTENT MATCHES "Red Hat" OR 
           OS_RELEASE_CONTENT MATCHES "Fedora")
            find_program(RPM_PROGRAM rpm)

            if(RPM_PROGRAM)
                message(STATUS "Found rpm: ${RPM_PROGRAM}, enabling RPM package generation")
                list(APPEND CPACK_GENERATOR "RPM")
                set(CPACK_RPM_PACKAGE_LICENSE "MIT")
                set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
                set(CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION_SUMMARY})
                set(CPACK_RPM_PACKAGE_URL ${CPACK_PACKAGE_HOMEPAGE_URL})
                set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
            else()
                message(STATUS "rpm not found, RPM package generation disabled")
            endif()
        endif()
    endif()

    # Always show which generators will be used
    message(STATUS "CPack generators: ${CPACK_GENERATOR}")
endif()

# Set the default package installation directory
if(WIN32)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "MCPServer++")
else()
    set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/mcp-server++")
endif()

# Include CPack
include(CPack)