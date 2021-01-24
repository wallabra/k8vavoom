# options "ENABLE_LAUNCHER" and "ENABLE_MASTER" were moved to main cmake file

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

add_subdirectory(fixmd2)
add_subdirectory(master)
add_subdirectory(server_query_demo)
add_subdirectory(vlumpy)

if(ENABLE_CLIENT OR ENABLE_UTILS_ONLY)
  add_subdirectory(vshadpp)
endif()
