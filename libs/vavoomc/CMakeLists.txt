if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DVAVOOM_DEBUG_BUILD=1)
endif()

add_library(vavoomc STATIC
  vc_local.h
  vc_public.h
  vc_progs.h
  vc_progdefs.h

  vc_location.h
  vc_location.cpp
  vc_error.h
  vc_error.cpp
  vc_type.h
  vc_type.cpp
  vc_member.h
  vc_member.cpp
  vc_constant.h
  vc_constant.cpp
  vc_property.h
  vc_property.cpp
  vc_state.h
  vc_state.cpp
  vc_field.h
  vc_field.cpp
  vc_method.h
  vc_method.cpp
  vc_struct.h
  vc_struct.cpp
  vc_class.h
  vc_class.cpp
  vc_object.h
  vc_object.cpp
  vc_package.h
  vc_package.cpp
  #vc_decorate.h
  #vc_decorate.cpp
  #vc_dehacked.h
  #vc_dehacked.cpp

  vc_emit_context.h
  vc_emit_context.cpp
  vc_expr_base.h
  vc_expr_base.cpp

  vc_expr_array.h
  vc_expr_array.cpp
  vc_expr_assign.h
  vc_expr_assign.cpp
  vc_expr_cast.h
  vc_expr_cast.cpp
  vc_expr_field.h
  vc_expr_field.cpp
  vc_expr_invoke.h
  vc_expr_invoke.cpp
  vc_expr_literal.h
  vc_expr_literal.cpp
  vc_expr_local.h
  vc_expr_local.cpp
  vc_expr_misc.h
  vc_expr_misc.cpp
  vc_expr_type.h
  vc_expr_type.cpp
  vc_expr_unary_binary.h
  vc_expr_unary_binary.cpp
  vc_statement.h
  vc_statement.cpp

  vc_lexer.h
  vc_lexer_tokens.h
  vc_lexer.cpp
  vc_modifiers.h
  vc_modifiers.cpp
  vc_parser.h
  vc_parser.cpp

  # included in "vc_api_common.cpp"
  #vc_zastar.h
  #vc_zastar.cpp

  vc_api_common.cpp
  vc_executor.cpp
)
