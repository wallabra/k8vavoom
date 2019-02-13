cmake_minimum_required(VERSION 3.0)
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  cmake_policy(SET CMP0026 NEW)
endif(COMMAND cmake_policy)


project(vavoom)

option(WITH_SDL2 "Use SDL2 library" ON)
option(WITH_OPENGL "Use OpenGL library" ON)
option(WITH_VORBIS "Enable Vorbis support" ON)
option(WITH_OPUS "Enable Opus support" ON)
option(WITH_LIBMAD "Enable MP3 support" ON)
option(WITH_FLAC "Enable FLAC support" ON)
option(WITH_SYS_LIBJPG "Use system libjpg" ON)
option(WITH_SYS_ZLIB "Use system zlib" ON)
option(WITH_SYS_LZMA "Use system liblzma" ON)
option(WITH_SSE3 "Use SSE3 instruction set" OFF)
option(WITH_SSE4 "Use SSE4 instruction set" OFF)
option(WITH_SSE41 "Use SSE4.1 instruction set" OFF)
option(WITH_SSE42 "Use SSE4.2 instruction set" OFF)
option(WITH_GLAD "Use supplied GLAD for GL bindings" OFF)
option(NO_SSE "Disable all SSE instructions" OFF)
option(NO_PTHREAD "Disable pthreads, use C11 threads instead" OFF)
option(NO_MEMRCHR "Disable usage of memrchr()" OFF)
option(INSANE_AVX "Use AVX2 instruction set (don't do this, it won't give you any noticeable speedup)" OFF)
option(WITH_STRTODEX "Use internal strtod implementation to parse floats" ON)
option(WITH_OLD_CPU "Use pIV CPU instruction set" OFF)
option(DEBUG_FPU_CHECKS "Use FPU and turn on some checks (GNU/Linux only)" OFF)

if(CMAKE_CROSSCOMPILING AND WIN32)
  #if(MINGW)
  option(WITH_WINE "Use wine to run built executables" ON)
  option(WITH_STATIC_OPENAL "Use special flags to link with static OpenAL" ON)
  #endif(MINGW)
  set(WITH_STRTODEX ON)
endif(CMAKE_CROSSCOMPILING AND WIN32)

if(SWITCH_LIBNX)
  option(WITH_NXLINK "Redirect standard output to NXLink" OFF)
endif(SWITCH_LIBNX)

if(NOT CMAKE_COMPILER_IS_GNUCC)
  message(SEND_ERROR "GNU C/C++ compilers are required to build VaVoom and tools")
  return()
endif(NOT CMAKE_COMPILER_IS_GNUCC)

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
  #set(WITH_VORBIS OFF)
  #set(WITH_LIBMAD OFF)
  #set(WITH_FLAC OFF)
  #set(WITH_SYS_LIBJPG OFF)
  #set(WITH_SYS_ZLIB OFF)
  set(ENABLE_CLIENT OFF)
  set(ENABLE_SERVER OFF)
  set(ENABLE_MASTER OFF)
  set(ENABLE_VCCRUN ON)
  set(ENABLE_MD2FIXER OFF)

  if (NOT WITH_VORBIS OR NOT WITH_FLAC)
    message(SEND_ERROR "OpenAL, Vorbis and Flac are required")
    return()
  endif()
else(ENABLE_VCCRUN_ONLY)
  if(NOT ENABLE_CLIENT)
    if(NOT ENABLE_SERVER)
      message(STATUS "Neither client, nor server was requested")
    else(NOT ENABLE_SERVER)
      message(STATUS "Server-only build, turning off SDL2, OpenGL and OpenAL")
    endif(NOT ENABLE_SERVER)
    set(WITH_SDL2 OFF)
    set(WITH_OPENGL OFF)
    set(WITH_VORBIS OFF)
    set(WITH_OPUS OFF)
    set(WITH_LIBMAD OFF)
    set(WITH_FLAC OFF)
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
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-diagnostics-show-caret")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wabi=10")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fabi-version=10")
if(SWITCH_LIBNX)
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fsigned-char")
  set(UNFUCK_GXX_FLAGS "${UNFUCK_GXX_FLAGS} -fexceptions")
  set(UNFUCK_GXX_FLAGS "${UNFUCK_GXX_FLAGS} -frtti")
