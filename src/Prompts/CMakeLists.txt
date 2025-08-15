# src/Prompts/CMakeLists.txt
set(PROMPTS_SOURCES
    prompt.cpp
)

set(PROMPTS_HEADERS
    prompt.h
)

# Add source files to the library
target_sources(mcp-server++ PRIVATE ${PROMPTS_SOURCES} ${PROMPTS_HEADERS})

# Add include directories
target_include_directories(mcp-server++ PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})