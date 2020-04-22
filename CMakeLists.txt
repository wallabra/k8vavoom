cmake_minimum_required(VERSION 3.0)
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  cmake_policy(SET CMP0026 NEW)
endif(COMMAND cmake_policy)


project(k8vavoom)


option(WITH_SDL2 "Use SDL2 library" ON)
option(WITH_OPENGL "Use OpenGL library" ON)
option(WITH_MIMALLOC "Use mi-malloc memory allocator" ON)
option(WITH_OPUS "Enable Opus support" ON)
option(WITH_SYS_OPUS "Enable Opus support via system libopus and libopusfile (if off, internal libraries will be used)" OFF)
option(WITH_VORBIS "Enable Vorbis support via libvorbis (if off, stb_vorbis will be used)" OFF)
option(WITH_LIBMAD "Enable MP3 support via libmad (if off, dr_mp3 will be used)" OFF)
option(WITH_FLAC "Enable FLAC support via libflac (if off, dr_flac will be used)" OFF)
option(WITH_FLUID "Enable FluidSynth support" ON)
if(WIN32)
option(WITH_SYS_FLUID "Use system FluidSynth library" OFF)
else(WIN32)
option(WITH_SYS_FLUID "Use system FluidSynth library" ON)
endif(WIN32)

if(WITH_OPENGL)
  option(WITH_GLES "Use hacks for GL4ES (experimental)" OFF)
  option(WITH_GL4ES "Use GL4ES directly (experimental)" OFF)
  if(WITH_GL4ES)
    if(WITH_GLES)
      set(WITH_GL4ES ON)
    endif()
    if(GL4ES_LIBRARY_DIR STREQUAL "")
      message(FATAL_ERROR "GL4ES_LIBRARY_DIR not setted")
    endif()
    if(GL4ES_INCLUDE_DIR STREQUAL "")
      message(FATAL_ERROR "GL4ES_INCLUDE_DIR not setted")
    endif()
  endif()
endif()

option(WITH_STB_JPEG "Use stb_image to load jpegs" ON)
option(WITH_SYS_LIBJPG "Use system libjpg (if stb_image is off)" ON)

option(WITH_OLD_CPU "Use pIV CPU instruction set" OFF)

# default is SSE2
option(WITH_SSE3 "Use SSE3 instruction set (may not compile)" OFF)
option(WITH_SSE4 "Use SSE4 instruction set (may not compile)" OFF)
option(WITH_SSE41 "Use SSE4.1 instruction set (may not compile)" OFF)
option(WITH_SSE42 "Use SSE4.2 instruction set (may not compile)" OFF)
option(NO_SSE "Disable all SSE instructions" OFF)
option(INSANE_AVX "Use AVX2 instruction set (don't do this, it won't give you any noticeable speedup and may be broken)" OFF)


option(WITH_GLAD "Use supplied GLAD for GL bindings" OFF)
option(NO_PTHREAD "Disable pthreads, use C11 threads instead" OFF)
option(NO_MEMRCHR "Disable usage of memrchr()" OFF)
option(NO_RAWTTY "Disable raw TTY control" OFF)

option(WITH_STRTODEX "Use internal strtod implementation to parse floats" ON)

option(DEBUG_FPU_CHECKS "Use FPU and turn on some checks (GNU/Linux only)" OFF)
option(UNSTABLE_OPTIMISATIONS "Use -O3 (WARNING! DON'T DO THAT! NEVER!)" OFF)

option(ARCH_LINUX_SPECIAL "Arch GNU/Linux specific changes (do not turn on if you don't know what it is)" OFF)

if(CMAKE_CROSSCOMPILING AND WIN32)
  #if(MINGW)
  option(WITH_WINE "Use wine to run built executables" ON)
  option(WITH_STATIC_OPENAL "Use special flags to link with static OpenAL" ON)
  #endif(MINGW)
  set(WITH_STRTODEX ON)
endif(CMAKE_CROSSCOMPILING AND WIN32)

if(SWITCH_LIBNX)
  option(WITH_NXLINK "Redirect standard output to NXLink" OFF)
  set(NO_SSE ON) # set it right away for proper logging and cflags
  set(WITH_MIMALLOC OFF)
