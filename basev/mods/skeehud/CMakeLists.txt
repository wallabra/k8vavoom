configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/skeehud.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    *.txt
    progs/*
    progs/skulldashhud/*
  )
add_custom_target(skeehud ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/skeehud.pk3)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/skeehud.pk3
  DESTINATION ${DATADIR}/basev/mods/skeehud)
