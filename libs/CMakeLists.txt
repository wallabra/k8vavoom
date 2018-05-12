add_subdirectory(core)
add_subdirectory(liblzma)

if(WITH_GME)
  message(STATUS "GME enabled")
  add_subdirectory(gme)
endif(WITH_GME)

add_subdirectory(timidity)
