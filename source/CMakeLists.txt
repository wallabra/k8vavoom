#---------------------------------------
#
# VaVoom C compiler sources
#
#---------------------------------------
set(VCCLIB_SOURCES
  vc/vc_class.cpp
  vc/vc_class.h
  vc/vc_constant.cpp
  vc/vc_constant.h
  vc/vc_decorate.cpp
  vc/vc_decorate.h
  vc/vc_dehacked.cpp
  vc/vc_dehacked.h
  vc/vc_emit_context.cpp
  vc/vc_emit_context.h
  vc/vc_error.cpp
  vc/vc_error.h
  vc/vc_expr_array.cpp
  vc/vc_expr_array.h
  vc/vc_expr_assign.cpp
  vc/vc_expr_assign.h
  vc/vc_expr_base.cpp
  vc/vc_expr_base.h
  vc/vc_expr_cast.cpp
  vc/vc_expr_cast.h
  vc/vc_expr_field.cpp
  vc/vc_expr_field.h
  vc/vc_expr_invoke.cpp
  vc/vc_expr_invoke.h
  vc/vc_expr_literal.cpp
  vc/vc_expr_literal.h
  vc/vc_expr_local.cpp
  vc/vc_expr_local.h
  vc/vc_expr_misc.cpp
  vc/vc_expr_misc.h
  vc/vc_expr_type.cpp
  vc/vc_expr_type.h
  vc/vc_expr_unary_binary.cpp
  vc/vc_expr_unary_binary.h
  vc/vc_field.cpp
  vc/vc_field.h
  vc/vc_lexer.cpp
  vc/vc_lexer.h
  vc/vc_local.h
  vc/vc_location.cpp
  vc/vc_location.h
  vc/vc_member.cpp
  vc/vc_member.h
  vc/vc_method.cpp
  vc/vc_method.h
  vc/vc_modifiers.cpp
  vc/vc_modifiers.h
  vc/vc_object.cpp
  vc/vc_object.h
  vc/vc_package.cpp
  vc/vc_package.h
  vc/vc_parser.cpp
  vc/vc_parser.h
  vc/vc_property.cpp
  vc/vc_property.h
  vc/vc_state.cpp
  vc/vc_state.h
  vc/vc_statement.cpp
  vc/vc_statement.h
  vc/vc_struct.cpp
  vc/vc_struct.h
  vc/vc_type.cpp
  vc/vc_type.h
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
  net/net_udp.cpp
  #sound/snd_sdl.cpp
  #sound/snd_sdlmusic.cpp
  sys_sdl.cpp
)
set(MAIN_OPENGL_UNIX_SDL_SOURCES
  render/hwgl/gl_sdl.cpp
)

#-- Windows --
set(MAIN_WIN32_SOURCES
  winshit/sound/eax.h
  winshit/in_win32.cpp
  winshit/net/net_winsock.cpp
  winshit/sound/snd_win32.cpp
  winshit/sound/snd_win32music.cpp
  winshit/sys_win.cpp
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
  net/net_udp.cpp
  sys_bsd.cpp
)

set(SERVER_WIN32_SOURCES
  winshit/net/net_winsock.cpp
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

if(WITH_JPG)
  set(JPG_LIBRARY "")
else(WITH_JPG)
  set(JPG_LIBRARY "jpeg")
endif(WITH_JPG)

set(MAIN_SOURCES ${MAIN_COMMON_SOURCES})
set(MAIN_LIBS glbsp libglvis timidity gme core lzma ${JPG_LIBRARY} ${ZLIB_LIBRARY} ${NET_LIBRARIES})
set(MAIN_COMPILE_FLAGS "")

#
#  **** Platform specific library checks ****
#

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
  set(MAIN_SOURCES ${MAIN_SOURCES})

  if(WITH_OPENGL)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_SOURCES} ${MAIN_OPENGL_WIN32_SOURCES})
    set(MAIN_LIBS opengl32 ${MAIN_LIBS})
  endif(WITH_OPENGL)

  if(WITH_OPENAL)
    find_path(OPENALAL_INCLUDE_DIR AL/al.h)
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENAL_SOURCES})
    set(MAIN_LIBS OpenAL32 ${MAIN_LIBS})
  endif(WITH_OPENAL)
