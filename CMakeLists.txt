cmake_minimum_required(VERSION 3.0)
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  cmake_policy(SET CMP0026 NEW)
endif(COMMAND cmake_policy)


project(vavoom)

option(WITH_SDL2 "Use SDL2 library" ON)
option(WITH_OPENGL "Use OpenGL library" ON)
option(WITH_OPENAL "Use OpenAL library" ON)
option(WITH_VORBIS "Enable Vorbis support" ON)
option(WITH_LIBMAD "Enable MP3 support" ON)
option(WITH_MIKMOD "Enable MikMod support" ON)
option(WITH_MODPLUG "Enable ModPlug support" ON)
option(WITH_FLAC "Enable FLAC support" ON)
option(WITH_GME "Enable built-in GME support" OFF)
option(WITH_SYS_LIBPNG "Use system libpng" OFF)
option(WITH_SYS_LIBJPG "Use system libjpg" ON)
option(WITH_SYS_ZLIB "Use system zlib" ON)
option(WITH_SYS_LZMA "Use system liblzma" ON)

if(MINGW)
  option(WITH_WINE "Use wine to run built executables" ON)
  option(WITH_STATIC_OPENAL "Use special flags to link with static OpenAL" ON)
endif(MINGW)

option(ENABLE_SIGNAL_HANDLER "Intercept system signals" OFF)

option(ENABLE_CLIENT   "Build client executable" ON)
option(ENABLE_SERVER   "Build dedicated server executable" OFF)
option(ENABLE_MASTER   "Build master server" OFF)
option(ENABLE_VCCRUN   "Build vcc_run executable" OFF)
option(ENABLE_MD2FIXER "Build fixmd2 executable" OFF)

option(ENABLE_VCCRUN_ONLY "Build ONLY vccrun" OFF)

if(UNIX)
  option(ENABLE_WRAPPERS "Enable creation of wrapper scripts" ON)
endif(UNIX)

if(ENABLE_VCCRUN_ONLY)
  set(WITH_OPENAL OFF)
  set(WITH_VORBIS OFF)
  set(WITH_LIBMAD OFF)
  set(WITH_MIKMOD OFF)
  set(WITH_MODPLUG OFF)
  set(WITH_FLAC OFF)
  set(WITH_GME OFF)
  set(WITH_SYS_LIBPNG OFF)
  #set(WITH_SYS_LIBJPG OFF)
  #set(WITH_SYS_ZLIB OFF)
  set(ENABLE_CLIENT OFF)
  set(ENABLE_SERVER OFF)
  set(ENABLE_MASTER OFF)
  set(ENABLE_VCCRUN ON)
  set(ENABLE_MD2FIXER OFF)
else(ENABLE_VCCRUN_ONLY)
  if(NOT ENABLE_CLIENT)
    if(NOT ENABLE_SERVER)
      message(STATUS "Neither client, nor server was requested")
      set(WITH_SYS_LIBPNG OFF)
    else(NOT ENABLE_SERVER)
      message(STATUS "Server-only build, turning off SDL2, OpenGL and OpenAL")
    endif(NOT ENABLE_SERVER)
    set(WITH_SDL2 OFF)
    set(WITH_OPENGL OFF)
    set(WITH_OPENAL OFF)
    set(WITH_VORBIS OFF)
    set(WITH_LIBMAD OFF)
    set(WITH_MIKMOD OFF)
    set(WITH_MODPLUG OFF)
    set(WITH_FLAC OFF)
    set(WITH_GME OFF)
  endif(NOT ENABLE_CLIENT)
endif(ENABLE_VCCRUN_ONLY)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14")

set(UNFUCK_GCC_FLAGS "")
set(UNFUCK_GXX_FLAGS "")

set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-aggressive-loop-optimizations")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-delete-null-pointer-checks")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-strict-aliasing")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-strict-overflow")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fwrapv")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wall")
#set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-frame-address")
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7)
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-stringop-truncation")
  set(UNFUCK_GXX_FLAGS "${UNFUCK_GXX_FLAGS} -Wno-class-memaccess")
endif()

if(ENABLE_SIGNAL_HANDLER)
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-frame-address")
endif(ENABLE_SIGNAL_HANDLER)


# unfuck all build types
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS} ${UNFUCK_GXX_FLAGS}")