endif(SWITCH_LIBNX)
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wall")
#set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-frame-address")
# k8: fuck you, gcc. i have my own style, gtfo.
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-misleading-indentation")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-ignored-attributes")

if(WITH_STRTODEX)
  add_definitions(-DVCORE_ALLOW_STRTODEX=1)
endif(WITH_STRTODEX)

if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.4)
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wimplicit-fallthrough")
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wshadow=local")
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Walloc-zero")
endif()

if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.7)
  #set(UNFUCK_GXX_FLAGS "${UNFUCK_GXX_FLAGS} -Wno-class-memaccess")
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wmultistatement-macros")
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wmissing-attributes")
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wcast-function-type")
  set(UNFUCK_GXX_FLAGS "${UNFUCK_GCC_FLAGS} -Wsuggest-override")
endif()

# some math/FPU options
if(NOT DEBUG_FPU_CHECKS AND NOT SWITCH_LIBNX)
  # use -ffast-math, 'cause why not?
  # k8: nope, i command my former self to not do this
  #set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -ffast-math")
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-math-errno")
endif(NOT DEBUG_FPU_CHECKS AND NOT SWITCH_LIBNX)
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-unsafe-math-optimizations")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-associative-math")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-reciprocal-math")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-finite-math-only")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fsigned-zeros")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fsignaling-nans") # do not touch my NaNs!
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -frounding-math") # get lost!

if(NOT NO_SSE)
  if(DEBUG_FPU_CHECKS)
    message(STATUS "BUILD: using FPU")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mfpmath=387")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -ffloat-store")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fsignaling-nans")
    add_definitions(-DUSE_FPU_MATH=1)
  elseif(INSANE_AVX)
    message(STATUS "BUILD: using AVX2 instriction set")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -march=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mtune=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mavx2")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mfpmath=sse")
  elseif (WITH_SSE42)
    message(STATUS "BUILD: using SSE4.2 instriction set")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -march=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mtune=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -msse42")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mfpmath=sse")
  elseif (WITH_SSE41)
    message(STATUS "BUILD: using SSE4.1 instriction set")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -march=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mtune=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -msse41")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mfpmath=sse")
  elseif (WITH_SSE4)
    message(STATUS "BUILD: using SSE4 instriction set")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -march=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mtune=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -msse4")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mfpmath=sse")
  elseif (WITH_SSE3)
    message(STATUS "BUILD: using SSE3 instriction set")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -march=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mtune=native")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -msse3")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mfpmath=sse")
  else()
    if(NOT WIN32)
      if(WITH_OLD_CPU)
        message(STATUS "BUILD: using SSE2/p4 instriction set")
        set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -march=pentium4")
        set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mtune=pentium4")
      else(WITH_OLD_CPU)
        message(STATUS "BUILD: using SSE2/native instriction set")
        set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -march=native")
        set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mtune=native")
      endif(WITH_OLD_CPU)
      set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -msse2")
      set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mno-avx")
      set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mfpmath=sse")
    else(NOT WIN32)
      message(STATUS "BUILD: using SSE2/p4 instriction set")
      set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -march=pentium4")
      set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mtune=pentium4")
      set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -msse2")
      set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mno-avx")
      set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -mfpmath=sse")
    endif(NOT WIN32)
  endif()
endif(NOT NO_SSE)

if(NOT WIN32)
  if(NOT NO_PTHREAD)
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -pthread")
    set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -pthread")
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -pthread")
  endif(NOT NO_PTHREAD)
else(NOT WIN32)
  set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -mwindows")
  set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -mwindows")
endif(NOT WIN32)


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


add_definitions(-DZDBSP_DISABLE_SSE=1)

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
if(ENABLE_CLIENT OR ENABLE_VCCRUN)
  if(SWITCH_LIBNX)
    # massive fucking hack because nothing works right
    set(OPENAL_LIBRARY "openal-soft")
    set(OPENAL_INCLUDE_DIR "$ENV{PORTLIBS_PREFIX}/include/AL")
  else(SWITCH_LIBNX)
    find_package(OpenAL REQUIRED)
  endif(SWITCH_LIBNX)
  include_directories(${OPENAL_INCLUDE_DIR})
  ##find_path(OPENALAL_INCLUDE_DIR AL/al.h)
  ##set(MAIN_LIBS OpenAL32 ${MAIN_LIBS})
  if(WITH_STATIC_OPENAL)
    #k8: my MXE builds are static. live with it.
    add_definitions(-DAL_LIBTYPE_STATIC=1)
  endif(WITH_STATIC_OPENAL)
  set(MAIN_LIBS ${OPENAL_LIBRARY} ${MAIN_LIBS})
  if(WIN32)
    set(MAIN_LIBS ${OPENAL_LIBRARY} winmm ${MAIN_LIBS})
  endif(WIN32)
