# options "ENABLE_LAUNCHER" and "ENABLE_MASTER" were moved to main cmake file

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

add_subdirectory(acc)
add_subdirectory(fixmd2)
#add_subdirectory(glbsp)
if(ENABLE_GLVIS)
  if(NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(glvis)
  endif(NOT CMAKE_CROSSCOMPILING)
endif(ENABLE_GLVIS)
add_subdirectory(master)
add_subdirectory(server_query_demo)
if(ENABLE_VCC)
  add_subdirectory(vcc)
endif(ENABLE_VCC)
add_subdirectory(vlumpy)

if(ENABLE_CLIENT OR ENABLE_UTILS_ONLY)
  add_subdirectory(vshadpp)
endif()
