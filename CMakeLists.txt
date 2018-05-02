project(vavoom)

IF( NOT CMAKE_BUILD_TYPE )
  SET( CMAKE_BUILD_TYPE Debug CACHE STRING
    "Choose the type of build, options are: Debug Release."
  FORCE )
ENDIF()

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++98" )

set( UNFUCK_GCC_FLAGS "" )

set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-aggressive-loop-optimizations" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-delete-null-pointer-checks" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-strict-aliasing" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fno-strict-overflow" )
set( UNFUCK_GCC_FLAGS "${UNFUCK_GCC_FLAGS} -fwrapv" )

set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS}" )
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${UNFUCK_GCC_FLAGS}" )

set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 ${UNFUCK_GCC_FLAGS}" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 ${UNFUCK_GCC_FLAGS}" )

## message(STATUS "BUILD: ${CMAKE_BUILD_TYPE}")
## message(STATUS "cflags  : ${CMAKE_C_FLAGS}")
## message(STATUS "cxxflags: ${CMAKE_CXX_FLAGS}")
## message(STATUS "cflags  (dbg): ${CMAKE_C_FLAGS_DEBUG}")
## message(STATUS "cxxflags(dbg): ${CMAKE_CXX_FLAGS_DEBUG}")

option(ENABLE_ZONE_DEBUG "Enable memory allocation debuging")

cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if(NOT DEFINED DATADIR)
	set(DATADIR share/vavoom)
endif(NOT DEFINED DATADIR)
if(NOT DEFINED BINDIR)
	set(BINDIR bin)
endif(NOT DEFINED BINDIR)

#
#  **** Add debug / development flags ****
#

if(ENABLE_ZONE_DEBUG)
	add_definitions(-DZONE_DEBUG=1)
endif(ENABLE_ZONE_DEBUG)

#
#  **** Check for zlib ****
#

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

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
#	AC_SEARCH_LIBS([recvfrom], [socket], [], [AC_ERROR([Library containing recvfrom not found])])
#	AC_SEARCH_LIBS([gethostbyname], [nsl], [], [AC_ERROR([Library containing gethostbyname not found])])
endif(WIN32)

add_subdirectory(libs)
add_subdirectory(utils)
add_subdirectory(source)
add_subdirectory(progs)
add_subdirectory(basev)
foreach(DIR ${EXTRA_SUBDIRS})
	add_subdirectory(${DIR})
endforeach(DIR)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/karena)
	add_subdirectory(karena)
endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/karena)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/koraxrpg)
	add_subdirectory(koraxrpg)
endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/koraxrpg)