endif(SWITCH_LIBNX)


# perform some platform and compiler checks
set(K8_UNFUCK_AS_GCC ON)
set(K8_UNFUCK_AS_SHITLANG OFF)
if(NOT CMAKE_COMPILER_IS_GNUCC)
  if (APPLE OR CMAKE_HOST_APPLE OR XCODE_VERSION)
    message(FATAL_ERROR "Apple platform is not supported.")
    return()
  endif(APPLE OR CMAKE_HOST_APPLE OR XCODE_VERSION)

  if (MSVC OR MSVC80 OR MSVC_IDE)
    message(FATAL_ERROR "m$ compilers aren't supported. GTFO.")
    return()
  endif(MSVC OR MSVC80 OR MSVC_IDE)

  message(WARNING "Only GNU C/C++ compilers are supported to build k8vavoom and tools!")
  message(WARNING "Your compiler is not GNU, please, don't report bugs.")
  set(K8_UNFUCK_AS_GCC OFF)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "And, by the way. clang sux.")
    set(K8_UNFUCK_AS_SHITLANG ON)
  endif()
endif(NOT CMAKE_COMPILER_IS_GNUCC)

option(ENABLE_SIGNAL_HANDLER "Intercept system signals" OFF)

option(ENABLE_VCC         "Use vcc to fast syntax check" OFF)

option(ENABLE_CLIENT      "Build client executable" ON)
option(ENABLE_SERVER      "Build dedicated server executable (don't do that!)" OFF)
option(ENABLE_MASTER      "Build master server (no reason to build it)" OFF)
option(ENABLE_QUERY_DEMO  "Build query demo (can be used to send rcon commands)" OFF)
option(ENABLE_VCCRUN      "Build vccrun executable" OFF)
option(ENABLE_MD2FIXER    "Build fixmd2 executable (you don't need it)" OFF)
option(ENABLE_GLVIS       "Build glvis PVS builder executable (you don't need it)" OFF)

option(ENABLE_VCCRUN_ONLY "Build ONLY vccrun" OFF)
option(ENABLE_UTILS_ONLY "Build ONLY utils" OFF)

if(UNIX)
  option(ENABLE_WRAPPERS "Enable creation of wrapper scripts" ON)
endif(UNIX)


#  COMMAND git rev-parse --verify -q --short=16 HEAD

execute_process(
  COMMAND git rev-parse --verify -q HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  ERROR_VARIABLE GIT_ERROR
  RESULT_VARIABLE GET_EXEC_RESULT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT ${GET_EXEC_RESULT} EQUAL 0)
  set(GIT_COMMIT_HASH "UNKNOWN")
else()
  if(NOT ${GIT_ERROR} STREQUAL "")
    set(GIT_COMMIT_HASH "UNKNOWN")
  endif()

  if(${GIT_COMMIT_HASH} STREQUAL "")
    set(GIT_COMMIT_HASH "UNKNOWN")
  endif()
endif()

#message(STATUS "hash=" ${GIT_COMMIT_HASH})
#message(STATUS "error=" ${GIT_ERROR})

#add_definitions("-DVV_GIT_COMMIT_HASH=${GIT_COMMIT_HASH}")


if(ENABLE_UTILS_ONLY)
  set(ENABLE_CLIENT OFF)
  set(ENABLE_SERVER OFF)
  set(ENABLE_VCCRUN OFF)
  set(ENABLE_VCCRUN_ONLY OFF)
elseif(ENABLE_VCCRUN_ONLY)
  #set(WITH_VORBIS OFF)
  #set(WITH_LIBMAD OFF)
  #set(WITH_FLAC OFF)
  #set(WITH_SYS_LIBJPG OFF)
  #set(WITH_SYS_ZLIB OFF)
  set(ENABLE_CLIENT OFF)
  set(ENABLE_SERVER OFF)
  set(ENABLE_MASTER OFF)
  set(ENABLE_QUERY_DEMO OFF)
  set(ENABLE_VCCRUN ON)
  set(ENABLE_MD2FIXER OFF)

  if (NOT WITH_VORBIS OR NOT WITH_FLAC)
    message(SEND_ERROR "OpenAL, Vorbis and Flac are required")
    return()
  endif()
