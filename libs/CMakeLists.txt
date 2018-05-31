add_subdirectory(core)

if(NOT WITH_SYS_LZMA)
  add_subdirectory(liblzma)
endif(NOT WITH_SYS_LZMA)

if(NOT WITH_SYS_LIBJPG)
  add_subdirectory(jpeg)
endif(NOT WITH_SYS_LIBJPG)

if(NOT WITH_SYS_ZLIB)
  add_subdirectory(zlib)
endif(NOT WITH_SYS_ZLIB)

add_subdirectory(imago)

if(ENABLE_CLIENT AND NOT ENABLE_VCCRUN_ONLY)
  if(WITH_GME)
    message(STATUS "GME enabled")
    add_subdirectory(gme)
  endif(WITH_GME)

  add_subdirectory(timidity)
endif(ENABLE_CLIENT)
