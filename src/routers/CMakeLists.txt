file(GLOB_RECURSE SOURCES *.h *.hpp *.cpp)
target_sources(mcp-server++ PRIVATE ${SOURCES})