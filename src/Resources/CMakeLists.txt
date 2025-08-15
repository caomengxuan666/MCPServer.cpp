# src/Resources/CMakeLists.txt
set(SOURCES
    resource.cpp
)

set(HEADERS
    resource.h
)

target_sources(mcp-server++ PRIVATE ${SOURCES} ${HEADERS})