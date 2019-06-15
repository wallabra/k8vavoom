if(ENABLE_MASTER)
  include_directories(../common)

  add_executable(k8vavoom-master
    main.cpp
  )
  set_target_properties(k8vavoom-master PROPERTIES OUTPUT_NAME ../../k8vavoom-master)
  target_link_libraries(k8vavoom-master core ${NET_LIBRARIES})
  add_dependencies(k8vavoom-master core)
endif(ENABLE_MASTER)