endif(ENABLE_CLIENT OR ENABLE_VCCRUN)

if(WIN32)
  set(VAVOOM_SHITDOZE_LIBS winmm)
else(WIN32)
  set(VAVOOM_SHITDOZE_LIBS )
endif(WIN32)

set(MAIN_LIBS ${VAVOOM_SHITDOZE_LIBS} ${MAIN_LIBS})


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
#  **** Check for Opus ****
#
if(WITH_OPUS)
  FIND_PACKAGE(PkgConfig REQUIRED)
  #PKG_CHECK_MODULES(OPUS opusfile libopusenc ogg)
  PKG_CHECK_MODULES(OPUS opusfile opus ogg)
  if(OPUS_INCLUDE_DIRS AND OPUS_LIBRARIES)
    message(STATUS "Found Opus: ${OPUS_LIBRARIES}")
    set(OPUS_FOUND ON)
    include_directories(${OPUS_INCLUDE_DIRS})
    set(MAIN_LIBS ${OPUS_LIBRARIES} ${MAIN_LIBS})
  else(OPUS_INCLUDE_DIRS AND OPUS_LIBRARIES)
    message(STATUS "Could NOT find Opus libraries")
    set(OPUS_FOUND OFF)
  endif(OPUS_INCLUDE_DIRS AND OPUS_LIBRARIES)
else(WITH_OPUS)
  set(OPUS_FOUND OFF)
endif(WITH_OPUS)


set(XMPLITE_FOUND ON)
set(MAIN_LIBS xmplite ${MAIN_LIBS})
include_directories(libs/xmplite)
add_definitions(-DLIBXMP_CORE_PLAYER)
add_definitions(-DXMP_SYM_VISIBILITY)


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
#if(NOT WIN32)
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
    if(SWITCH_LIBNX)
      # massive fucking hack because nothing works right
      set(OPENGL_gl_LIBRARY "-L$ENV{PORTLIBS_PREFIX}/lib -lEGL -lglapi -ldrm_nouveau")
      set(OPENGL_INCLUDE_DIR "$ENV{PORTLIBS_PREFIX}/include")
      set(OPENGL_FOUND ON)
    else(SWITCH_LIBNX)
      find_package(OpenGL REQUIRED)
    endif(SWITCH_LIBNX)
    if(OPENGL_FOUND)
      message(STATUS "Found OpenGL")
      include_directories(${OPENGL_INCLUDE_DIR})
      if(SWITCH_LIBNX)
        # have to reorder this shit or else it doesn't link
        set(MAIN_LIBS ${MAIN_LIBS} ${OPENGL_gl_LIBRARY})
      else(SWITCH_LIBNX)
        set(MAIN_LIBS ${OPENGL_gl_LIBRARY} ${MAIN_LIBS})
      endif(SWITCH_LIBNX)
    endif(OPENGL_FOUND)
  else(WITH_OPENGL)
    set(OPENGL_FOUND OFF)
  endif(WITH_OPENGL)
#endif(NOT WIN32)


if(NO_SSE)
  add_definitions(-DNO_SSE=1)
endif(NO_SSE)

if(NO_PTHREAD)
  add_definitions(-DNO_PTHREAD=1)
endif(NO_PTHREAD)

if(NO_MEMRCHR)
  add_definitions(-DNO_MEMRCHR=1)
endif(NO_MEMRCHR)

if(WITH_GLAD)
  add_definitions(-DUSE_GLAD=1)
  if(GLAD_INCLUDE_DIR AND GLAD_LIBRARY)
    include_directories(${GLAD_INCLUDE_DIR})
    set(MAIN_LIBS ${GLAD_LIBRARY} ${MAIN_LIBS})
  else(GLAD_INCLUDE_DIR AND GLAD_LIBRARY)
    include_directories("libs/glad")
    set(MAIN_LIBS glad ${MAIN_LIBS})
    set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} glad)
  endif(GLAD_INCLUDE_DIR AND GLAD_LIBRARY)