else(WIN32)
  # ---------------- Other *NIX platforms ----------------
  if(WITH_SDL2)
    find_package(SDL2 REQUIRED)
    #PKG_SEARCH_MODULE(SDL2 REQUIRED sdl2)
  endif(WITH_SDL2)

  if(SDL2_FOUND)
    message(STATUS "Using SDL2")
    include_directories(${SDL2_INCLUDE_DIRS})
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_UNIX_SDL_SOURCES})
    set(MAIN_LIBS ${SDL2_LIBRARIES} ${MAIN_LIBS})
  else(SDL2_FOUND)
    if(WITH_SDL2)
      message(SEND_ERROR "SDL NOT FOUND")
      return()
    endif(WITH_SDL2)
  endif(SDL2_FOUND)

  if(WITH_OPENGL)
    if(NOT SDL2_FOUND)
      message(SEND_ERROR "SDL2 required for OpenGL")
      return()
    endif(NOT SDL2_FOUND)
    find_package(OpenGL)
    if(OPENGL_FOUND)
      message(STATUS "Found OpenGL")
      include_directories(${OPENGL_INCLUDE_DIR})
      set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_SOURCES})
      set(MAIN_LIBS ${OPENGL_gl_LIBRARY} ${MAIN_LIBS})
      set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENGL_UNIX_SDL_SOURCES})
    endif(OPENGL_FOUND)
  endif(WITH_OPENGL)

  if(WITH_OPENAL)
    find_package(OpenAL)
    if(OPENAL_FOUND)
      message(STATUS "Found OpenAL")
      include_directories(${OPENAL_INCLUDE_DIR})
      set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_OPENAL_SOURCES})
      set(MAIN_LIBS ${OPENAL_LIBRARY} ${MAIN_LIBS})
    endif(OPENAL_FOUND)
  endif(WITH_OPENAL)
endif(WIN32)

#
#  **** Check for libpng ****
#
if(WITH_LIBPNG)
  find_package(PNG REQUIRED)
  include_directories(${PNG_INCLUDE_DIR})
  set(MAIN_LIBS ${PNG_LIBRARY} ${MAIN_LIBS})
  add_definitions(-DVAVOOM_USE_LIBPNG=1)
endif(WITH_LIBPNG)

#
#  **** Check for libjpeg ****
#
if(WITH_LIBJPG)
  find_package(JPEG REQUIRED)
  include_directories(${JPEG_INCLUDE_DIR})
  set(MAIN_LIBS ${JPEG_LIBRARY} ${MAIN_LIBS})
  add_definitions(-DVAVOOM_USE_LIBJPG=1)
endif(WITH_LIBJPG)

#
#  **** Check for Vorbis ****
#

if(WITH_VORBIS)
  find_path(OGG_INCLUDE_DIR ogg/ogg.h)
  find_path(VORBIS_INCLUDE_DIR vorbis/codec.h)
  find_library(OGG_LIBRARY NAMES ogg)
  find_library(VORBIS_LIBRARY NAMES vorbis)
  if (OGG_INCLUDE_DIR AND VORBIS_INCLUDE_DIR AND OGG_LIBRARY AND VORBIS_LIBRARY)
    message(STATUS "Found OggVorbis: ${OGG_LIBRARY} ${VORBIS_LIBRARY}")
    include_directories(${OGG_INCLUDE_DIR})
    include_directories(${VORBIS_INCLUDE_DIR})
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_VORBIS_SOURCES})
    set(MAIN_LIBS ${VORBIS_LIBRARY} ${OGG_LIBRARY} ${MAIN_LIBS})
  else (OGG_INCLUDE_DIR AND VORBIS_INCLUDE_DIR AND OGG_LIBRARY AND VORBIS_LIBRARY)
    message(STATUS "Could NOT find OggVorbis libraries")
  endif (OGG_INCLUDE_DIR AND VORBIS_INCLUDE_DIR AND OGG_LIBRARY AND VORBIS_LIBRARY)