else()
  if(NOT ENABLE_CLIENT)
    if(NOT ENABLE_SERVER)
      message(STATUS "Neither client, nor server was requested")
    else(NOT ENABLE_SERVER)
      message(STATUS "Server-only build, turning off SDL2, OpenGL and OpenAL")
    endif(NOT ENABLE_SERVER)
    set(WITH_SDL2 OFF)
    set(WITH_OPENGL OFF)
    set(WITH_GL4ES OFF)
    set(WITH_VORBIS OFF)
    set(WITH_OPUS OFF)
    set(WITH_LIBMAD OFF)
    set(WITH_FLAC OFF)
  endif(NOT ENABLE_CLIENT)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()


if(ARCH_LINUX_SPECIAL)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14 -lSDL2main -lSDL2")
else(ARCH_LINUX_SPECIAL)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14")
endif(ARCH_LINUX_SPECIAL)

set(UNFUCK_GCC_FLAGS "")
set(UNFUCK_GXX_FLAGS "")

if(K8_UNFUCK_AS_SHITLANG)
 set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-ignored-optimization-argument")
 set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-unused-command-line-argument")
 set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-unknown-warning-option")
endif(K8_UNFUCK_AS_SHITLANG)

if(K8_UNFUCK_AS_GCC)
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-aggressive-loop-optimizations")
endif(K8_UNFUCK_AS_GCC)
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-delete-null-pointer-checks")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-strict-aliasing")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-strict-overflow")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fwrapv")
if(K8_UNFUCK_AS_GCC)
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-diagnostics-show-caret")
endif(K8_UNFUCK_AS_GCC)
if(K8_UNFUCK_AS_GCC AND UNIX AND NOT CMAKE_CROSSCOMPILING)
  # i refuse to upgrade, lol
  if("$ENV{USER}" STREQUAL "ketmar")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wabi=10")
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fabi-version=10")
  endif()
endif(K8_UNFUCK_AS_GCC AND UNIX AND NOT CMAKE_CROSSCOMPILING)

if(SWITCH_LIBNX)
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fsigned-char")
  set(UNFUCK_GXX_FLAGS "${UNFUCK_GXX_FLAGS} -fexceptions")
  set(UNFUCK_GXX_FLAGS "${UNFUCK_GXX_FLAGS} -frtti")
endif(SWITCH_LIBNX)
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wall")
#set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-frame-address")
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
  # k8: fuck you, gcc. i have my own style, gtfo.
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-misleading-indentation")
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-ignored-attributes")
endif()

if(WITH_STRTODEX)
  add_definitions(-DVCORE_ALLOW_STRTODEX=1)
endif(WITH_STRTODEX)

# for mi-malloc
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-invalid-memory-model")

if(ARCH_LINUX_SPECIAL)
  add_definitions(-DVAVOOM_ARCH_LINUX_SPECIAL_SDL=1)
endif(ARCH_LINUX_SPECIAL)


if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.9)
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

if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 9.0)
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-diagnostics-show-line-numbers")
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-diagnostics-show-labels")
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-address-of-packed-member")
endif()

# some math/FPU options
if(NOT DEBUG_FPU_CHECKS AND NOT SWITCH_LIBNX)
  # use -ffast-math, 'cause why not?
  # k8: nope, i command my former self to not do this
  #set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -ffast-math")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
    # on gcc 5.x leads to internal compiler errors
    set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-math-errno")
  endif()
endif(NOT DEBUG_FPU_CHECKS AND NOT SWITCH_LIBNX)
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-unsafe-math-optimizations")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-associative-math")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-reciprocal-math")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-finite-math-only")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fsigned-zeros")
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fsignaling-nans") # do not touch my NaNs!
set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -frounding-math") # get lost!

# force gcc to unroll some loops
set(K8_OPT_GCC_FLAGS "${K8_OPT_GCC_FLAGS} -funroll-loops")
set(K8_OPT_GCC_FLAGS "${K8_OPT_GCC_FLAGS} -fpredictive-commoning")

