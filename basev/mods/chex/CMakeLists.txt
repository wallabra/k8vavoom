configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/chex.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    *.txt
    language.en
    actors/chex/*
    progs/chex/*
  )
add_custom_target(chex_modpak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/chex.pk3)
add_dependencies(chex_modpak doom_game doom_cgame doom2_cgame)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/chex.pk3
  DESTINATION ${DATADIR}/basev/mods/chex)
