add_definitions(-DHAVE_SYS_TYPES_H)
add_definitions(-DHAVE_STDINT_H)
add_definitions(-DHAVE_STDDEF_H)
add_definitions(-DHAVE_UNISTD_H)
#add_definitions(-DZ_HAVE_UNISTD_H)
#add_definitions(-D_LARGEFILE64_SOURCE=1)
#add_definitions(-DNO_FSEEKO)

add_library(z STATIC
  # public headers
  zconf.h
  zlib.h

  # private headers
  crc32.h
  deflate.h
  #gzguts.h
  inffast.h
  inffixed.h
  inflate.h
  inftrees.h
  trees.h
  zutil.h

  # sources
  adler32.c
  compress.c
  crc32.c
  deflate.c
  #gzclose.c
  #gzlib.c
  #gzread.c
  #gzwrite.c
  inflate.c
  infback.c
  inftrees.c
  inffast.c
  trees.c
  uncompr.c
  zutil.c
  #win32/zlib1.rc
)
