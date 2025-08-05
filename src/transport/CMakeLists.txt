# src/transport/CMakeLists.txt
file(GLOB TRANSPORT_SOURCES "*.cpp")

add_library(mcp_transport ${TRANSPORT_SOURCES})

target_include_directories(mcp_transport PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mcp_transport>
    ${PROJECT_SOURCE_DIR}/third_party
    ${PROJECT_SOURCE_DIR}/third_party/asio/include
    ${PROJECT_SOURCE_DIR}/third_party/asio/standalone/include
)

target_compile_features(mcp_transport PUBLIC cxx_std_20)

if(WIN32)
    if(MSVC)
        target_compile_options(mcp_transport PRIVATE /W4 /permissive-)
        target_compile_definitions(mcp_transport PRIVATE _WIN32_WINNT=0x0601)
    endif()
else()
    target_compile_options(mcp_transport PRIVATE -Wall -Wextra)
endif()

target_link_libraries(mcp_transport PUBLIC 
    mcp_protocol
)