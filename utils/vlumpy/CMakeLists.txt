include_directories(../common ${ZLIB_INCLUDE_DIR})

add_executable(vlumpy
  ../common/cmdlib.cpp
  ../common/cmdlib.h
  ../common/ftexdefs.h
  ../common/fwaddefs.h
  ../common/imglib.cpp
  ../common/imglib.h
  ../common/scrlib.cpp
  ../common/scrlib.h
  ../common/wadlib.cpp
  ../common/wadlib.h

  zopfli/blocksplitter.c
  zopfli/blocksplitter.h
  zopfli/cache.c
  zopfli/cache.h
  zopfli/deflate.c
  zopfli/deflate.h
  zopfli/gzip_container.c
  zopfli/gzip_container.h
  zopfli/hash.c
  zopfli/hash.h
  zopfli/katajainen.c
  zopfli/katajainen.h
  zopfli/lz77.c
  zopfli/lz77.h
  zopfli/squeeze.c
  zopfli/squeeze.h
  zopfli/symbols.h
  zopfli/tree.c
  zopfli/tree.h
  zopfli/util.c
  zopfli/util.h
  zopfli/zlib_container.c
  zopfli/zlib_container.h
  zopfli/zopfli.h
  zopfli/zopfli_lib.c

  ioapi.cpp
  ioapi.h
  vlumpy.cpp
  zip.cpp
  zip.h
)
target_link_libraries(vlumpy core ${ZLIB_LIBRARIES} ${VAVOOM_SHITDOZE_LIBS})
set_target_properties(vlumpy PROPERTIES OUTPUT_NAME ../bin/vlumpy)
#add_dependencies(vlumpy core ${LIBLZMA_LIBRARIES} ${ZLIB_DEPS})
add_dependencies(vlumpy core ${ZLIB_DEPS})
