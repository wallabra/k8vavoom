configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bdw.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    decaldef.txt
    decorate.txt
    gldefs.txt
    keyconf.txt
    sndinfo.txt
    actors/0common.txt
    actors/casing.txt
    actors/lights.gl
    actors/muzzle.gl
    actors/muzzle.txt
    actors/puffs.gl
    actors/puffs.txt
    actors/rifle.txt
    actors/shotgun.txt
    actors/tracers.txt
    actors/water.txt
  )
add_custom_target(bdw_modpak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bdw.pk3)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bdw.pk3
  DESTINATION ${DATADIR}/basev/mods/bdw)
