function(sxx_compile output filename)
  get_filename_component(ext ${filename} EXT)
  if(NOT ext STREQUAL ".sxx")
    return()
  endif()
  get_filename_component(base ${filename} NAME_WE)
  set(base_abs ${CMAKE_CURRENT_BINARY_DIR}/${base})
  set(outfiles ${base_abs}.ci ${base_abs}.hi)
  set(${output} ${${output}} ${outfiles} PARENT_SCOPE)
  #message("outfiles=${outfiles}, DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}")
  if(CMAKE_CROSSCOMPILING AND NOT WIN32)
    # assume we have vshadpp in PATH
    add_custom_command(
      OUTPUT ${outfiles}
      COMMAND vshadpp -o ${CMAKE_CURRENT_BINARY_DIR} -b ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/*.vs
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/*.fs
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.vs
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.fs
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.inc
      #${CMAKE_CURRENT_SOURCE_DIR}/${mastername}
    )
  else()
    add_custom_command(
      OUTPUT ${outfiles}
      COMMAND ${WINERUN} $<TARGET_FILE:vshadpp> -o ${CMAKE_CURRENT_BINARY_DIR} -b ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
      DEPENDS vshadpp ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/*.vs
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/*.fs
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.vs
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.fs
        ${CMAKE_SOURCE_DIR}/basev/common/glshaders/common/*.inc
      #${CMAKE_CURRENT_SOURCE_DIR}/${mastername}
    )
  endif()
  set_source_files_properties(${outfiles} PROPERTIES GENERATED TRUE)
endfunction()


#---------------------------------------
#
# k8vavoom C compiler sources
#
#---------------------------------------
set(VCCLIB_SOURCES
  vc/vc_local.h
  vc/vc_public.h
  vc/vc_progs.h

  vc/vc_location.h
  vc/vc_location.cpp
  vc/vc_error.h
  vc/vc_error.cpp
  vc/vc_type.h
  vc/vc_type.cpp
  vc/vc_member.h
  vc/vc_member.cpp
  vc/vc_constant.h
  vc/vc_constant.cpp
  vc/vc_property.h
  vc/vc_property.cpp
  vc/vc_state.h
  vc/vc_state.cpp
  vc/vc_field.h
  vc/vc_field.cpp
  vc/vc_method.h
  vc/vc_method.cpp
  vc/vc_struct.h
  vc/vc_struct.cpp
  vc/vc_class.h
  vc/vc_class.cpp
  vc/vc_object.h
  vc/vc_object.cpp
  vc/vc_package.h
  vc/vc_package.cpp
  vc/vc_decorate.h
  vc/vc_decorate.cpp
  vc/vc_dehacked.h
  vc/vc_dehacked.cpp

  vc/vc_emit_context.h
  vc/vc_emit_context.cpp
  vc/vc_expr_base.h
  vc/vc_expr_base.cpp

  vc/vc_expr_array.h
  vc/vc_expr_array.cpp
  vc/vc_expr_assign.h
  vc/vc_expr_assign.cpp
  vc/vc_expr_cast.h
  vc/vc_expr_cast.cpp
  vc/vc_expr_field.h
  vc/vc_expr_field.cpp
  vc/vc_expr_invoke.h
  vc/vc_expr_invoke.cpp
  vc/vc_expr_literal.h
  vc/vc_expr_literal.cpp
  vc/vc_expr_local.h
  vc/vc_expr_local.cpp
  vc/vc_expr_misc.h
  vc/vc_expr_misc.cpp
  vc/vc_expr_type.h
  vc/vc_expr_type.cpp
  vc/vc_expr_unary_binary.h
  vc/vc_expr_unary_binary.cpp
  vc/vc_statement.h
  vc/vc_statement.cpp

  vc/vc_lexer.h
  vc/vc_lexer_tokens.h
  vc/vc_lexer.cpp
  vc/vc_modifiers.h
  vc/vc_modifiers.cpp
  vc/vc_parser.h
  vc/vc_parser.cpp

  vc/vc_zastar.h
  vc/vc_zastar.cpp
)

#---------------------------------------
#
# Texture loader and manager sources
#
#---------------------------------------
set(TEXTURELIB_SOURCES
  textures/r_tex_id.h
  textures/r_tex_public.h
  textures/r_tex.h
  textures/r_tex.cpp
  textures/r_tex_id.cpp
  textures/r_tex_automap.cpp
  textures/r_tex_base.cpp
  textures/r_tex_camera.cpp
  textures/r_tex_flat.cpp
  textures/r_tex_imgz.cpp
  textures/r_tex_jpeg.cpp
  textures/r_tex_multipatch.cpp
  textures/r_tex_patch.cpp
  textures/r_tex_pcx.cpp
  textures/r_tex_png.cpp
  textures/r_tex_raw.cpp
  textures/r_tex_tga.cpp
  textures/r_tex_warp.cpp
)

set(RENDERBASE_SOURCES
  render/anorm_dots.h
  render/anorms.h
  render/fmd2defs.h
  render/r_local.h
  render/r_public.h
  render/r_shared.h
  render/r_bsp.cpp
  render/r_bsp_reg.cpp
  render/r_bsp_adv.cpp
  render/r_data.cpp
  render/r_data.cpp
  render/r_light.cpp
  render/r_light_reg.cpp
  render/r_light_adv.cpp
  render/r_main.cpp
  render/r_main_reg.cpp
  render/r_main_adv.cpp
  render/r_model.cpp
  render/r_particle.cpp
  render/r_portal.cpp
  render/r_sky.cpp
  render/r_surf.cpp
  render/r_surf_adv.cpp
  render/r_surf_reg.cpp
  render/r_things.cpp
  render/r_things_adv.cpp
  render/r_updateworld.cpp
)

#---------------------------------------
#
# UI library (widgets) sources
#
#---------------------------------------
set(UIWIDGETS_SOURCES
  ui/ui.h
  ui/ui_actor.cpp
  ui/ui_font.cpp
  ui/ui_font.h
  ui/ui_root.cpp
  ui/ui_root.h
  ui/ui_widget.cpp
  ui/ui_widget.h
)

#---------------------------------------
#
# New UI library sources
#
#---------------------------------------
set(NEWUI_SOURCES
  neoui/neoui.h
  neoui/neoui_main.cpp
)

#---------------------------------------
#
# sound subsystem sources
#
#---------------------------------------
set(SOUNDSYS_SOURCES
  sound/sound.h
  sound/snd_data.cpp
  sound/snd_local.h
  sound/snd_main.cpp
  sound/snd_al.cpp
  sound/snd_qmus2mid.cpp
  #k8:not used, 'cause there is no EAX/EFX support: sound/snd_reverbs.cpp
  sound/snd_streamplayer.cpp
  sound/snd_timidity.cpp
  sound/snd_wav.cpp
  # xmplite is always here
  sound/snd_xmplite.cpp
)


#---------------------------------------
#
# network subsystem sources
#
#---------------------------------------
set(NETSUB_SOURCES
  net/network.h
  net/net_channel.cpp
  net/net_channel_control.cpp
  net/net_channel_level.cpp
  net/net_channel_object_map.cpp
  net/net_channel_player.cpp
  net/net_channel_thinker.cpp
  net/net_connection.cpp
  net/net_context.cpp
  net/net_datagram.cpp
  net/net_demo.cpp
  net/net_local.h
  net/net_loopback.cpp
  net/net_main.cpp
  net/net_message.cpp
  net/net_message.h
  net/net_object_map.cpp
  net/net_udp.cpp
)


#---------------------------------------
#
# ajbsp sources
#
#---------------------------------------
set(AJBSP_SOURCES
  ajbsp/bsp_level.cpp
  ajbsp/bsp_node.cpp
  ajbsp/bsp_util.cpp
  #ajbsp/lib_util.cpp
)


#---------------------------------------
#
# zdbsp sources
#
#---------------------------------------
set(ZDBSP_SOURCES
  zdbsp/nodebuild.cpp
  zdbsp/nodebuild_events.cpp
  zdbsp/nodebuild_extract.cpp
  zdbsp/nodebuild_gl.cpp
  zdbsp/nodebuild_utility.cpp
  zdbsp/nodebuild_classify_nosse2.cpp
  zdbsp/zdbsp_extr.cpp
)


#---------------------------------------
#
# file subsystem sources
#
#---------------------------------------
set(FILESYS_SOURCES
  filesys/fwaddefs.h
  filesys/fs_local.h
  filesys/fs_pakbase.cpp
  filesys/fs_dir.cpp
  filesys/fs_dirpak.cpp
  filesys/fs_wad.cpp
  filesys/fs_zip.cpp
  filesys/files.cpp
  filesys/files.h
  filesys/wad.cpp
  filesys/wad.h
)


#---------------------------------------
#
# Main executable sources
#
#---------------------------------------

#-- Common files --
set(MAIN_COMMON_SOURCES
  am_map.cpp
  automap.h
  build.h
  chat.cpp
  chat.h
  cheats.cpp
  client.h
  cl_local.h
  cl_main.cpp
  cmd.cpp
  cmd.h
  common.h
  console.cpp
  console.h
  cvar.cpp
  cvar.h
  debug.cpp
  debug.h
  drawer.h
  drawer.cpp
  finale.cpp
  finale.h
  ${FILESYS_SOURCES}
  gamedefs.h
  host.cpp
  host.h
  iline.cpp
  iline.h
  in_input.cpp
  infostr.cpp
  infostr.h
  input.h
  #l_glbsp.cpp
  #l_glvis.cpp
  language.cpp
  language.h
  level.cpp
  level.h
  lockdefs.cpp
  lockdefs.h
  mapinfo.cpp
  mapinfo.h
  menu.cpp
  menu.h
  misc.cpp
  misc.h
  ${NETSUB_SOURCES}
  qs_data.h
  qs_data.cpp
  p_acs.cpp
  p_acs.h
  p_clip.cpp
  p_clip.h
  p_decal.cpp
  p_decal.h
  p_entity.cpp
  p_entity.h
  p_entity_sight.cpp
  p_entity_world.cpp
  #p_level_tracebox.cpp
  p_gameinfo.cpp
  p_gameinfo.h
  p_gameobject.cpp
  p_gameobject.h
  p_level_think.cpp
  p_levelinfo.cpp
  p_levelinfo.h
  p_setup_calc.cpp
  p_player.cpp
  p_player.h
  p_player_input.cpp
  p_playerreplicationinfo.cpp
  p_playerreplicationinfo.h
  p_polyobj.cpp
  p_setup.cpp
  p_setup_udmf.cpp
  p_switch.cpp
  p_terrain.cpp
  p_thinker.cpp
  p_thinker.h
  p_trace.cpp
  p_world.cpp
  p_world.h
  p_worldinfo.cpp
  p_worldinfo.h
  pr_cmds.cpp
  pr_exec.cpp
  progdefs.h
  ${RENDERBASE_SOURCES}
  ${TEXTURELIB_SOURCES}
  ntvalueioex.cpp
  save.h
  sbar.cpp
  sbar.h
  screen.cpp
  screen.h
  scripts.cpp
  scripts.h
  server.h
  ${SOUNDSYS_SOURCES}
  sv_local.h
  sv_main.cpp
  sv_save.cpp
  sv_world.cpp
  system.h
  text.cpp
  text.h
  ${UIWIDGETS_SOURCES}
  ${NEWUI_SOURCES}
  ${VCCLIB_SOURCES}
  video.h
  ${AJBSP_SOURCES}
  ${ZDBSP_SOURCES}
)

#-- Common OpenGL files --
set(MAIN_OPENGL_SOURCES
  render/hwgl/gl_local.h
  render/hwgl/gl_decal.cpp
  render/hwgl/gl_draw.cpp
  render/hwgl/gl_main.cpp
  render/hwgl/gl_masked.cpp
  render/hwgl/gl_model.cpp
  render/hwgl/gl_particle.cpp
  render/hwgl/gl_poly.cpp
  render/hwgl/gl_poly_adv.cpp
  render/hwgl/gl_portal.cpp
  render/hwgl/gl_tex.cpp
  render/hwgl/gl_shaddef.sxx
  ${CMAKE_CURRENT_BINARY_DIR}/gl_shaddef.hi
  ${CMAKE_CURRENT_BINARY_DIR}/gl_shaddef.ci
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
foreach(file ${MAIN_OPENGL_SOURCES})
  sxx_compile(OPENGL_GENERATED_SOURCES ${file})
endforeach()

source_group("" FILES ${MAIN_OPENGL_SOURCES})
source_group(generated FILES ${OPENGL_GENERATED_SOURCES})

#add_executable(vshadpp ${MAIN_OPENGL_SOURCES} ${OPENGL_GENERATED_SOURCES})
#sxx_compile(render/hwgl/gl_shaddef.sxx  render/hwgl/gl_main.cpp)


include_directories( .
  vc
  ../libs
  ${SYSTEM_SOURCES_DIR}
)

#---------------------------------------
#
# Dedicated server sources
#
#---------------------------------------

set(SERVER_COMMON_SOURCES
  cheats.cpp
  cmd.cpp
  cvar.cpp
  debug.cpp
  drawer.h
  drawer.cpp
  ${FILESYS_SOURCES}
  host.cpp
  infostr.cpp
  language.cpp
  level.cpp
  lockdefs.cpp
  mapinfo.cpp
  misc.cpp
  ${NETSUB_SOURCES}
  p_acs.cpp
  p_clip.cpp
  p_decal.cpp
  p_decal.h
  p_entity.cpp
  p_entity_sight.cpp
  p_entity_world.cpp
  p_gameinfo.cpp
  p_gameobject.cpp
  p_level_think.cpp
  p_levelinfo.cpp
  p_setup_calc.cpp
  p_player.cpp
  p_player_input.cpp
  p_playerreplicationinfo.cpp
  p_polyobj.cpp
  p_setup.cpp
  p_setup_udmf.cpp
  p_switch.cpp
  p_terrain.cpp
  p_thinker.cpp
  p_trace.cpp
  p_world.cpp
  p_worldinfo.cpp
  pr_cmds.cpp
  pr_exec.cpp
  ${TEXTURELIB_SOURCES}
  ntvalueioex.h
  ntvalueioex.cpp
  qs_data.h
  qs_data.cpp
  render/r_data.cpp
  sound/snd_data.cpp
  scripts.cpp
  sv_main.cpp
  sv_save.cpp
  sv_world.cpp
  ${VCCLIB_SOURCES}
  ${AJBSP_SOURCES}
  ${ZDBSP_SOURCES}
  # for skybox parsing
  render/r_sky.cpp
)


#---------------------------------------
#
# svnrev.h stuff
#
#---------------------------------------

### add_custom_target( revision_check ALL
###   COMMAND updaterevision ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/svnrev.h
###   WORKING_DIRECTORY ${UPDATEREVISION_EXE}
###   DEPENDS updaterevision )

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(NOT WIN32)
  set(K8VAVOOMDEV )
  if("$ENV{K8VAVOOMDEV}" STREQUAL "tan")
    set(K8VAVOOMDEV ON)
  endif()
  if("$ENV{USER}" STREQUAL "ketmar")
    set(K8VAVOOMDEV ON)
  endif()

  if(K8VAVOOMDEV)
    add_definitions(-DVAVOOM_K8_DEVELOPER)
  endif(K8VAVOOMDEV)
endif(NOT WIN32)


#---------------------------------------
#
# Main executable
#
#---------------------------------------
if(ENABLE_CLIENT)
  set(MAIN_SOURCES ${MAIN_COMMON_SOURCES})
  #set(MAIN_LIBS glbsp libglvis timidity core ${VAVOOM_MAIN_DEPS} ${NET_LIBRARIES} ${MAIN_LIBS})
  set(MAIN_LIBS timidity core ${VAVOOM_MAIN_DEPS} ${NET_LIBRARIES} ${MAIN_LIBS})
  set(MAIN_COMPILE_FLAGS "")

  if(VORBIS_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/snd_vorbis.cpp)
  endif(VORBIS_FOUND)

  if(OPUS_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/snd_opus.cpp)
  endif(OPUS_FOUND)

  if(LIBMAD_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/snd_mp3.cpp)
  endif(LIBMAD_FOUND)

  if(FLAC_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/snd_flac.cpp)
  endif(FLAC_FOUND)


  ### if(WIN32)
  ###   # ---------------- Windows ----------------
  ###   set(MAIN_SOURCES ${MAIN_SOURCES} winshit/winlocal.h winshit/in_win32.cpp winshit/sys_win.cpp)
  ###   set(MAIN_LIBS gdi32 ole32 winmm ${MAIN_LIBS})
  ###
  ###   #  CMake doesn't support automatic building of resource files so
  ###   # we must add a custom command and add object files to the list
  ###   # of source files
  ###   ### set(RC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/winshit/res/k8vavoom.rc)
  ###   ### set(RC_OBJ ${CMAKE_CURRENT_BINARY_DIR}/winshit/res/k8vavoom.o)
  ###   ### add_custom_command(OUTPUT ${RC_OBJ}
  ###   ###   DEPENDS ${RC_SRC}
  ###   ###   COMMAND windres --include-dir=${CMAKE_CURRENT_SOURCE_DIR} --include-dir=${CMAKE_CURRENT_BINARY_DIR} -o ${RC_OBJ} -i ${RC_SRC}
  ###   ### )
  ###   ### set(MAIN_SOURCES ${MAIN_SOURCES} ${RC_OBJ})
  ###   #set(MAIN_SOURCES ${MAIN_SOURCES})
  ### else(WIN32)
  ###   if(SDL2_FOUND)
  ###     set(MAIN_SOURCES ${MAIN_SOURCES} in_sdl.cpp sys_sdl.cpp)
  ###   endif(SDL2_FOUND)
  ### endif(WIN32)
  if(SDL2_FOUND)
    if(WIN32)
      set(MAIN_LIBS gdi32 ole32 winmm imm32 version ${MAIN_LIBS})
    endif(WIN32)
    set(MAIN_SOURCES ${MAIN_SOURCES} in_sdl.cpp sys_sdl.cpp)
    set(MAIN_SOURCES ${MAIN_SOURCES} render/hwgl/gl_sdl.cpp)
    set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} imago)
    set(MAIN_LIBS imago ${MAIN_LIBS})
  endif(SDL2_FOUND)


  ### if(OPENGL_FOUND)
  ###   set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_SOURCES})
  ###   if(WIN32)
  ###     set(MAIN_SOURCES ${MAIN_SOURCES} winshit/render/hwgl/gl_win32.cpp)
  ###     set(MAIN_LIBS opengl32 ${MAIN_LIBS})
  ###   else(WIN32)
  ###     set(MAIN_SOURCES ${MAIN_SOURCES} render/hwgl/gl_sdl.cpp)
  ###   endif(WIN32)
  ### endif(OPENGL_FOUND)
  if(OPENGL_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_SOURCES})
  endif(OPENGL_FOUND)


  if(CYGWIN OR MINGW)
    # prune --out-implib from executables: it doesn't make sense, even with --export-all-symbols
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE}")
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
  endif()


  add_executable(k8vavoom ${MAIN_SOURCES})
  if(ENABLE_WRAPPERS)
    set_target_properties(k8vavoom PROPERTIES OUTPUT_NAME ../k8vavoom.bin)
  else(ENABLE_WRAPPERS)
    set_target_properties(k8vavoom PROPERTIES OUTPUT_NAME ../k8vavoom)
  endif(ENABLE_WRAPPERS)
  if (MAIN_COMPILE_FLAGS)
    set_target_properties(k8vavoom PROPERTIES COMPILE_FLAGS ${MAIN_COMPILE_FLAGS})
  endif (MAIN_COMPILE_FLAGS)
  #message(STATUS "MAIN LINKLIBS: " ${MAIN_LIBS})
  target_link_libraries(k8vavoom ${MAIN_LIBS})
  #add_dependencies(k8vavoom core timidity glbsp libglvis ${VAVOOM_MAIN_DEPS})
  add_dependencies(k8vavoom core timidity ${VAVOOM_MAIN_DEPS})

  install(TARGETS k8vavoom DESTINATION ${BINDIR})

  #
  # Icon
  #
  install(FILES ../res/k8vavoom.png DESTINATION ${DATADIR})

  if(ENABLE_WRAPPERS)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/k8vavoom
"#!/bin/sh
# Needed to make symlinks/shortcuts work.
# the binaries must run with correct working directory
exec \"${CMAKE_INSTALL_PREFIX}/${BINDIR}/k8vavoom.bin\" \"\$\@\" -basedir \"${CMAKE_INSTALL_PREFIX}/${DATADIR}\" ${IWADDIR}
")
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/k8vavoom DESTINATION ${BINDIR})
  endif(ENABLE_WRAPPERS)
endif(ENABLE_CLIENT)


#---------------------------------------
#
# Dedicated server
#
#---------------------------------------
if(ENABLE_SERVER)
  ### if(WIN32)
  ###   set(SERVER_SOURCES ${SERVER_COMMON_SOURCES} winshit/sys_wind.cpp)
  ###
  ###   #  CMake doesn't support automatic building of resource files so
  ###   # we must add a custom command and add object files to the list
  ###   # of source files
  ###   ### set(RC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/winshit/res/k8vavoom.rc)
  ###   ### set(RC_OBJ ${CMAKE_CURRENT_BINARY_DIR}/winshit/res/k8vavoom_sv.o)
  ###   ### add_custom_command(OUTPUT ${RC_OBJ}
  ###   ###   DEPENDS ${RC_SRC}
  ###   ###   COMMAND windres --include-dir=${CMAKE_CURRENT_SOURCE_DIR} --include-dir=${CMAKE_CURRENT_BINARY_DIR} -o ${RC_OBJ} -i ${RC_SRC}
  ###   ### )
  ###   ### set(SERVER_SOURCES ${SERVER_SOURCES} ${RC_OBJ})
  ###   #set(SERVER_SOURCES ${SERVER_SOURCES})
  ### else(WIN32)
  ###   set(SERVER_SOURCES ${SERVER_COMMON_SOURCES} sys_ded.cpp)
  ### endif(WIN32)
  set(SERVER_SOURCES ${SERVER_COMMON_SOURCES} sys_ded.cpp)

  add_executable(k8vavoom-dedicated ${SERVER_SOURCES})
  if(ENABLE_WRAPPERS)
    set_target_properties(k8vavoom-dedicated PROPERTIES OUTPUT_NAME ../k8vavoom-dedicated.bin)
  else(ENABLE_WRAPPERS)
    set_target_properties(k8vavoom-dedicated PROPERTIES OUTPUT_NAME ../k8vavoom-dedicated)
  endif(ENABLE_WRAPPERS)
  set_target_properties(k8vavoom-dedicated PROPERTIES COMPILE_FLAGS -DSERVER)
  #FIXME: create separate from MAIN_LIBS link list
  #target_link_libraries(k8vavoom-dedicated glbsp core ${MAIN_LIBS} ${NET_LIBRARIES})
  target_link_libraries(k8vavoom-dedicated core ${MAIN_LIBS} ${NET_LIBRARIES})
  #add_dependencies(k8vavoom-dedicated core glbsp ${VAVOOM_MAIN_DEPS})
  add_dependencies(k8vavoom-dedicated core ${VAVOOM_MAIN_DEPS})

  install(TARGETS k8vavoom-dedicated DESTINATION ${BINDIR})

  if(ENABLE_WRAPPERS)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/k8vavoom-dedicated
"#!/bin/sh
# Needed to make symlinks/shortcuts work.
# the binaries must run with correct working directory
exec \"${CMAKE_INSTALL_PREFIX}/${BINDIR}/k8vavoom-dedicated.bin\" \"\$\@\" -basedir \"${CMAKE_INSTALL_PREFIX}/${DATADIR}\" ${IWADDIR}
")
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/k8vavoom-dedicated DESTINATION ${BINDIR})
  endif(ENABLE_WRAPPERS)
endif(ENABLE_SERVER)