# and force gcc to not vectrosize (whatever it is)
set(K8_OPT_GCC_FLAGS "${K8_OPT_GCC_FLAGS} -fno-tree-vectorize")
set(K8_OPT_GCC_FLAGS "${K8_OPT_GCC_FLAGS} -fvect-cost-model=cheap")
set(K8_OPT_GCC_FLAGS "${K8_OPT_GCC_FLAGS} -fsimd-cost-model=cheap")

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
  set(NO_RAWTTY ON)
endif(NOT WIN32)


if(ENABLE_SIGNAL_HANDLER)
  set(UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -Wno-frame-address")
endif(ENABLE_SIGNAL_HANDLER)


if(UNSTABLE_OPTIMISATIONS)
  set(VV_OPTIM "-O3")
else(UNSTABLE_OPTIMISATIONS)
  set(VV_OPTIM "-O2")
endif(UNSTABLE_OPTIMISATIONS)

# unfuck all build types

string(REGEX REPLACE "-O[0-9]" " " CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
string(REGEX REPLACE "-O[0-9]" " " CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS} ${UNFUCK_GXX_FLAGS}")

string(REGEX REPLACE "-O[0-9]" " " CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
string(REGEX REPLACE "-O[0-9]" " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} ${VV_OPTIM} ${UNFUCK_GCC_FLAGS} ${K8_OPT_GCC_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${VV_OPTIM} ${UNFUCK_GCC_FLAGS} ${UNFUCK_GXX_FLAGS} ${K8_OPT_GCC_FLAGS}")

string(REGEX REPLACE "-O[0-9]" " " CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
string(REGEX REPLACE "-O[0-9]" " " CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${VV_OPTIM} -g ${UNFUCK_GCC_FLAGS} ${K8_OPT_GCC_FLAGS}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${VV_OPTIM} -g ${UNFUCK_GCC_FLAGS} ${UNFUCK_GXX_FLAGS} ${K8_OPT_GCC_FLAGS}")

string(REGEX REPLACE "-O[0-9]" " " CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
string(REGEX REPLACE "-O[0-9]" " " CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")

set(CMAKE_C_FLAGS_MINSIZEREL   "${CMAKE_C_FLAGS_RELWITHDEBINFO} -Os ${UNFUCK_GCC_FLAGS}")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Os ${UNFUCK_GCC_FLAGS} ${UNFUCK_GXX_FLAGS}")


add_definitions(-DZDBSP_DISABLE_SSE=1)

if(NOT WITH_MIMALLOC)
  add_definitions(-DVAVOOM_DISABLE_MIMALLOC)
  message(STATUS "Using system memory allocator")
endif(NOT WITH_MIMALLOC)

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
  set(DATADIR share/k8vavoom)
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
### find_path(INTTYPES_INCLUDE_DIR inttypes.h)
### if(INTTYPES_INCLUDE_DIR)
###   add_definitions(-DHAVE_INTTYPES_H=1)
### endif(INTTYPES_INCLUDE_DIR)


#
#  **** Check for newtwork libraries ****
#
if(WIN32)
  set(NET_LIBRARIES wsock32)
else(WIN32)
  set(NET_LIBRARIES "")
endif(WIN32)


#
#  **** pkg-config is required ****
#
FIND_PACKAGE(PkgConfig REQUIRED)


#
#  **** Check for zlib ****
#
#if(WITH_SYS_ZLIB)
#  find_package(ZLIB)
#  if(ZLIB_FOUND)
#    include_directories(${ZLIB_INCLUDE_DIR})
#  else(ZLIB_FOUND)
#    message(STATUS "WARNING: Zlib not found, using internal zlib")
#    set(WITH_SYS_ZLIB OFF)
#    set(ZLIB_LIBRARIES z)
#    set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} z)
#  endif(ZLIB_FOUND)
#else(WITH_SYS_ZLIB)
#  add_definitions(-DUSE_INTERNAL_ZLIB=1)
#  add_definitions(-DHAVE_ZLIB_H=1)
#  message(STATUS "Using built-in zlib")
#  set(ZLIB_LIBRARIES "z")
#  set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} z)
#endif(WITH_SYS_ZLIB)
#set(MAIN_LIBS z ${MAIN_LIBS})


#
#  **** Check for libjpeg ****
#
if(ENABLE_CLIENT OR ENABLE_SERVER OR ENABLE_VCCRUN)
  if(WITH_STB_JPEG)
  else(WITH_STB_JPEG)
    add_definitions(-DVAVOOM_DISABLE_STB_IMAGE_JPEG=1)
    if(WITH_SYS_LIBJPG)
      find_package(JPEG)
      if(JPEG_FOUND)
        include_directories(${JPEG_INCLUDE_DIR})
        set(MAIN_LIBS ${JPEG_LIBRARIES} ${MAIN_LIBS})
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
  endif(WITH_STB_JPEG)
endif(ENABLE_CLIENT OR ENABLE_SERVER OR ENABLE_VCCRUN)


#
#  **** Check for liblzma ****
#
#if(WITH_SYS_LZMA)
#  find_package(LibLZMA)
#  if(LIBLZMA_FOUND)
#    include_directories(${LIBLZMA_INCLUDE_DIRS})
#    #message(STATUS "***: " ${LIBLZMA_INCLUDE_DIRS})
#    #message(STATUS "***: " ${LIBLZMA_LIBRARIES})
#    set(MAIN_LIBS ${LIBLZMA_LIBRARIES} ${MAIN_LIBS})
#  else(LIBLZMA_FOUND)
#    message(STATUS "WARNING: LibLzma not found, using internal lzma decoder")
#    set(WITH_SYS_LZMA OFF)
#  endif(LIBLZMA_FOUND)
#endif(WITH_SYS_LZMA)
#
#if(NOT WITH_SYS_LZMA)
#  set(MAIN_LIBS lzma ${MAIN_LIBS})
#  set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} lzma)
#  add_definitions(-DUSE_INTERNAL_LZMA=1)
#endif(NOT WITH_SYS_LZMA)
#
#if(NOT WITH_SYS_LZMA)
#  message(STATUS "LZMA: using internal liblzma")
#  set(LIBLZMA_LIBRARIES "lzma")
#  add_definitions(-DUSE_INTERNAL_LZMA=1)
#endif(NOT WITH_SYS_LZMA)


#
#  **** Check for OpenAL ****
#
if(ENABLE_CLIENT OR ENABLE_VCCRUN)
  find_package(OpenAL REQUIRED)
  include_directories(${OPENAL_INCLUDE_DIR})
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

set(NEED_SYS_OGG OFF)
if(WITH_VORBIS)
  set(NEED_SYS_OGG ON)
endif(WITH_VORBIS)

if(WITH_OPUS AND WITH_SYS_OPUS)
  set(NEED_SYS_OGG ON)
endif(WITH_OPUS AND WITH_SYS_OPUS)

#
#  **** Check for libogg ****
#
if(NEED_SYS_OGG)
  PKG_CHECK_MODULES(OGG ogg)
  if(OGG_FOUND)
    message(STATUS "Found libogg: ${OGG_LIBRARIES}")
    include_directories(${OGG_INCLUDE_DIRS})
    set(MAIN_LIBS ${OGG_LIBRARIES} ${MAIN_LIBS})
  endif(OGG_FOUND)
endif(NEED_SYS_OGG)


#
#  **** Check for Vorbis ****
#
if(WITH_VORBIS)
  PKG_CHECK_MODULES(VORBIS vorbis)
  if(OGG_FOUND AND VORBIS_FOUND)
    message(STATUS "Found Vorbis: ${VORBIS_LIBRARIES}")
    set(VORBIS_FOUND ON)
    include_directories(${VORBIS_INCLUDE_DIRS})
    set(MAIN_LIBS ${VORBIS_LIBRARIES} ${MAIN_LIBS})
  else(OGG_FOUND AND VORBIS_FOUND)
    message(STATUS "Could NOT find OggVorbis libraries")
    set(VORBIS_FOUND OFF)
    message(FATAL_ERROR "You need libvorbis to compile k8vavoom.")
    return()
  endif(OGG_FOUND AND VORBIS_FOUND)
else(WITH_VORBIS)
  set(VORBIS_FOUND OFF)
  message(STATUS "using stb_vorbis for Ogg/Vorbis decoding")
endif(WITH_VORBIS)


#
#  **** Check for Opus ****
#
set(OPUS_USE_INTERNAL OFF)
if(WITH_OPUS)
  if(WITH_SYS_OPUS)
    PKG_CHECK_MODULES(OPUS opusfile opus)
    if(OPUS_FOUND AND OGG_FOUND)
      message(STATUS "Found Opus: ${OPUS_LIBRARIES}")
      set(OPUS_FOUND ON)
      include_directories(${OPUS_INCLUDE_DIRS})
      set(MAIN_LIBS ${OPUS_LIBRARIES} ${MAIN_LIBS})
    else(OPUS_FOUND AND OGG_FOUND)
      message(STATUS "Could NOT find Opus libraries")
      set(OPUS_FOUND OFF)
      message(FATAL_ERROR "You need libopus to compile k8vavoom.")
      return()
    endif(OPUS_FOUND AND OGG_FOUND)
  else(WITH_SYS_OPUS)
    set(OPUS_FOUND OFF)
    set(OPUS_USE_INTERNAL ON)
    include_directories(${CMAKE_SOURCE_DIR}/libs/opus/libopus/include)
    include_directories(${CMAKE_SOURCE_DIR}/libs/opus/libopusfile/include)
    if(OGG_FOUND)
      set(MAIN_LIBS opusfile opus ${MAIN_LIBS})
    else(OGG_FOUND)
      # use built-in libogg
      include_directories(${CMAKE_SOURCE_DIR}/libs/opus/libogg/include)
      set(MAIN_LIBS opusfile opus ogg ${MAIN_LIBS})
      add_definitions(-DVV_BUILTIN_OGG)
    endif(OGG_FOUND)
    add_definitions(-DVV_BUILTIN_OPUS)
  endif(WITH_SYS_OPUS)
else(WITH_OPUS)
  set(OPUS_FOUND OFF)
endif(WITH_OPUS)




if(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY OR ENABLE_VCCRUN)
  set(XMPLITE_FOUND ON)
  set(MAIN_LIBS xmplite ${MAIN_LIBS})
  include_directories(libs/xmplite)
  add_definitions(-DLIBXMP_CORE_PLAYER)
  add_definitions(-DXMP_SYM_VISIBILITY)
endif(ENABLE_CLIENT OR ENABLE_VCCRUN_ONLY OR ENABLE_VCCRUN)


#
#  **** Check for libmad ****
#
if(WITH_LIBMAD)
  find_path(MAD_INCLUDE_DIRS mad.h)
  find_library(MAD_LIBRARIES NAMES mad)
  if(MAD_LIBRARIES)
    message(STATUS "Found LibMAD: ${MAD_LIBRARIES}")
    set(LIBMAD_FOUND ON)
    include_directories(${MAD_INCLUDE_DIRS})
    set(MAIN_LIBS ${MAD_LIBRARIES} ${MAIN_LIBS})
  else(MAD_LIBRARIES)
    message(STATUS "Could NOT find LibMAD libraries")
    set(LIBMAD_FOUND OFF)
  endif(MAD_LIBRARIES)
else(WITH_LIBMAD)
  set(LIBMAD_FOUND OFF)
  message(STATUS "using dr_mp3 for MP3 decoding")
endif(WITH_LIBMAD)


#
#  **** Check for FLAC ****
#
if(WITH_FLAC)
  PKG_CHECK_MODULES(FLAC flac)
  if(FLAC_FOUND)
    message(STATUS "Found FLAC: ${FLAC_LIBRARIES}")
    include_directories(${FLAC_INCLUDE_DIRS})
    set(MAIN_LIBS ${FLAC_LIBRARIES} ${MAIN_LIBS})
  else(FLAC_FOUND)
    message(STATUS "Could NOT find FLAC library")
    set(FLAC_FOUND OFF)
  endif(FLAC_FOUND)
else(WITH_FLAC)
  set(FLAC_FOUND OFF)
  message(STATUS "using dr_flac for FLAC decoding")
endif(WITH_FLAC)


#
#  **** Check for FluidSynth ****
#
if(WITH_FLUID AND ENABLE_CLIENT)
  if(WITH_SYS_FLUID)
    PKG_CHECK_MODULES(FLUID fluidsynth)
    #message(STATUS, "FINC:" ${FLUID_INCLUDE_DIRS})
    #message(STATUS, "FLIB:" ${FLUID_LIBRARIES})
    if(WIN32 AND FLUID_FOUND)
      PKG_CHECK_MODULES(GLIB glib-2.0)
      if(GLIB_FOUND)
        include_directories(${GLIB_INCLUDE_DIRS})
        set(MAIN_LIBS ${GLIB_LIBRARIES} ${MAIN_LIBS})
      else(GLIB_FOUND)
        set(FLUID_FOUND OFF)
      endif(GLIB_FOUND)
    endif(WIN32 AND FLUID_FOUND)
    if(FLUID_FOUND)
      message(STATUS "Found FluidSynth: ${FLUID_LIBRARIES}")
      set(FLUID_FOUND ON)
      if(SWITCH_LIBNX)
        # catch all possible fluid installs (only one dir in there anyway)
        include_directories(${FLUID_INCLUDE_DIRS})
        include_directories(${FLUID_INCLUDE_DIRS}/fluidsynth)
      else(SWITCH_LIBNX)
        include_directories(${FLUID_INCLUDE_DIRS})
      endif(SWITCH_LIBNX)
      set(MAIN_LIBS ${FLUID_LIBRARIES} ${MAIN_LIBS})
    else(FLUID_INCLUDE_DIRS AND FLUID_LIBRARIES)
      message(STATUS "Could NOT find FluidSynth libraries")
      if(NOT WIN32)
        message(STATUS "Using built-in FluidSynth")
        set(WITH_SYS_FLUID OFF)
        set(FLUID_FOUND ON)
      endif(NOT WIN32)
    endif(FLUID_FOUND)
  else(WITH_SYS_FLUID)
    message(STATUS "Using built-in FluidSynth")
    set(FLUID_FOUND ON)
  endif(WITH_SYS_FLUID)
else(WITH_FLUID)
  set(FLUID_FOUND OFF)
endif(WITH_FLUID AND ENABLE_CLIENT)

if(FLUID_FOUND AND NOT WITH_SYS_FLUID AND ENABLE_CLIENT)
  add_definitions(-DBUILTIN_FLUID)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/fluidsynth_lite/include)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/libs/fluidsynth_lite/include)
  set(MAIN_LIBS libfluidsynth_lite ${MAIN_LIBS})
endif(FLUID_FOUND AND NOT WITH_SYS_FLUID AND ENABLE_CLIENT)


#
#  **** Platform specific library checks ****
#
#if(NOT WIN32)
  # ---------------- Other *NIX platforms ----------------
  if(WITH_SDL2)
    if(CMAKE_CROSSCOMPILING AND ANDROID)
      if(SDL2_INCLUDE_DIRS STREQUAL "")
        message(FATAL_ERROR "SDL2_INCLUDE_DIRS not specified")
      endif()
      if(SDL2_LIBRARIES STREQUAL "")
        message(FATAL_ERROR "SDL2_LIBRARIES not specified")
      endif()
      set(SDL2_FOUND ON)
    else()
      find_package(SDL2 REQUIRED)
      #PKG_SEARCH_MODULE(SDL2 REQUIRED sdl2)
    endif()
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
    elseif(WITH_GL4ES)
      set(OPENGL_gl_LIBRARY "-L${GL4ES_LIBRARY_DIR} -lGL")
      set(OPENGL_INCLUDE_DIR "${GL4ES_INCLUDE_DIR}")
      set(OPENGL_FOUND ON)
    else()
      find_package(OpenGL REQUIRED)
    endif()
    if(OPENGL_FOUND)
      message(STATUS "Found OpenGL")
      include_directories(${OPENGL_INCLUDE_DIR})
      if(SWITCH_LIBNX OR WITH_GL4ES)
        # have to reorder this shit or else it doesn't link
        set(MAIN_LIBS ${MAIN_LIBS} ${OPENGL_gl_LIBRARY})
      else()
        set(MAIN_LIBS ${OPENGL_gl_LIBRARY} ${MAIN_LIBS})
      endif()
    endif(OPENGL_FOUND)
  else(WITH_OPENGL)
    set(OPENGL_FOUND OFF)
  endif(WITH_OPENGL)
#endif(NOT WIN32)

if(WITH_OPENGL)
  if(WITH_GLES)
    add_definitions(-DGL4ES_HACKS)
  endif()
  if(WITH_GL4ES)
    add_definitions(-DGL4ES_NO_CONSTRUCTOR)
  endif()
endif()

if(ANDROID)
  add_definitions(-UDEBUG)
  set(MAIN_LIBS log android ${MAIN_LIBS})
endif()

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
  # enable necessary switches
  set(NO_SSE ON)
  set(NO_SIGNALS ON)
  set(NO_SYSCONF ON)
  set(NO_SINCOS ON)
  set(NO_MEMRCHR ON)
  set(NO_RAWTTY ON)
  set(WITH_MIMALLOC OFF)
  # turn on nxlink if necessary
  if(WITH_NXLINK)
    add_definitions(-DSWITCH_NXLINK=1)
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

  if(ENABLE_VCCRUN)
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
  endif(ENABLE_VCCRUN)
endif(SWITCH_LIBNX)


if(NO_SSE)
  add_definitions(-DNO_SSE=1)
endif(NO_SSE)

if(NO_PTHREAD)
  add_definitions(-DNO_PTHREAD=1)
endif(NO_PTHREAD)

if(NO_SINCOS)
  add_definitions(-DNO_SINCOS=1)
endif(NO_SINCOS)

if(NO_SIGNALS)
  add_definitions(-DNO_SIGNALS=1)
endif(NO_SIGNALS)

if(NO_SYSCONF)
  add_definitions(-DNO_SYSCONF=1)
endif(NO_SYSCONF)

if(NO_MEMRCHR)
  add_definitions(-DNO_MEMRCHR=1)
endif(NO_MEMRCHR)

if(NO_RAWTTY)
  add_definitions(-DNO_RAWTTY=1)
endif(NO_RAWTTY)

### if(CMAKE_EXECUTABLE_FORMAT STREQUAL "ELF")
###   add_subdirectory(libs/libbacktrace)
###   #include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libbacktrace)
###   set(MAIN_LIBS backtrace ${MAIN_LIBS})
###   add_definitions(-DHAVE_BACKTRACE=1)
### endif()


message(STATUS "=== BUILD OPTIONS ===")
message(STATUS "BUILD: ${CMAKE_BUILD_TYPE}")
#message(STATUS "cflags  : ${CMAKE_C_FLAGS}")
#message(STATUS "cxxflags: ${CMAKE_CXX_FLAGS}")

message(STATUS "=== COMPONENTS ===")
message(STATUS "  Vavoom Client: " ${ENABLE_CLIENT})
message(STATUS "  Vavoom Server: " ${ENABLE_SERVER})
message(STATUS "  Vavoom Master: " ${ENABLE_MASTER})
message(STATUS "  Vavoom Query : " ${ENABLE_QUERY_DEMO})
message(STATUS "  VccRun       : " ${ENABLE_VCCRUN})
message(STATUS "  strtodEx     : " ${WITH_STRTODEX})
if(ENABLE_VCCRUN)
  if(GNUTLS_FOUND)
    message(STATUS "  GNUTLS       : ON")
  endif(GNUTLS_FOUND)
endif(ENABLE_VCCRUN)
if(ARCH_LINUX_SPECIAL)
  message(STATUS "Arch Sepcific Build Mode")
endif(ARCH_LINUX_SPECIAL)

add_subdirectory(libs)

if(ENABLE_UTILS_ONLY)
  add_subdirectory(utils)
elseif(NOT ENABLE_VCCRUN_ONLY)
  if(NOT CMAKE_CROSSCOMPILING OR WIN32)
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
endif()

if(ENABLE_VCCRUN)
  add_subdirectory(vccrun)
endif(ENABLE_VCCRUN)
