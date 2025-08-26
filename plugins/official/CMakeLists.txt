# Automatically add all subdirectories that contain a CMakeLists.txt file
file(GLOB PLUGIN_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
foreach(PLUGIN_DIR ${PLUGIN_DIRS})
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_DIR})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_DIR}/CMakeLists.txt)
      add_subdirectory(${PLUGIN_DIR})
    endif()
  endif()
endforeach()