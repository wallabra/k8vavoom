configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gore.pk3
  COMMAND ${WINERUN} ${VLUMPY} ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    blood.blue.transient.txt
    blood.blue.txt
    blood.green.transient.txt
    blood.green.txt
    blood.red.transient.txt
    blood.red.txt
    blood.underwater.txt
    decorate.txt
    sndinfo.txt
    decaldef.brutalbloodsplats.txt
  )
add_custom_target(gore_modpak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gore.pk3)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gore.pk3
  DESTINATION ${DATADIR}/basev/mods/gore)
