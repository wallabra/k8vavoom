#get_target_property(VLUMPY vlumpy $<TARGET_FILE>)
#get_target_property(ACC acc $<TARGET_FILE>)

add_subdirectory(common)
add_subdirectory(doom)
add_subdirectory(doom1)
add_subdirectory(doom2)
add_subdirectory(heretic)
add_subdirectory(hexen)
add_subdirectory(nerve)
add_subdirectory(plutonia)
#add_subdirectory(strife)
add_subdirectory(tnt)

add_subdirectory(mods)

install(FILES games.txt DESTINATION ${DATADIR}/basev)
