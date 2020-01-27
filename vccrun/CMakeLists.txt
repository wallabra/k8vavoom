set(VCCRUN_HAXPAK )
set(VCCRUN_K8BUILD )

if("$ENV{USER}" STREQUAL "ketmar")
  set(VCCRUN_HAXPAK ON)
  set(VCCRUN_K8BUILD ON)
endif()

if("$ENV{K8VAVOOMDEV}" STREQUAL "tan")
  set(VCCRUN_HAXPAK ON)
  set(VCCRUN_K8BUILD ON)
endif()

if (ENABLE_VCCRUN)
  if(WIN32)
    # unfuck all build types
    #set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS}")
    #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS} ${UNFUCK_GXX_FLAGS}")

    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -O2 -s")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -s")

    set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2 -s")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -s")

    set(CMAKE_C_FLAGS_MINSIZEREL   "${CMAKE_C_FLAGS_RELWITHDEBINFO} -Os -s")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Os -s")

    set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -s")
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -s")
  endif(WIN32)
  set(MODSOCK_SRC
    modules/mod_socket.h
    modules/mod_socket.cpp
  )


  set(VCCRUN_DEPS ${VAVOOM_MAIN_DEPS} imago)
  set(VCCRUN_LINKLIBS imago ${MAIN_LIBS})

  set(VCCRUN_SOURCES "")
  set(VCCRUN_DEFINES VCC_STANDALONE_EXECUTOR)

  if(WITH_SDL2 AND SDL2_FOUND)
    #include_directories(${SDL2_INCLUDE_DIRS})
    #set(VCCRUN_LINKLIBS ${SDL2_LIBRARIES} ${VCCRUN_LINKLIBS})
    if(WIN32)
      set(VCCRUN_LINKLIBS ${VCCRUN_LINKLIBS} -limm32)
      set(VCCRUN_LINKLIBS ${VCCRUN_LINKLIBS} -lwinmm)
      set(VCCRUN_LINKLIBS ${VCCRUN_LINKLIBS} -lversion)
    endif(WIN32)
    set(VCCRUN_DEFINES "VCCRUN_HAS_SDL" ${VCCRUN_DEFINES})
    set(VCCRUN_DEFINES "VCCRUN_HAS_OPENGL" ${VCCRUN_DEFINES})
  endif(WITH_SDL2 AND SDL2_FOUND)

  set(VCCRUN_RUNTIME_SOURCES
    ../source/scripts.h
    ../source/scripts.cpp
  )

  set(VCCRUN_FSYS_SOURCES
    filesys/fsys.h
    filesys/fsys.cpp
    filesys/pak/zip.cpp
    filesys/pak/dfwad.cpp
  )

  if(VCCRUN_HAXPAK)
    add_definitions(-DSERIALIZER_USE_LIBHA=1)
    set(VCCRUN_FSYS_SOURCES
      ${VCCRUN_FSYS_SOURCES}
      filesys/pak/hax.cpp
      filesys/halib/libha.c
    )
  endif(VCCRUN_HAXPAK)

  if(VCCRUN_K8BUILD)
    add_definitions(-DVCCRUN_K8BUILD=1)
  endif(VCCRUN_K8BUILD)


  set(VCCRUN_SOUND_VORBIS_SOURCES )
  if(VORBIS_FOUND)
    set(VCCRUN_SOUND_VORBIS_SOURCES modules/mod_sound/decoders/vorbis.cpp)
  endif(VORBIS_FOUND)

  set(VCCRUN_SOUND_OPUS_SOURCES )
  if(OPUS_FOUND)
    set(VCCRUN_SOUND_OPUS_SOURCES modules/mod_sound/decoders/opus.cpp)
  endif(OPUS_FOUND)

  set(VCCRUN_SOUND_FLAC_SOURCES )
  if(FLAC_FOUND)
    set(VCCRUN_SOUND_FLAC_SOURCES modules/mod_sound/decoders/flac.cpp)
  endif(FLAC_FOUND)

  set(VCCRUN_SOUND_LIBMAD_SOURCES )
  if(LIBMAD_FOUND)
    set(VCCRUN_SOUND_LIBMAD_SOURCES modules/mod_sound/decoders/mad.cpp)
  endif(LIBMAD_FOUND)

  set(VCCRUN_SOUND_SOURCES
    modules/mod_sound/sound.h
    modules/mod_sound/openal.cpp
    modules/mod_sound/main.cpp
    modules/mod_sound/streamplayer.cpp
    modules/mod_sound/vcapi.cpp
    modules/mod_sound/decoders/wav.cpp
    # xmplite is always here
    modules/mod_sound/decoders/xmplite.cpp
    ${VCCRUN_SOUND_VORBIS_SOURCES}
    ${VCCRUN_SOUND_OPUS_SOURCES}
    ${VCCRUN_SOUND_FLAC_SOURCES}
    ${VCCRUN_SOUND_LIBMAD_SOURCES}
  )


  add_executable(vccrun
    ${VCCRUN_RUNTIME_SOURCES}
    ${VCCRUN_FSYS_SOURCES}
    convars.h
    convars.cpp
    vcc_run.cpp
    vcc_run.h
    vcc_netobj.h
    vcc_netobj.cpp
    vc_api_vccrun.h
    vc_api_vccrun.cpp
    # ini
    modules/mod_ini.h
    modules/mod_ini.cpp
    # SDL2 and OpenGL
    modules/sdlgl/mod_sdlgl.h
    modules/sdlgl/mod_sdlgl.cpp
    # DooM2D:Forever stuff
    modules/dfmap/dfmap.h
    modules/dfmap/mod_dfmap.h
    modules/dfmap/mod_dfmap.cpp
    # file reader
    modules/mod_filerider.h
    modules/mod_filerider.cpp
    # socket
    ${MODSOCK_SRC}
    # sound system
    ${VCCRUN_SOUND_SOURCES}
    # entity grid
    modules/mod_entgrid.h
    modules/mod_entgrid.cpp
    ../source/misc.cpp
  )

  set(VCCRUN_LINKLIBS ${VCCRUN_LINKLIBS} ${NET_LIBRARIES})

  #message(STATUS "VCCRUN: LIBS: " ${VCCRUN_LINKLIBS})
  #message(STATUS "VCCRUN: DEFS: " ${VCCRUN_DEFINES})
  #message(STATUS "VCCRUN: XX: " ${TARGET_IMPLIB})

  if(CYGWIN OR MINGW)
    # prune --out-implib from executables: it doesn't make sense even with --export-all-symbols
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE}")
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
  endif()

  set_target_properties(vccrun PROPERTIES OUTPUT_NAME vccrun)
  target_compile_definitions(vccrun PRIVATE ${VCCRUN_DEFINES})
  #target_link_libraries(vccrun core ${LIBLZMA_LIBRARIES} ${VCCRUN_LINKLIBS})
  add_dependencies(vccrun core vavoomc ${VCCRUN_DEPS})
  target_link_libraries(vccrun vavoomc ${VCCRUN_LINKLIBS} core)
endif(ENABLE_VCCRUN)
