if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DVAVOOM_DEBUG_BUILD=1)
endif()

if(WITH_MIMALLOC)
  set(CORELIB_MIMALLOC_SOURCES
    mimalloc/mimalloc.h
    mimalloc/mimalloc-atomic.h
    mimalloc/mimalloc-internal.h
    mimalloc/mimalloc-types.h
    mimalloc/mimalloc.h
    mimalloc/static.c
  )
endif()

add_library(core STATIC
  propp.h
  args.cpp
  args.h
  array.h
  common.h
  common.cpp
  core.h
  colorutil.h
  colorutil.cpp
  endianness.h
  endianness.cpp
  exception.h
  exception.cpp
  log.h
  log.cpp
  map_impl.h
  map.h
  name.h
  names.h
  name.cpp
  ryu.h
  ryu_d2s.c
  ryu_f2s.c
  str.h
  vstrci.h
  str.cpp
  stream/stream.h
  stream/stream.cpp
  stream/streamsbit.h
  stream/streamsbit.cpp
  stream/streamsex.h
  stream/streamsex.cpp
  stream/streamszip.h
  stream/streamszip.cpp
  stream/ntvalue.cpp
  mathutil.h
  vecmat/vector.h
  vecmat/vector.cpp
  vecmat/matrix.h
  vecmat/matrix.cpp
  wlist.h
  xml.h
  xml.cpp
  zone.h
  zone.cpp
  minipng.h
  minipng.cpp
  syslow.h
  syslow.cpp
  prngs.cpp
  timsort-impl.h
  timsort.h
  timsort.c
  strtod_plan9.h
  strtod_plan9.c
  micropather.h
  micropather.cpp
  cvarsys.h
  cvarsys.cpp

  rawtty.cpp


  hash/hashfunc.h
  hash/crc.h
  hash/crc.cpp
  hash/SpookyV2.h
  hash/SpookyV2.c
  hash/xxhash.h
  hash/xxhash.cpp
  hash/rg32.h
  hash/rg32.cpp
  hash/md5.h
  hash/md5.cpp

  crypto/prng_randombytes.h
  crypto/prng_randombytes.c
  crypto/sha2.h
  crypto/sha2.c
  crypto/hmac_sha2.h
  crypto/hmac_sha2.c
  crypto/chacha20.h
  crypto/chacha20.c
  crypto/poly1305-donna.h
  crypto/poly1305-donna.c
  crypto/curve25519-donna.h
  crypto/curve25519-donna.c
  crypto/ed25519.h
  crypto/ed25519.c
  crypto/chachaprng_c.h
  crypto/chachaprng_cpp.h
  crypto/jh32.h
  crypto/jh32.cpp

  # mimalloc
  ${CORELIB_MIMALLOC_SOURCES}

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
