# options "ENABLE_LAUNCHER" and "ENABLE_MASTER" were moved to main cmake file

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

add_subdirectory(acc)
add_subdirectory(fixmd2)
#add_subdirectory(glbsp)
add_subdirectory(glvis)
add_subdirectory(master)
add_subdirectory(vcc)
add_subdirectory(vlumpy)