endif(WITH_GLAD)

if(SWITCH_LIBNX)
  message(STATUS "Building for Switch")
  include_directories("$ENV{DEVKITPRO}/libnx/include")
  include_directories("$ENV{PORTLIBS_PREFIX}/include")
  add_definitions(-D__SWITCH__)
  # add libnx at the end of lib list
  set(MAIN_LIBS ${MAIN_LIBS} "-L$ENV{PORTLIBS_PREFIX}/lib -lnx")
  message(STATUS "Libs: ${MAIN_LIBS}")
  # why the fuck does this not work properly in the toolchain file
  set(CMAKE_C_COMPILER "aarch64-none-elf-gcc")
  set(CMAKE_CXX_COMPILER "aarch64-none-elf-g++")
  set(CMAKE_AR "aarch64-none-elf-gcc-ar")
  set(CMAKE_RANLIB "aarch64-none-elf-gcc-ranlib")
  # turn on nxlink if necessary
  if(WITH_NXLINK)
    add_definitions(-DSWITCH_NXLINK)
  endif(WITH_NXLINK)
else(SWITCH_LIBNX)
  ### GnuTLS
  # # #FindGnuTLS()
  # # find_package(GnuTLS)
  # # if(GNUTLS_FOUND)
  # #   include_directories(${GNUTLS_INCLUDE_DIR})
  # #   set(MAIN_LIBS ${GNUTLS_LIBRARIES} ${MAIN_LIBS})
  # #   add_definitions(${GNUTLS_DEFINITIONS})
  # #   add_definitions(-DUSE_GNU_TLS)
  # # endif(GNUTLS_FOUND)

  FIND_PACKAGE(PkgConfig REQUIRED)
  PKG_CHECK_MODULES(GNUTLS gnutls)
  if(GNUTLS_FOUND)
    #message(STATUS "GnuTLS INC: " ${GNUTLS_INCLUDE_DIR})
    #message(STATUS "GnuTLS LIB: " ${GNUTLS_LIBRARIES})
    #message(STATUS "GnuTLS DEF: " ${GNUTLS_DEFINITIONS})
    include_directories(${GNUTLS_INCLUDE_DIR})
    set(MAIN_LIBS ${GNUTLS_LIBRARIES} ${MAIN_LIBS})
    add_definitions(${GNUTLS_DEFINITIONS})
    add_definitions(-DUSE_GNU_TLS)
  endif(GNUTLS_FOUND)
endif(SWITCH_LIBNX)


message(STATUS "=== COMPONENTS ===")
message(STATUS "  VaVoom Client: " ${ENABLE_CLIENT})
message(STATUS "  VaVoom Server: " ${ENABLE_SERVER})
message(STATUS "  VaVoom Master: " ${ENABLE_MASTER})
message(STATUS "  VccRun       : " ${ENABLE_VCCRUN})
message(STATUS "  strtodEx     : " ${WITH_STRTODEX})
if(GNUTLS_FOUND)
  message(STATUS "  GNUTLS       : ON")
endif(GNUTLS_FOUND)


add_subdirectory(libs)
if(NOT ENABLE_VCCRUN_ONLY)
  if(NOT CMAKE_CROSSCOMPILING OR WIN32)
    # don't build utilities, progs or the pk3 for the target platform if it's not the same as host
    add_subdirectory(utils)
  endif(NOT CMAKE_CROSSCOMPILING OR WIN32)
  add_subdirectory(source)
  if(NOT CMAKE_CROSSCOMPILING OR WIN32)
    add_subdirectory(progs)
    add_subdirectory(basev)
  endif(NOT CMAKE_CROSSCOMPILING OR WIN32)
  #foreach(DIR ${EXTRA_SUBDIRS})
  #  add_subdirectory(${DIR})
  #endforeach(DIR)
endif(NOT ENABLE_VCCRUN_ONLY)

if(ENABLE_VCCRUN)
  add_subdirectory(vccrun)
endif(ENABLE_VCCRUN)
