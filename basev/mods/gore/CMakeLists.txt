configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gore.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    *.txt
    models/models.xml
    models/gore/*
    models/blood/*
    progs/k8gore/*
    sprites/gore/*
  )
add_custom_target(gore_modpak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gore.pk3)
add_dependencies(gore_modpak doom_game doom_cgame doom2_cgame heretic_cgame hexen_cgame strife_cgame)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gore.pk3
  DESTINATION ${DATADIR}/basev/mods/gore)
