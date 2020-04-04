if(ENABLE_QUERY_DEMO)
  add_executable(k8vavoom-query
    src/main.cpp
    src/netchan.h
    src/netchan.cpp
  )
  set_target_properties(k8vavoom-query PROPERTIES OUTPUT_NAME ../../k8vavoom-query)
  if(CYGWIN OR MINGW)
    set_target_properties(k8vavoom-query PROPERTIES LINK_FLAGS "-Wl,--subsystem,console")
  endif()
  target_link_libraries(k8vavoom-query ${NET_LIBRARIES})
endif(ENABLE_QUERY_DEMO)
