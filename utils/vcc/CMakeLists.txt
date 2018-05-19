set(VC_COMPILER_SOURCES
  ../../source/common.h
  ../../source/vc/vc_class.cpp
  ../../source/vc/vc_class.h
  ../../source/vc/vc_constant.cpp
  ../../source/vc/vc_constant.h
  ../../source/vc/vc_emit_context.cpp
  ../../source/vc/vc_emit_context.h
  ../../source/vc/vc_error.cpp
  ../../source/vc/vc_error.h
  ../../source/vc/vc_expr_array.cpp
  ../../source/vc/vc_expr_array.h
  ../../source/vc/vc_expr_assign.cpp
  ../../source/vc/vc_expr_assign.h
  ../../source/vc/vc_expr_base.cpp
  ../../source/vc/vc_expr_base.h
  ../../source/vc/vc_expr_cast.cpp
  ../../source/vc/vc_expr_cast.h
  ../../source/vc/vc_expr_field.cpp
  ../../source/vc/vc_expr_field.h
  ../../source/vc/vc_expr_invoke.cpp
  ../../source/vc/vc_expr_invoke.h
  ../../source/vc/vc_expr_literal.cpp
  ../../source/vc/vc_expr_literal.h
  ../../source/vc/vc_expr_local.cpp
  ../../source/vc/vc_expr_local.h
  ../../source/vc/vc_expr_misc.cpp
  ../../source/vc/vc_expr_misc.h
  ../../source/vc/vc_expr_type.cpp
  ../../source/vc/vc_expr_type.h
  ../../source/vc/vc_expr_unary_binary.cpp
  ../../source/vc/vc_expr_unary_binary.h
  ../../source/vc/vc_field.cpp
  ../../source/vc/vc_field.h
  ../../source/vc/vc_lexer.cpp
  ../../source/vc/vc_lexer.h
  ../../source/vc/vc_local.h
  ../../source/vc/vc_location.cpp
  ../../source/vc/vc_location.h
  ../../source/vc/vc_member.cpp
  ../../source/vc/vc_member.h
  ../../source/vc/vc_method.cpp
  ../../source/vc/vc_method.h
  ../../source/vc/vc_modifiers.cpp
  ../../source/vc/vc_modifiers.h
  ../../source/vc/vc_package.cpp
  ../../source/vc/vc_package.h
  ../../source/vc/vc_parser.cpp
  ../../source/vc/vc_parser.h
  ../../source/vc/vc_property.cpp
  ../../source/vc/vc_property.h
  ../../source/vc/vc_state.cpp
  ../../source/vc/vc_state.h
  ../../source/vc/vc_statement.cpp
  ../../source/vc/vc_statement.h
  ../../source/vc/vc_struct.cpp
  ../../source/vc/vc_struct.h
  ../../source/vc/vc_type.cpp
  ../../source/vc/vc_type.h
)

add_executable(vcc
  ${VC_COMPILER_SOURCES}
  vcc.cpp
  vcc.h
)
set_target_properties(vcc PROPERTIES OUTPUT_NAME ../bin/vcc)
set_target_properties(vcc PROPERTIES COMPILE_FLAGS -DIN_VCC)
target_link_libraries(vcc core)
add_dependencies(vcc core)


if (ENABLE_VCCRUN)
  add_executable(vcc_run
    ${VC_COMPILER_SOURCES}
    ../../source/maths.h
    ../../source/maths.cpp
    ../../source/pr_cmds.cpp
    ../../source/pr_exec.cpp
    ../../source/vc/vc_object.cpp
    ../../source/scripts.h
    ../../source/sc_man.cpp
    vcc_run.cpp
    vcc_run.h
  )
  set_target_properties(vcc_run PROPERTIES OUTPUT_NAME ../../vcc_run)
  set_target_properties(vcc_run PROPERTIES COMPILE_FLAGS -DVCC_STANDALONE_EXECUTOR)
  target_link_libraries(vcc_run core)
  add_dependencies(vcc_run core)
endif(ENABLE_VCCRUN)
