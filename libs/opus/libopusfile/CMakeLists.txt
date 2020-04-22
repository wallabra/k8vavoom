#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-sign-compare -Wno-unused-but-set-variable -Wno-unused-function" )
#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare -Wno-unused-but-set-variable -Wno-unused-function" )

add_library(opusfile STATIC
  include/opusfile.h
  #src/http.c
  src/info.c
  src/internal.c
  src/internal.h
  src/opusfile.c
  src/stream.c
  #src/wincerts.c
  #src/winerrno.h
)
