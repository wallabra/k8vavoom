configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/models/models.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/models/gore/*
    ${CMAKE_CURRENT_SOURCE_DIR}/models/blood/*
    ${CMAKE_CURRENT_SOURCE_DIR}/progs/k8gore/*
    ${CMAKE_CURRENT_SOURCE_DIR}/sprites/gore/*
    ${CMAKE_CURRENT_SOURCE_DIR}/graphics/BloodDecals/*
  )
add_custom_target(gore_modpak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3)
if(ENABLE_VCC)
  add_dependencies(gore_modpak doom_game doom_cgame doom2_cgame heretic_cgame hexen_cgame strife_cgame)
endif(ENABLE_VCC)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  DESTINATION ${DATADIR}/basev/mods/gore)
