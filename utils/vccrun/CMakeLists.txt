if (ENABLE_VCCRUN)
  # without client, we'll have no gfx and sound libraries
  set(VCCRUN_DEPS "")
  set(VCCRUN_LINKLIBS "imago")

  if(NOT WITH_SYS_ZLIB)
    set(VCCRUN_DEPS "z" ${VCCRUN_DEPS})
    set(ZLIB_LIBRARIES "z")
  endif(WITH_SYS_ZLIB)

  if(NOT WITH_SYS_LZMA)
    set(VCCRUN_DEPS "lzma" ${VCCRUN_DEPS})
  endif(NOT WITH_SYS_LZMA)

  set(VCCRUN_LINKLIBS ${ZLIB_LIBRARIES} ${VCCRUN_LINKLIBS})
  set(VCCRUN_SOURCES "")
  set(VCCRUN_DEFINES VCC_STANDALONE_EXECUTOR)

  if(ENABLE_CLIENT)
    set(VCCRUN_LINKLIBS "jpeg" ${VCCRUN_LINKLIBS})
    if(NOT WITH_JPG)
      set(VCCRUN_DEPS "jpeg" ${VCCRUN_DEPS})
    endif(NOT WITH_JPG)

    #if(WIN32)
    #  message(SEND_ERROR "NO SHITDOZE YET")
    #  return()
    #else(WIN32)
      if(WITH_SDL2)
        find_package(SDL2 REQUIRED)
      endif(WITH_SDL2)

      if(SDL2_FOUND)
        message(STATUS "VCCRUN: SDL2 found")
        include_directories(${SDL2_INCLUDE_DIRS})
        set(VCCRUN_LINKLIBS ${SDL2_LIBRARIES} ${VCCRUN_LINKLIBS})
        if(WIN32)
          set(VCCRUN_LINKLIBS ${VCCRUN_LINKLIBS} -limm32)
          set(VCCRUN_LINKLIBS ${VCCRUN_LINKLIBS} -lwinmm)
          set(VCCRUN_LINKLIBS ${VCCRUN_LINKLIBS} -lversion)
        endif(WIN32)
        set(VCCRUN_DEFINES "VCCRUN_HAS_SDL" ${VCCRUN_DEFINES})
      endif(SDL2_FOUND)

      if(WITH_OPENGL)
        if(NOT SDL2_FOUND)
          message(SEND_ERROR "SDL2 required for OpenGL")
          return()
        endif(NOT SDL2_FOUND)
        find_package(OpenGL)
        if(OPENGL_FOUND)
          #message(STATUS "VCCRUN: OpenGL found")
          include_directories(${OPENGL_INCLUDE_DIR})
          set(VCCRUN_LINKLIBS ${OPENGL_gl_LIBRARY} ${VCCRUN_LINKLIBS})
          set(VCCRUN_DEFINES "VCCRUN_HAS_OPENGL" ${VCCRUN_DEFINES})
        endif(OPENGL_FOUND)
      endif(WITH_OPENGL)

      if(WITH_OPENAL)
        find_package(OpenAL)
        if(OPENAL_FOUND)
          include_directories(${OPENAL_INCLUDE_DIR})
          set(VCCRUN_LINKLIBS ${OPENAL_LIBRARY} ${VCCRUN_LINKLIBS})
          set(VCCRUN_DEFINES "VCCRUN_HAS_OPENAL" ${VCCRUN_DEFINES})
        endif(OPENAL_FOUND)
      endif(WITH_OPENAL)
    #endif(WIN32)
  endif(ENABLE_CLIENT)

  set(VCCRUN_COMPILER_SOURCES
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

  set(VCCRUN_RUNTIME_SOURCES
    ../../source/maths.h
    ../../source/maths.cpp
    ../../source/pr_cmds.cpp
    ../../source/pr_exec.cpp
    ../../source/vc/vc_object.cpp
    ../../source/scripts.h
    ../../source/sc_man.cpp
  )

  set(VCCRUN_FSYS_SOURCES
    filesys/fsys.h
    filesys/fsys.cpp
    filesys/pak/zip.cpp
    filesys/pak/dfwad.cpp
  )

  add_executable(vcc_run
    ${VCCRUN_COMPILER_SOURCES}
    ${VCCRUN_RUNTIME_SOURCES}
    ${VCCRUN_FSYS_SOURCES}
    vcc_run.cpp
    vcc_run.h
    modules/mod_ini.h
    modules/mod_ini.cpp
    modules/sdlgl/mod_sdlgl.h
    modules/sdlgl/mod_sdlgl.cpp
    modules/dfmap/dfmap.h
    modules/dfmap/mod_dfmap.h
    modules/dfmap/mod_dfmap.cpp
  )

  #message(STATUS "VCCRUN: LIBS: " ${VCCRUN_LINKLIBS})
  #message(STATUS "VCCRUN: DEFS: " ${VCCRUN_DEFINES})

  #message(STATUS "VCCRUN: XX: " ${TARGET_IMPLIB})

  if(CYGWIN OR MINGW)
    # Prune --out-implib from executables. It doesn't make sense even
    # with --export-all-symbols.
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE}")
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
  endif()

  set_target_properties(vcc_run PROPERTIES OUTPUT_NAME ../../vcc_run)
  target_compile_definitions(vcc_run PRIVATE ${VCCRUN_DEFINES})
  target_link_libraries(vcc_run core ${LIBLZMA_LIBRARIES} ${VCCRUN_LINKLIBS})
  add_dependencies(vcc_run core imago ${VCCRUN_DEPS})
endif(ENABLE_VCCRUN)
