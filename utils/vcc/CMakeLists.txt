set(VC_COMPILER_SOURCES
  ../../source/common.h

  ../../libs/vavoomc/vc_local.h
  ../../libs/vavoomc/vc_public.h
  ../../libs/vavoomc/vc_progs.h
  ../../libs/vavoomc/vc_progdefs.h

  ../../libs/vavoomc/vc_location.h
  ../../libs/vavoomc/vc_location.cpp
  ../../libs/vavoomc/vc_error.h
  ../../libs/vavoomc/vc_error.cpp
  ../../libs/vavoomc/vc_type.h
  ../../libs/vavoomc/vc_type.cpp
  ../../libs/vavoomc/vc_member.h
  ../../libs/vavoomc/vc_member.cpp
  ../../libs/vavoomc/vc_constant.h
  ../../libs/vavoomc/vc_constant.cpp
  ../../libs/vavoomc/vc_property.h
  ../../libs/vavoomc/vc_property.cpp
  ../../libs/vavoomc/vc_state.h
  ../../libs/vavoomc/vc_state.cpp
  ../../libs/vavoomc/vc_field.h
  ../../libs/vavoomc/vc_field.cpp
  ../../libs/vavoomc/vc_method.h
  ../../libs/vavoomc/vc_method.cpp
  ../../libs/vavoomc/vc_struct.h
  ../../libs/vavoomc/vc_struct.cpp
  ../../libs/vavoomc/vc_class.h
  ../../libs/vavoomc/vc_class.cpp
  ../../libs/vavoomc/vc_object.h
  ../../libs/vavoomc/vc_object.cpp
  ../../libs/vavoomc/vc_package.h
  ../../libs/vavoomc/vc_package.cpp

  ../../libs/vavoomc/vc_emit_context.h
  ../../libs/vavoomc/vc_emit_context.cpp
  ../../libs/vavoomc/vc_expr_base.h
  ../../libs/vavoomc/vc_expr_base.cpp

  ../../libs/vavoomc/vc_expr_array.h
  ../../libs/vavoomc/vc_expr_array.cpp
  ../../libs/vavoomc/vc_expr_assign.h
  ../../libs/vavoomc/vc_expr_assign.cpp
  ../../libs/vavoomc/vc_expr_cast.h
  ../../libs/vavoomc/vc_expr_cast.cpp
  ../../libs/vavoomc/vc_expr_field.h
  ../../libs/vavoomc/vc_expr_field.cpp
  ../../libs/vavoomc/vc_expr_invoke.h
  ../../libs/vavoomc/vc_expr_invoke.cpp
  ../../libs/vavoomc/vc_expr_literal.h
  ../../libs/vavoomc/vc_expr_literal.cpp
  ../../libs/vavoomc/vc_expr_local.h
  ../../libs/vavoomc/vc_expr_local.cpp
  ../../libs/vavoomc/vc_expr_misc.h
  ../../libs/vavoomc/vc_expr_misc.cpp
  ../../libs/vavoomc/vc_expr_type.h
  ../../libs/vavoomc/vc_expr_type.cpp
  ../../libs/vavoomc/vc_expr_unary_binary.h
  ../../libs/vavoomc/vc_expr_unary_binary.cpp
  ../../libs/vavoomc/vc_statement.h
  ../../libs/vavoomc/vc_statement.cpp

  ../../libs/vavoomc/vc_lexer.h
  ../../libs/vavoomc/vc_lexer_tokens.h
  ../../libs/vavoomc/vc_lexer.cpp
  ../../libs/vavoomc/vc_modifiers.h
  ../../libs/vavoomc/vc_modifiers.cpp
  ../../libs/vavoomc/vc_parser.h
  ../../libs/vavoomc/vc_parser.cpp
)

#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Og -O3" )
#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og -O3" )

add_executable(vcc
  ${VC_COMPILER_SOURCES}
  vcc.cpp
  vcc.h
  vc_api_vcc.h
  vc_api_vcc.cpp
)
set_target_properties(vcc PROPERTIES OUTPUT_NAME ../bin/vcc)
#set_target_properties(vcc PROPERTIES COMPILE_FLAGS IN_VCC)
target_compile_definitions(vcc PRIVATE IN_VCC)
target_link_libraries(vcc core ${VAVOOM_SHITDOZE_LIBS})
add_dependencies(vcc core vavoomc)
add_definitions(-DIN_VCC)
