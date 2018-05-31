include_directories(${ZLIB_INCLUDE_DIR})
add_definitions(-DGLBSP_PLUGIN -DUNIX -DINLINE_G=inline)

add_library(glbsp
  analyze.c
  analyze.h
  blockmap.c
  blockmap.h
  glbsp.c
  glbsp.h
  level.c
  level.h
  node.c
  node.h
  reject.c
  reject.h
  seg.c
  seg.h
  structs.h
  system.c
  system.h
  util.c
  util.h
  wad.c
  wad.h
)
target_link_libraries(glbsp ${ZLIB_LIBRARIES})
if(ZLIB_DEPS)
  add_dependencies(glbsp ${ZLIB_DEPS})
endif(ZLIB_DEPS)
