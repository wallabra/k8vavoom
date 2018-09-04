#---------------------------------------
#
# VaVoom C compiler sources
#
#---------------------------------------
set(VCCLIB_SOURCES
  vc/vc_local.h

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
)

#---------------------------------------
#
# Texture loader and manager sources
#
#---------------------------------------
set(TEXTURELIB_SOURCES
  textures/r_tex.cpp
  textures/r_tex.h
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
  render/r_adv_light.cpp
  render/r_adv_things.cpp
  render/r_bsp.cpp
  render/r_data.cpp
  render/r_data.cpp
  render/r_light.cpp
  render/r_main.cpp
  render/r_model.cpp
  render/r_particle.cpp
  render/r_portal.cpp
  render/r_sky.cpp
  render/r_surf.cpp
  render/r_things.cpp
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
# sound subsystem sources
#
#---------------------------------------
set(SOUNDSYS_SOURCES
  sound/sound.h
  sound/snd_data.cpp
  sound/snd_local.h
  sound/snd_main.cpp
  sound/snd_qmus2mid.cpp
  sound/snd_reverbs.cpp
  sound/snd_streamplayer.cpp
  sound/snd_timidity.cpp
  sound/snd_wav.cpp
)

#-- OpenAL files --
set(MAIN_OPENAL_SOURCES
  sound/snd_al.cpp
)

#-- Vorbis files --
set(MAIN_VORBIS_SOURCES
  sound/snd_vorbis.cpp
)

#-- MP3 files --
set(MAIN_MP3_SOURCES
  sound/snd_mp3.cpp
)

#-- MikMod files --
set(MAIN_MIKMOD_SOURCES
  sound/snd_mikmod.cpp
)

#-- ModPlug files --
set(MAIN_MODPLUG_SOURCES
  sound/snd_modplug.cpp
)

#-- FLAC files --
set(MAIN_FLAC_SOURCES
  sound/snd_flac.cpp
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
# file subsystem sources
#
#---------------------------------------
set(FILESYS_SOURCES
  filesys/fwaddefs.h
  filesys/fs_local.h
  filesys/fs_dir.cpp
  filesys/fs_wad.cpp
  filesys/fs_zip.cpp
  filesys/files.cpp
  filesys/files.h
  filesys/wad.cpp
  filesys/wad.h
  filesys/zipstream.cpp
  filesys/zipstream.h
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
  l_glbsp.cpp
  l_glvis.cpp
  language.cpp
  language.h
  level.cpp
  level.h
  lockdefs.cpp
  lockdefs.h
  mapinfo.cpp
  mapinfo.h
  maths.cpp
  maths.h
  menu.cpp
  menu.h
  misc.cpp
  misc.h
  ${NETSUB_SOURCES}
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
  p_gameinfo.cpp
  p_gameinfo.h
  p_gameobject.cpp
  p_gameobject.h
  p_level_think.cpp
  p_levelinfo.cpp
  p_levelinfo.h
  p_nodebuild.cpp
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
  progs.h
  ${RENDERBASE_SOURCES}
  ${TEXTURELIB_SOURCES}
  save.h
  sbar.cpp
  sbar.h
  sc_man.cpp
  screen.cpp
  screen.h
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
  ${VCCLIB_SOURCES}
  video.h
)

#-- Common OpenGL files --
set(MAIN_OPENGL_SOURCES
  render/hwgl/gl_local.h
  render/hwgl/gl_draw.cpp
  render/hwgl/gl_main.cpp
  render/hwgl/gl_model.cpp
  render/hwgl/gl_poly.cpp
  render/hwgl/gl_tex.cpp
)

#-- Linux with SDL --
set(MAIN_UNIX_SDL_SOURCES
  in_sdl.cpp
  sys_sdl.cpp
)
set(MAIN_OPENGL_UNIX_SDL_SOURCES
  render/hwgl/gl_sdl.cpp
)

#-- Windows --
set(MAIN_WIN32_SOURCES
  winshit/in_win32.cpp
  winshit/sys_win.cpp
#  winshit/sound/eax.h
#  winshit/sound/snd_win32.cpp
#  winshit/sound/snd_win32music.cpp
#  winshit/res/vavoom.ico
#  winshit/res/vavoom_2.ico
#  winshit/res/vavoom_3.ico
#  winshit/res/vavoom.rc
  winshit/winlocal.h
)

set(MAIN_OPENGL_WIN32_SOURCES
  winshit/render/hwgl/gl_win32.cpp
)

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
  ${FILESYS_SOURCES}
  host.cpp
  infostr.cpp
  language.cpp
  level.cpp
  lockdefs.cpp
  mapinfo.cpp
  maths.cpp
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
  p_nodebuild.cpp
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
  render/r_data.cpp
  sound/snd_data.cpp
  sound/snd_reverbs.cpp
  sc_man.cpp
  sv_main.cpp
  sv_save.cpp
  sv_world.cpp
  ${VCCLIB_SOURCES}
)

set(SERVER_UNIX_SOURCES
  sys_bsd.cpp
)

set(SERVER_WIN32_SOURCES
  winshit/sys_wind.cpp
#  winshit/res/vavoom.rc
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


#---------------------------------------
#
# Main executable
#
#---------------------------------------
if(ENABLE_CLIENT)

  set(MAIN_SOURCES ${MAIN_COMMON_SOURCES})
  set(MAIN_LIBS glbsp libglvis timidity core ${VAVOOM_MAIN_DEPS} ${NET_LIBRARIES} ${MAIN_LIBS})
  set(MAIN_COMPILE_FLAGS "")

  if(OPENAL_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENAL_SOURCES})
  endif(OPENAL_FOUND)

  if(VORBIS_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_VORBIS_SOURCES})
  endif(VORBIS_FOUND)

  if(LIBMAD_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_MP3_SOURCES})
  endif(LIBMAD_FOUND)

  if(MIKMOD_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_MIKMOD_SOURCES})
  endif(MIKMOD_FOUND)

  if(MODPLUG_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_MODPLUG_SOURCES})
  endif(MODPLUG_FOUND)

  if(FLAC_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_FLAC_SOURCES})
  endif(FLAC_FOUND)


  if(WIN32)
    # ---------------- Windows ----------------
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_WIN32_SOURCES})
    set(MAIN_LIBS gdi32 ole32 winmm ${MAIN_LIBS})

    #  CMake doesn't support automatic building of resource files so
    # we must add a custom command and add object files to the list
    # of source files
    ### set(RC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/winshit/res/vavoom.rc)
    ### set(RC_OBJ ${CMAKE_CURRENT_BINARY_DIR}/winshit/res/vavoom.o)
    ### add_custom_command(OUTPUT ${RC_OBJ}
    ###   DEPENDS ${RC_SRC}
    ###   COMMAND windres --include-dir=${CMAKE_CURRENT_SOURCE_DIR} --include-dir=${CMAKE_CURRENT_BINARY_DIR} -o ${RC_OBJ} -i ${RC_SRC}
    ### )
    ### set(MAIN_SOURCES ${MAIN_SOURCES} ${RC_OBJ})
    #set(MAIN_SOURCES ${MAIN_SOURCES})
  else(WIN32)
    if(SDL2_FOUND)
      set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_UNIX_SDL_SOURCES})
    endif(SDL2_FOUND)
  endif(WIN32)


  if(OPENGL_FOUND)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_SOURCES})
    if(WIN32)
      set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_WIN32_SOURCES})
      set(MAIN_LIBS opengl32 ${MAIN_LIBS})
    else(WIN32)
      set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_UNIX_SDL_SOURCES})
    endif(WIN32)
  endif(OPENGL_FOUND)


  if(WITH_GME)
    set(MAIN_SOURCES ${MAIN_SOURCES} sound/snd_gme.cpp)
    set(VAVOOM_MAIN_DEPS ${VAVOOM_MAIN_DEPS} gme)
  endif(WITH_GME)


  if(CYGWIN OR MINGW)
    # prune --out-implib from executables: it doesn't make sense, even with --export-all-symbols
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE}")
    string(REGEX REPLACE "-Wl,--out-implib,[^ ]+ " " " CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
  endif()


  add_executable(vavoom ${MAIN_SOURCES})
  if(ENABLE_WRAPPERS)
    set_target_properties(vavoom PROPERTIES OUTPUT_NAME ../vavoom.bin)
  else(ENABLE_WRAPPERS)
    set_target_properties(vavoom PROPERTIES OUTPUT_NAME ../vavoom)
  endif(ENABLE_WRAPPERS)
  if (MAIN_COMPILE_FLAGS)
    set_target_properties(vavoom PROPERTIES COMPILE_FLAGS ${MAIN_COMPILE_FLAGS})
  endif (MAIN_COMPILE_FLAGS)
  #message(STATUS "MAIN LINKLIBS: " ${MAIN_LIBS})
  target_link_libraries(vavoom ${MAIN_LIBS})
  add_dependencies(vavoom core timidity glbsp libglvis ${VAVOOM_MAIN_DEPS})

  install(TARGETS vavoom DESTINATION ${BINDIR})

  #
  # Icon
  #
  install(FILES ../res/vavoom.png DESTINATION ${DATADIR})

  if(ENABLE_WRAPPERS)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/vavoom
"#!/bin/sh
# Needed to make symlinks/shortcuts work.
# the binaries must run with correct working directory
exec \"${CMAKE_INSTALL_PREFIX}/${BINDIR}/vavoom.bin\" $* -basedir \"${CMAKE_INSTALL_PREFIX}/${DATADIR}\" ${IWADDIR}
")
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vavoom DESTINATION ${BINDIR})
  endif(ENABLE_WRAPPERS)