set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} -O2 ${UNFUCK_GCC_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 ${UNFUCK_GCC_FLAGS} ${UNFUCK_GXX_FLAGS}")

set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2 -g ${UNFUCK_GCC_FLAGS}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g ${UNFUCK_GCC_FLAGS} ${UNFUCK_GXX_FLAGS}")

set(CMAKE_C_FLAGS_MINSIZEREL   "${CMAKE_C_FLAGS_RELWITHDEBINFO} -Os ${UNFUCK_GCC_FLAGS}")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Os ${UNFUCK_GCC_FLAGS} ${UNFUCK_GXX_FLAGS}")


set(VAVOOM_MAIN_DEPS "")

if(WITH_WINE)
  set(WINERUN "wine")
else(WITH_WINE)
  set(WINERUN "")
endif(WITH_WINE)


## message(STATUS "BUILD: ${CMAKE_BUILD_TYPE}")
## message(STATUS "cflags  : ${CMAKE_C_FLAGS}")
## message(STATUS "cxxflags: ${CMAKE_CXX_FLAGS}")
## message(STATUS "cflags  (dbg): ${CMAKE_C_FLAGS_DEBUG}")
## message(STATUS "cxxflags(dbg): ${CMAKE_CXX_FLAGS_DEBUG}")

if(NOT DEFINED DATADIR)
  set(DATADIR share/vavoom)
endif(NOT DEFINED DATADIR)
if(NOT DEFINED BINDIR)
  set(BINDIR bin)
endif(NOT DEFINED BINDIR)


#
#  **** Add debug / development flags ****
#
if(ENABLE_SIGNAL_HANDLER)
  add_definitions(-DUSE_SIGNAL_HANDLER=1)
  message(STATUS "BUILD: Using Signal Handler")
endif(ENABLE_SIGNAL_HANDLER)


#
#  **** Check for inttypes.h ****
#
find_path(INTTYPES_INCLUDE_DIR inttypes.h)
if(INTTYPES_INCLUDE_DIR)
  add_definitions(-DHAVE_INTTYPES_H=1)
endif(INTTYPES_INCLUDE_DIR)


#
#  **** Check for newtwork libraries ****
#
if(WIN32)
  set(NET_LIBRARIES wsock32)
else(WIN32)
  set(NET_LIBRARIES "")
endif(WIN32)


#
#  **** Check for zlib ****
#
if(WITH_SYS_ZLIB)
  find_package(ZLIB)
  if(ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIR})
  else(ZLIB_FOUND)
    message(STATUS "WARNING: Zlib not found, using internal zlib")
    set(WITH_SYS_ZLIB OFF)
    set(ZLIB_LIBRARIES z)
    set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} z)
  endif(ZLIB_FOUND)
else(WITH_SYS_ZLIB)
  add_definitions(-DUSE_INTERNAL_ZLIB=1)
  add_definitions(-DHAVE_ZLIB_H=1)
  message(STATUS "Using built-in zlib")
  set(ZLIB_LIBRARIES "z")
  set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} z)
endif(WITH_SYS_ZLIB)
set(MAIN_LIBS z ${MAIN_LIBS})


#
#  **** Check for libpng ****
#
if(ENABLE_CLIENT OR ENABLE_SERVER OR ENABLE_VCCRUN)
  if(WITH_SYS_LIBPNG)
    # don't require it, fallback to built-in one, if anything
    find_package(PNG)
    if(PNG_FOUND)
      include_directories(${PNG_INCLUDE_DIR})
      set(MAIN_LIBS ${PNG_LIBRARY} ${MAIN_LIBS})
      add_definitions(-DVAVOOM_USE_LIBPNG=1)
    else(PNG_FOUND)
      message(STATUS "WARNING: LibPNG not found, using internal png reader")
      set(WITH_SYS_LIBPNG OFF)
    endif(PNG_FOUND)
  endif(WITH_SYS_LIBPNG)

  # it is in corelib, so no need to do anything
  #if(NOT WITH_SYS_LIBPNG)
  #  set(MAIN_LIBS png ${MAIN_LIBS})
  #  set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} png)
  #endif(NOT WITH_SYS_LIBPNG)
endif(ENABLE_CLIENT OR ENABLE_SERVER OR ENABLE_VCCRUN)


#
#  **** Check for libjpeg ****
#
if(ENABLE_CLIENT OR ENABLE_SERVER OR ENABLE_VCCRUN)
  if(WITH_SYS_LIBJPG)
    find_package(JPEG)
    if(JPEG_FOUND)
      include_directories(${JPEG_INCLUDE_DIR})
      set(MAIN_LIBS ${JPEG_LIBRARY} ${MAIN_LIBS})
      add_definitions(-DVAVOOM_USE_LIBJPG=1)
    else(JPEG_FOUND)
      message(STATUS "WARNING: LibJpeg not found, using internal jpeg reader")
      set(WITH_SYS_LIBJPG OFF)
    endif(JPEG_FOUND)
  endif(WITH_SYS_LIBJPG)

  if(NOT WITH_SYS_LIBJPG)
    set(MAIN_LIBS jpeg ${MAIN_LIBS})
    set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} jpeg)
  endif(NOT WITH_SYS_LIBJPG)
