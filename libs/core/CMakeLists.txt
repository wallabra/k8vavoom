if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DVAVOOM_DEBUG_BUILD=1)
endif()

add_library(core STATIC
  mimalloc/mimalloc.h

  propp.h
  args.cpp
  args.h
  array.h
  common.h
  common.cpp
  core.h
  colorutil.h
  colorutil.cpp
  crc.h
  crc.cpp
  endianness.h
  endianness.cpp
  exception.h
  exception.cpp
  hashfunc.h
  jh32.h
  jh32.cpp
  rg32.h
  rg32.cpp
  log.h
  log.cpp
  map_impl.h
  map.h
  matrix.h
  matrix.cpp
  name.h
  names.h
  name.cpp
  str.h
  vstrci.h
  str.cpp
  stream.h
  stream.cpp
  xstreams.h
  xstreams.cpp
  zipstreams.h
  zipstreams.cpp
  mathutil.h
  vector.h
  vector.cpp
  wlist.h
  xml.h
  xml.cpp
  zone.h
  zone.cpp
  minipng.h
  minipng.cpp
  syslow.h
  syslow.cpp
  poly1305-donna.c
  prngs.cpp
  ed25519.c
  chacha20.c
  sha2.c
  md5.h
  md5.cpp
  timsort-impl.h
  timsort.h
  timsort.c
  strtod_plan9.h
  strtod_plan9.c
  xxhash.h
  xxhash.cpp
  ntvalue.cpp
  micropather.h
  micropather.cpp
  cvarsys.h
  cvarsys.cpp

  chachaprng_c.h
  chachaprng_cpp.h

  SpookyV2.h
  SpookyV2.c

  # mimalloc
  mimalloc/mimalloc-atomic.h
  mimalloc/mimalloc-internal.h
  mimalloc/mimalloc-types.h
  mimalloc/mimalloc.h
  mimalloc/static.c

  # lzma decoder
  lzmadec/LzmaDec.h
  lzmadec/LzmaDec.c

  # miniz
  miniz/miniz.h
  miniz/miniz.c

  # fsys (aka VFS)
  fsys/fsys.h
  fsys/fsys.cpp
  fsys/fsys_local.h
  fsys/fsys_pakbase.cpp
  fsys/fsys_vfs.cpp
  # formats
  fsys/formats/fsys_dirpak.cpp
  fsys/formats/fsys_wad.cpp
  fsys/formats/fsys_zip.cpp
  fsys/formats/fsys_qkpak.cpp
  fsys/formats/fsys_dfwad.cpp
  # included directly in "fsys.cpp", otherwise "smart" linker will throw it away
  #fsys/fsys_register.cpp
)
