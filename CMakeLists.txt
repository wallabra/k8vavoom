cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
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
  SET(WITH_OPENAL OFF)
  SET(WITH_VORBIS OFF)
  SET(WITH_LIBMAD OFF)
  SET(WITH_MIKMOD OFF)
  SET(WITH_MODPLUG OFF)
  SET(WITH_FLAC OFF)
  SET(WITH_GME OFF)
  SET(WITH_SYS_LIBPNG OFF)
  #SET(WITH_SYS_LIBJPG OFF)
  #SET(WITH_SYS_ZLIB OFF)
  SET(ENABLE_CLIENT ON)
  SET(ENABLE_SERVER OFF)
  SET(ENABLE_MASTER OFF)
  SET(ENABLE_VCCRUN ON)
  SET(ENABLE_MD2FIXER OFF)
else(ENABLE_VCCRUN_ONLY)
  if(NOT ENABLE_CLIENT)
    message(STATUS "Server-only build, turning off SDL2, OpenGL and OpenAL")
    SET(WITH_SDL2 OFF)
    SET(WITH_OPENGL OFF)
    SET(WITH_OPENAL OFF)
    SET(WITH_VORBIS OFF)
    SET(WITH_LIBMAD OFF)
    SET(WITH_MIKMOD OFF)
    SET(WITH_MODPLUG OFF)
    SET(WITH_FLAC OFF)
    SET(WITH_GME OFF)
  endif(NOT ENABLE_CLIENT)
endif(ENABLE_VCCRUN_ONLY)

IF( NOT CMAKE_BUILD_TYPE )
  SET( CMAKE_BUILD_TYPE Debug CACHE STRING
    "Choose the type of build, options are: Debug Release."
  FORCE )
ENDIF()


set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14" )

set( UNFUCK_GCC_FLAGS "" )

set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-aggressive-loop-optimizations" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-delete-null-pointer-checks" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-strict-aliasing" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-strict-overflow" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fwrapv" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wall" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-frame-address" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-stringop-truncation" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-class-memaccess" )

if(ENABLE_SIGNAL_HANDLER)
  set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-frame-address" )
endif(ENABLE_SIGNAL_HANDLER)

set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS}" )
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS}" )

set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 ${UNFUCK_GCC_FLAGS}" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 ${UNFUCK_GCC_FLAGS}" )


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
#  **** Check for liblzma ****
#
if(WITH_SYS_LZMA)
  find_package(LibLZMA)
  if(LIBLZMA_FOUND)
    include_directories(${LIBLZMA_INCLUDE_DIRS})
  else(LIBLZMA_FOUND)
    set(WITH_SYS_LZMA OFF)
  endif(LIBLZMA_FOUND)
endif(WITH_SYS_LZMA)

if(NOT WITH_SYS_LZMA)
  message(STATUS "LZMA: using internal liblzma")
  set(LIBLZMA_LIBRARIES "lzma")
  add_definitions(-DUSE_INTERNAL_LZMA=1)
endif(NOT WITH_SYS_LZMA)

#
#  **** Check for zlib ****
#
if(WITH_SYS_ZLIB)
  find_package(ZLIB REQUIRED)
  include_directories(${ZLIB_INCLUDE_DIR})
  set(ZLIB_DEPS "")
else(WITH_SYS_ZLIB)
  add_definitions(-DUSE_INTERNAL_ZLIB=1)
  add_definitions(-DHAVE_ZLIB_H=1)
  message(STATUS "Using built-in zlib")
  set(ZLIB_LIBRARIES "z")
  set(ZLIB_DEPS "z")
endif(WITH_SYS_ZLIB)

#
#  **** Check for OpenAL ****
#
if(WITH_OPENAL)
  find_package(OpenAL REQUIRED)
  ##find_path(OPENALAL_INCLUDE_DIR AL/al.h)
  ##set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENAL_SOURCES})
  ##set(MAIN_LIBS OpenAL32 ${MAIN_LIBS})
endif(WITH_OPENAL)

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
#FIXME we need this on Solaris
# AC_SEARCH_LIBS([recvfrom], [socket], [], [AC_ERROR([Library containing recvfrom not found])])
# AC_SEARCH_LIBS([gethostbyname], [nsl], [], [AC_ERROR([Library containing gethostbyname not found])])
endif(WIN32)

add_subdirectory(libs)
add_subdirectory(utils)
if(NOT ENABLE_VCCRUN_ONLY)
  add_subdirectory(source)
  add_subdirectory(progs)
  add_subdirectory(basev)
  foreach(DIR ${EXTRA_SUBDIRS})
    add_subdirectory(${DIR})
  endforeach(DIR)
endif(NOT ENABLE_VCCRUN_ONLY)