endif(ENABLE_CLIENT OR ENABLE_SERVER OR ENABLE_VCCRUN)


#
#  **** Check for liblzma ****
#
if(WITH_SYS_LZMA)
  find_package(LibLZMA)
  if(LIBLZMA_FOUND)
    include_directories(${LIBLZMA_INCLUDE_DIRS})
    #message(STATUS "***: " ${LIBLZMA_INCLUDE_DIRS})
    #message(STATUS "***: " ${LIBLZMA_LIBRARIES})
    set(MAIN_LIBS ${LIBLZMA_LIBRARIES} ${MAIN_LIBS})
  else(LIBLZMA_FOUND)
    message(STATUS "WARNING: LibLzma not found, using internal lzma decoder")
    set(WITH_SYS_LZMA OFF)
  endif(LIBLZMA_FOUND)
endif(WITH_SYS_LZMA)

if(NOT WITH_SYS_LZMA)
  set(MAIN_LIBS lzma ${MAIN_LIBS})
  set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} lzma)
  add_definitions(-DUSE_INTERNAL_LZMA=1)
endif(NOT WITH_SYS_LZMA)

if(NOT WITH_SYS_LZMA)
  message(STATUS "LZMA: using internal liblzma")
  set(LIBLZMA_LIBRARIES "lzma")
  add_definitions(-DUSE_INTERNAL_LZMA=1)
endif(NOT WITH_SYS_LZMA)


#
#  **** Check for OpenAL ****
#
if(WITH_OPENAL)
  find_package(OpenAL REQUIRED)
  include_directories(${OPENAL_INCLUDE_DIR})
  ##find_path(OPENALAL_INCLUDE_DIR AL/al.h)
  ##set(MAIN_LIBS OpenAL32 ${MAIN_LIBS})
  set(MAIN_LIBS ${OPENAL_LIBRARY} ${MAIN_LIBS})
  if(WIN32)
    if(WITH_STATIC_OPENAL)
      #k8: my MXE builds are static. live with it.
      add_definitions(-DAL_LIBTYPE_STATIC=1)
    endif(WITH_STATIC_OPENAL)
  endif(WIN32)
endif(WITH_OPENAL)


#
#  **** Check for Vorbis ****
#
if(WITH_VORBIS)
  find_path(OGG_INCLUDE_DIR ogg/ogg.h)
  find_path(VORBIS_INCLUDE_DIR vorbis/codec.h)
  find_library(OGG_LIBRARY NAMES ogg)
  find_library(VORBIS_LIBRARY NAMES vorbis)
  if (OGG_INCLUDE_DIR AND VORBIS_INCLUDE_DIR AND OGG_LIBRARY AND VORBIS_LIBRARY)
    message(STATUS "Found OggVorbis: ${OGG_LIBRARY} ${VORBIS_LIBRARY}")
    set(VORBIS_FOUND ON)
    include_directories(${OGG_INCLUDE_DIR})
    include_directories(${VORBIS_INCLUDE_DIR})
    set(MAIN_LIBS ${VORBIS_LIBRARY} ${OGG_LIBRARY} ${MAIN_LIBS})
  else (OGG_INCLUDE_DIR AND VORBIS_INCLUDE_DIR AND OGG_LIBRARY AND VORBIS_LIBRARY)
    message(STATUS "Could NOT find OggVorbis libraries")
    set(VORBIS_FOUND OFF)
  endif (OGG_INCLUDE_DIR AND VORBIS_INCLUDE_DIR AND OGG_LIBRARY AND VORBIS_LIBRARY)
else(WITH_VORBIS)
  set(VORBIS_FOUND OFF)
endif(WITH_VORBIS)


#
#  **** Check for libmad ****
#
if(WITH_LIBMAD)
  find_path(MAD_INCLUDE_DIR mad.h)
  find_library(MAD_LIBRARY NAMES mad)
  if(MAD_INCLUDE_DIR AND MAD_LIBRARY)
    message(STATUS "Found LibMAD: ${MAD_LIBRARY}")
    set(LIBMAD_FOUND ON)
    include_directories(${MAD_INCLUDE_DIR})
    set(MAIN_LIBS ${MAD_LIBRARY} ${MAIN_LIBS})
  else(MAD_INCLUDE_DIR AND MAD_LIBRARY)
    message(STATUS "Could NOT find LibMAD libraries")
    set(LIBMAD_FOUND OFF)
  endif(MAD_INCLUDE_DIR AND MAD_LIBRARY)
