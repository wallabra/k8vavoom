include_directories(src src/loaders)
add_definitions(-DLIBXMP_CORE_PLAYER)
add_definitions(-DXMP_SYM_VISIBILITY)
add_definitions(-DHAVE_ROUND)
add_definitions(-DHAVE_POWF)
if(NOT WIN32)
  add_definitions(-DHAVE_LOCALTIME_R)
endif(NOT WIN32)

set(XMPLITE_C_FLAGS "-Wno-shadow=local -Wno-stringop-truncation")

set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} ${XMPLITE_C_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} ${XMPLITE_C_FLAGS}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${XMPLITE_C_FLAGS}")
set(CMAKE_C_FLAGS_MINSIZEREL   "${CMAKE_C_FLAGS_MINSIZEREL} ${XMPLITE_C_FLAGS}")


add_library(xmplite STATIC
  xmp.h
  src/common.h
  src/control.c
  src/dataio.c
  src/effects.c
  src/effects.h
  src/filter.c
  src/format.c
  src/format.h
  src/hio.c
  src/hio.h
  src/lfo.c
  src/lfo.h
  src/list.h
  src/load.c
  src/load_helpers.c
  src/mdataio.h
  src/memio.c
  src/memio.h
  src/mix_all.c
  src/mixer.c
  src/mixer.h
  src/period.c
  src/period.h
  src/player.c
  src/player.h
  src/precomp_lut.h
  src/read_event.c
  src/scan.c
  src/smix.c
  src/tempfile.h
  src/virtual.c
  src/virtual.h
  src/win32.c
  src/loaders/common.c
  src/loaders/it.h
  src/loaders/it_load.c
  src/loaders/itsex.c
  src/loaders/loader.h
  src/loaders/mod.h
  src/loaders/mod_load.c
  src/loaders/s3m.h
  src/loaders/s3m_load.c
  src/loaders/sample.c
  src/loaders/xm.h
  src/loaders/xm_load.c
)