endif(ENABLE_CLIENT)


#---------------------------------------
#
# Dedicated server
#
#---------------------------------------
if(ENABLE_SERVER)

  if(WIN32)
    set(SERVER_SOURCES ${SERVER_COMMON_SOURCES} ${SERVER_WIN32_SOURCES})

    #  CMake doesn't support automatic building of resource files so
    # we must add a custom command and add object files to the list
    # of source files
    ### set(RC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/winshit/res/vavoom.rc)
    ### set(RC_OBJ ${CMAKE_CURRENT_BINARY_DIR}/winshit/res/vavoom_sv.o)
    ### add_custom_command(OUTPUT ${RC_OBJ}
    ###   DEPENDS ${RC_SRC}
    ###   COMMAND windres --include-dir=${CMAKE_CURRENT_SOURCE_DIR} --include-dir=${CMAKE_CURRENT_BINARY_DIR} -o ${RC_OBJ} -i ${RC_SRC}
    ### )
    ### set(SERVER_SOURCES ${SERVER_SOURCES} ${RC_OBJ})
    #set(SERVER_SOURCES ${SERVER_SOURCES})
  else(WIN32)
    set(SERVER_SOURCES ${SERVER_COMMON_SOURCES} ${SERVER_UNIX_SOURCES})
  endif(WIN32)

  add_executable(vavoom-dedicated ${SERVER_SOURCES})
  if(ENABLE_WRAPPERS)
    set_target_properties(vavoom-dedicated PROPERTIES OUTPUT_NAME ../vavoom-dedicated.bin)
  else(ENABLE_WRAPPERS)
    set_target_properties(vavoom-dedicated PROPERTIES OUTPUT_NAME ../vavoom-dedicated)
  endif(ENABLE_WRAPPERS)
  set_target_properties(vavoom-dedicated PROPERTIES COMPILE_FLAGS -DSERVER)
  #FIXME: create separate from MAIN_LIBS link list
  target_link_libraries(vavoom-dedicated glbsp core ${MAIN_LIBS} ${NET_LIBRARIES})
  add_dependencies(vavoom-dedicated core glbsp ${VAVOOM_MAIN_DEPS})

  install(TARGETS vavoom-dedicated DESTINATION ${BINDIR})

  if(ENABLE_WRAPPERS)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/vavoom-dedicated
"#!/bin/sh
# Needed to make symlinks/shortcuts work.
# the binaries must run with correct working directory
exec \"${CMAKE_INSTALL_PREFIX}/${BINDIR}/vavoom-dedicated.bin\" $* -basedir \"${CMAKE_INSTALL_PREFIX}/${DATADIR}\" ${IWADDIR}
")
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vavoom-dedicated DESTINATION ${BINDIR})
  endif(ENABLE_WRAPPERS)

endif(ENABLE_SERVER)