endif(WITH_VORBIS)

#
#  **** Check for libmad ****
#

if(WITH_LIBMAD)
  find_path(MAD_INCLUDE_DIR mad.h)
  find_library(MAD_LIBRARY NAMES mad)
  if(MAD_INCLUDE_DIR AND MAD_LIBRARY)
    message(STATUS "Found LibMAD: ${MAD_LIBRARY}")
    include_directories(${MAD_INCLUDE_DIR})
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_MP3_SOURCES})
    set(MAIN_LIBS ${MAD_LIBRARY} ${MAIN_LIBS})
  else(MAD_INCLUDE_DIR AND MAD_LIBRARY)
    message(STATUS "Could NOT find LibMAD libraries")
  endif(MAD_INCLUDE_DIR AND MAD_LIBRARY)
endif(WITH_LIBMAD)

#
#  **** Check for MikMod ****
#

if(WITH_MIKMOD)
  # Find libmikmod-config script
  if(NOT DEFINED LIBMIKMOD_CONFIG)
    set(LIBMIKMOD_CONFIG libmikmod-config)
  endif(NOT DEFINED LIBMIKMOD_CONFIG)
  find_program(LIBMIKMOD_CONFIG_EXECUTABLE ${LIBMIKMOD_CONFIG})
  if(LIBMIKMOD_CONFIG_EXECUTABLE)
    # run the libmikmod-config program to get cxxflags
    exec_program(sh
      ARGS "${LIBMIKMOD_CONFIG_EXECUTABLE} --cflags"
      OUTPUT_VARIABLE MIKMOD_CXX_FLAGS
      RETURN_VALUE RET)
    if(RET EQUAL 0)
      # parse definitions from cxxflags
      string(REGEX MATCHALL "-D.*[^ ;]+" MIKMOD_DEFINITIONS ${MIKMOD_CXX_FLAGS})
      # drop -D* from CXXFLAGS
      string(REGEX REPLACE "-D[^ ;]*" "" MIKMOD_CXX_FLAGS ${MIKMOD_CXX_FLAGS})
      # parse incdirs from cxxflags, drop -I prefix
      string(REGEX MATCHALL "-I.*[^ ;]+" MIKMOD_INCLUDE_DIRS ${MIKMOD_CXX_FLAGS})
      string(REGEX REPLACE "-I" "" MIKMOD_INCLUDE_DIRS "${MIKMOD_CXX_FLAGS}")
      # convert space to semicolons for list
      string(REGEX REPLACE " " ";" MIKMOD_INCLUDE_DIRS "${MIKMOD_INCLUDE_DIRS}")
    endif(RET EQUAL 0)

    # run the libmikmod-config program to get the libs
    exec_program(sh
      ARGS "${LIBMIKMOD_CONFIG_EXECUTABLE} --libs"
      OUTPUT_VARIABLE MIKMOD_LIBRARIES
      RETURN_VALUE RET)
    if(RET EQUAL 0)
      string(REGEX REPLACE " " ";" MIKMOD_LIBRARIES "${MIKMOD_LIBRARIES}")
      # extract linkdirs (-L) for rpath (i.e., LINK_DIRECTORIES)
      string(REGEX MATCHALL "-L[^ ;]+" MIKMOD_LIBRARY_DIRS "${MIKMOD_LIBRARIES}")
      string(REGEX REPLACE "-L" "" MIKMOD_LIBRARY_DIRS "${MIKMOD_LIBRARY_DIRS}")
      # convert space to semicolons for list
      STRING(REGEX REPLACE " " ";" MIKMOD_LIBRARY_DIRS "${MIKMOD_LIBRARY_DIRS}")
    endif(RET EQUAL 0)

    if(MIKMOD_INCLUDE_DIRS AND MIKMOD_LIBRARIES)
      message(STATUS "Found MIKMOD: ${MIKMOD_LIBRARIES}")
      include_directories(${MIKMOD_INCLUDE_DIRS})
      set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_MIKMOD_SOURCES})
      set(MAIN_LIBS ${MIKMOD_LIBRARIES} ${MAIN_LIBS})
    else(MIKMOD_INCLUDE_DIRS AND MIKMOD_LIBRARIES)
      message(STATUS "Could NOT find MIKMOD libraries")
    endif(MIKMOD_INCLUDE_DIRS AND MIKMOD_LIBRARIES)
  endif(LIBMIKMOD_CONFIG_EXECUTABLE)