else(WITH_LIBMAD)
  set(LIBMAD_FOUND OFF)
endif(WITH_LIBMAD)


#
#  **** Check for MikMod ****
#
if(WITH_MIKMOD)
  # Find libmikmod-config script
  if(NOT DEFINED LIBMIKMOD_CONFIG)
    set(LIBMIKMOD_CONFIG libmikmod-config)
  endif(NOT DEFINED LIBMIKMOD_CONFIG)
  find_program(LIBMIKMOD_CONFIG_EXECUTABLE ${LIBMIKMOD_CONFIG})
  if(LIBMIKMOD_CONFIG_EXECUTABLE)
    # run the libmikmod-config program to get cxxflags
    exec_program(sh
      ARGS "${LIBMIKMOD_CONFIG_EXECUTABLE} --cflags"
      OUTPUT_VARIABLE MIKMOD_CXX_FLAGS
      RETURN_VALUE RET)
    if(RET EQUAL 0)
      # parse definitions from cxxflags
      string(REGEX MATCHALL "-D.*[^ ;]+" MIKMOD_DEFINITIONS ${MIKMOD_CXX_FLAGS})
      # drop -D* from CXXFLAGS
      string(REGEX REPLACE "-D[^ ;]*" "" MIKMOD_CXX_FLAGS ${MIKMOD_CXX_FLAGS})
      # parse incdirs from cxxflags, drop -I prefix
      string(REGEX MATCHALL "-I.*[^ ;]+" MIKMOD_INCLUDE_DIRS ${MIKMOD_CXX_FLAGS})
      string(REGEX REPLACE "-I" "" MIKMOD_INCLUDE_DIRS "${MIKMOD_CXX_FLAGS}")
      # convert space to semicolons for list
      string(REGEX REPLACE " " ";" MIKMOD_INCLUDE_DIRS "${MIKMOD_INCLUDE_DIRS}")
    endif(RET EQUAL 0)

    # run the libmikmod-config program to get the libs
    exec_program(sh
      ARGS "${LIBMIKMOD_CONFIG_EXECUTABLE} --libs"
      OUTPUT_VARIABLE MIKMOD_LIBRARIES
      RETURN_VALUE RET)
    if(RET EQUAL 0)
      string(REGEX REPLACE " " ";" MIKMOD_LIBRARIES "${MIKMOD_LIBRARIES}")
      # extract linkdirs (-L) for rpath (i.e., LINK_DIRECTORIES)
      string(REGEX MATCHALL "-L[^ ;]+" MIKMOD_LIBRARY_DIRS "${MIKMOD_LIBRARIES}")
      string(REGEX REPLACE "-L" "" MIKMOD_LIBRARY_DIRS "${MIKMOD_LIBRARY_DIRS}")
      # convert space to semicolons for list
      STRING(REGEX REPLACE " " ";" MIKMOD_LIBRARY_DIRS "${MIKMOD_LIBRARY_DIRS}")
    endif(RET EQUAL 0)

    if(MIKMOD_INCLUDE_DIRS AND MIKMOD_LIBRARIES)
      message(STATUS "Found MIKMOD: ${MIKMOD_LIBRARIES}")
      set(MIKMOD_FOUND ON)
      include_directories(${MIKMOD_INCLUDE_DIRS})
      set(MAIN_LIBS ${MIKMOD_LIBRARIES} ${MAIN_LIBS})
    else(MIKMOD_INCLUDE_DIRS AND MIKMOD_LIBRARIES)
      message(STATUS "Could NOT find MIKMOD libraries")
      set(MIKMOD_FOUND OFF)
    endif(MIKMOD_INCLUDE_DIRS AND MIKMOD_LIBRARIES)
  endif(LIBMIKMOD_CONFIG_EXECUTABLE)
else(WITH_MIKMOD)
  set(MIKMOD_FOUND OFF)
endif(WITH_MIKMOD)


