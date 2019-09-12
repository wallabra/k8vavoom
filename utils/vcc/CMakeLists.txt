#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Og -O3" )
#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og -O3" )

add_executable(vcc
  vcc.cpp
  vcc.h
  vc_api_vcc.h
  vc_api_vcc.cpp
  vc_package_vcc.cpp
)
set_target_properties(vcc PROPERTIES OUTPUT_NAME ../bin/vcc)
#set_target_properties(vcc PROPERTIES COMPILE_FLAGS IN_VCC)
target_compile_definitions(vcc PRIVATE IN_VCC)
target_link_libraries(vcc vavoomc core ${VAVOOM_SHITDOZE_LIBS})
add_dependencies(vcc core vavoomc)
add_definitions(-DIN_VCC)
