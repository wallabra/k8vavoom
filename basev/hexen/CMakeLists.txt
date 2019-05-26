configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    graphics/croshai1.lmp
    graphics/croshai2.lmp
    graphics/croshai3.lmp
    graphics/fonta30.lmp
    graphics/fonta63.lmp
    palette.pcx
    basepak.ls.in
    *.txt
    ${CMAKE_SOURCE_DIR}/progs/hexen/cgame/*.vc
    ${CMAKE_SOURCE_DIR}/progs/hexen/game/*.vc
  )
add_custom_target(hexen_basepak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3)
add_dependencies(hexen_basepak hexen_game hexen_cgame)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  DESTINATION ${DATADIR}/basev/hexen)