endif(WITH_MIKMOD)

#
#  **** Check for ModPlug ****
#

if(WITH_MODPLUG)
  FIND_PACKAGE ( PkgConfig REQUIRED )
  PKG_CHECK_MODULES ( MODPLUG libmodplug )
    if(MODPLUG_INCLUDE_DIRS AND MODPLUG_LIBRARIES)
      message(STATUS "Found MODPLUG: ${MODPLUG_LIBRARIES}")
      include_directories(${MODPLUG_INCLUDE_DIRS})
      set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_MODPLUG_SOURCES})
      set(MAIN_LIBS ${MODPLUG_LIBRARIES} ${MAIN_LIBS})
    else(MODPLUG_INCLUDE_DIRS AND MODPLUG_LIBRARIES)
      message(STATUS "Could NOT find MODPLUG libraries")
    endif(MODPLUG_INCLUDE_DIRS AND MODPLUG_LIBRARIES)
endif(WITH_MODPLUG)

#
#  **** Check for FLAC ****
#

if(WITH_FLAC)
  find_path(FLAC_INCLUDE_DIR FLAC++/decoder.h)
  find_library(FLAC_LIBRARY NAMES FLAC)
  find_library(FLACPP_LIBRARY NAMES FLAC++)
  if(FLAC_INCLUDE_DIR AND FLAC_LIBRARY AND FLACPP_LIBRARY)
    message(STATUS "Found FLAC: ${FLAC_LIBRARY} ${FLACPP_LIBRARY}")
    include_directories(${FLAC_INCLUDE_DIR})
    set(MAIN_SOURCES ${MAIN_SOURCES} ${MAIN_FLAC_SOURCES})
    set(MAIN_LIBS ${FLACPP_LIBRARY} ${FLAC_LIBRARY} ${MAIN_LIBS})
  else(FLAC_INCLUDE_DIR AND FLAC_LIBRARY AND FLACPP_LIBRARY)
    message(STATUS "Could NOT find FLAC libraries")
  endif(FLAC_INCLUDE_DIR AND FLAC_LIBRARY AND FLACPP_LIBRARY)
endif(WITH_FLAC)


if(WITH_GME)
  add_subdirectory(gme)
  set(MAIN_SOURCES ${MAIN_SOURCES} sound/snd_gme.cpp)
endif(WITH_GME)


add_executable(vavoom ${MAIN_SOURCES})
if(ENABLE_WRAPPERS)
set_target_properties(vavoom PROPERTIES OUTPUT_NAME ../vavoom.bin)
else(ENABLE_WRAPPERS)
set_target_properties(vavoom PROPERTIES OUTPUT_NAME ../vavoom)
endif(ENABLE_WRAPPERS)
if (MAIN_COMPILE_FLAGS)
  set_target_properties(vavoom PROPERTIES COMPILE_FLAGS ${MAIN_COMPILE_FLAGS})
endif (MAIN_COMPILE_FLAGS)
target_link_libraries(vavoom ${MAIN_LIBS})
add_dependencies(vavoom core timidity gme glbsp libglvis lzma ${ZLIB_DEPS})

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
  set(SERVER_SOURCES ${SERVER_SOURCES})
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
target_link_libraries(vavoom-dedicated glbsp core lzma ${ZLIB_LIBRARY} ${NET_LIBRARIES})
add_dependencies(vavoom-dedicated core glbsp lzma)

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