#
#  **** Check for ModPlug ****
#
if(WITH_MODPLUG)
  FIND_PACKAGE(PkgConfig REQUIRED)
  PKG_CHECK_MODULES(MODPLUG libmodplug)
  if(MODPLUG_INCLUDE_DIRS AND MODPLUG_LIBRARIES)
    message(STATUS "Found MODPLUG: ${MODPLUG_LIBRARIES}")
    set(MODPLUG_FOUND ON)
    include_directories(${MODPLUG_INCLUDE_DIRS})
    set(MAIN_LIBS ${MODPLUG_LIBRARIES} ${MAIN_LIBS})
  else(MODPLUG_INCLUDE_DIRS AND MODPLUG_LIBRARIES)
    message(STATUS "Could NOT find MODPLUG libraries")
    set(MODPLUG_FOUND OFF)
  endif(MODPLUG_INCLUDE_DIRS AND MODPLUG_LIBRARIES)
else(WITH_MODPLUG)
  set(MODPLUG_FOUND OFF)
endif(WITH_MODPLUG)


#
#  **** Check for FLAC ****
#
if(WITH_FLAC)
  find_path(FLAC_INCLUDE_DIR FLAC++/decoder.h)
  find_library(FLAC_LIBRARY NAMES FLAC)
  find_library(FLACPP_LIBRARY NAMES FLAC++)
  if(FLAC_INCLUDE_DIR AND FLAC_LIBRARY AND FLACPP_LIBRARY)
    message(STATUS "Found FLAC: ${FLAC_LIBRARY} ${FLACPP_LIBRARY}")
    set(FLAC_FOUND ON)
    include_directories(${FLAC_INCLUDE_DIR})
    set(MAIN_LIBS ${FLACPP_LIBRARY} ${FLAC_LIBRARY} ${MAIN_LIBS})
  else(FLAC_INCLUDE_DIR AND FLAC_LIBRARY AND FLACPP_LIBRARY)
    message(STATUS "Could NOT find FLAC libraries")
    set(FLAC_FOUND OFF)
  endif(FLAC_INCLUDE_DIR AND FLAC_LIBRARY AND FLACPP_LIBRARY)
else(WITH_FLAC)
  set(FLAC_FOUND OFF)
endif(WITH_FLAC)


#
#  **** Platform specific library checks ****
#
if(NOT WIN32)
  # ---------------- Other *NIX platforms ----------------
  if(WITH_SDL2)
    find_package(SDL2 REQUIRED)
    #PKG_SEARCH_MODULE(SDL2 REQUIRED sdl2)
    include_directories(${SDL2_INCLUDE_DIRS})
    set(MAIN_LIBS ${SDL2_LIBRARIES} ${MAIN_LIBS})
    #message(STATUS "SDL2 INCLUDES:" ${SDL2_INCLUDE_DIRS})
    #message(STATUS "SDL2 LIBS:" ${SDL2_LIBRARIES})
  else(WITH_SDL2)
    set(OPENGL_FOUND OFF)
    set(WITH_OPENGL OFF)
  endif(WITH_SDL2)

  if(WITH_OPENGL)
    if(NOT WITH_SDL2)
      message(SEND_ERROR "SDL2 required for OpenGL")
      return()
    endif(NOT WITH_SDL2)
    set(OpenGL_GL_PREFERENCE LEGACY)
    find_package(OpenGL REQUIRED)
    if(OPENGL_FOUND)
      message(STATUS "Found OpenGL")
      include_directories(${OPENGL_INCLUDE_DIR})
      set(MAIN_LIBS ${OPENGL_gl_LIBRARY} ${MAIN_LIBS})
    endif(OPENGL_FOUND)
  else(WITH_OPENGL)
    set(OPENGL_FOUND OFF)
  endif(WITH_OPENGL)
endif(NOT WIN32)


if(WITH_GME)
  set(MAIN_LIBS gmp ${MAIN_LIBS})
  set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} gme)
endif(WITH_GME)


message(STATUS "=== COMPONENTS ===")
message(STATUS "  VaVoom Client: " ${ENABLE_CLIENT})
message(STATUS "  VaVoom Server: " ${ENABLE_SERVER})
message(STATUS "  VaVoom Master: " ${ENABLE_MASTER})
message(STATUS "  VccRun       : " ${ENABLE_VCCRUN})


add_subdirectory(libs)
if(NOT ENABLE_VCCRUN_ONLY)
  add_subdirectory(utils)
  add_subdirectory(source)
  add_subdirectory(progs)
  add_subdirectory(basev)
  #foreach(DIR ${EXTRA_SUBDIRS})
  #  add_subdirectory(${DIR})
  #endforeach(DIR)
endif(NOT ENABLE_VCCRUN_ONLY)

if(ENABLE_VCCRUN)
  add_subdirectory(vccrun)
endif(ENABLE_VCCRUN)
