configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bdw.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    *.txt
    actors/*.txt
    actors/gl/*.gl
    loadvcs.txt
    progs/k8bdw/*
    models/*
    models/tracer/*
  )
add_custom_target(bdw_modpak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bdw.pk3)
add_dependencies(bdw_modpak doom_game doom_cgame doom2_cgame heretic_cgame hexen_cgame strife_cgame)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bdw.pk3
  DESTINATION ${DATADIR}/basev/mods/bdw)
