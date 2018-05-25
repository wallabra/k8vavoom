add_subdirectory(core)
add_subdirectory(liblzma)
add_subdirectory(imago)

if(NOT WITH_ZLIB)
  add_subdirectory(zlib)
endif(NOT WITH_ZLIB)

if(ENABLE_CLIENT)
  if(WITH_GME)
    message(STATUS "GME enabled")
    add_subdirectory(gme)
  endif(WITH_GME)

  add_subdirectory(timidity)

  if(NOT WITH_LIBJPG)
    add_subdirectory(jpeg)
  endif(NOT WITH_LIBJPG)
endif(ENABLE_CLIENT)
