configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    graphics/autopage.lmp
    ${CMAKE_CURRENT_SOURCE_DIR}/graphics/croshai*.*
    ${CMAKE_CURRENT_SOURCE_DIR}/graphics/croshai*
    basepak.ls.in
    bigfont.pcx
    #animated.lmp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
    palette.pcx
    ${CMAKE_SOURCE_DIR}/progs/doom/cgame/*.vc
    ${CMAKE_SOURCE_DIR}/progs/doom/game/*.vc
  )
add_custom_target(doom_basepak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3)
if(ENABLE_VCC)
  add_dependencies(doom_basepak doom_game doom_cgame)
endif(ENABLE_VCC)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  DESTINATION ${DATADIR}/basev/doom)
