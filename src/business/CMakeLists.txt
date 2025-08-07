# src/business/CMakeLists.txt
file(GLOB BUSINESS_SOURCES *.cpp)

add_library(mcp_business ${BUSINESS_SOURCES})

target_include_directories(mcp_business PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mcp_business>
    ${PROJECT_SOURCE_DIR}/third_party
    ${PROJECT_SOURCE_DIR}/third_party/nlohmann
)

target_compile_features(mcp_business PUBLIC cxx_std_20)

if(WIN32)
    if(MSVC)
        target_compile_options(mcp_business PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(mcp_business PRIVATE -Wall -Wextra)
endif()

target_link_libraries(mcp_business PUBLIC 
    mcp_transport
)