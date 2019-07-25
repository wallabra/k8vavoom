set(OUTDIR ${CMAKE_BINARY_DIR}/basev/doom1/progs)
set(COMMON_PROGS_DIR ${CMAKE_BINARY_DIR}/basev/common/progs)

add_custom_command(OUTPUT ${OUTDIR}/game.dat
  COMMAND mkdir ARGS -p ${OUTDIR}
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${COMMON_PROGS_DIR} -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/game/classes.vc ${OUTDIR}/game.dat
  DEPENDS
    vcc
    game/*.vc
    #${COMMON_PROGS_DIR}/engine.dat
    ${COMMON_PROGS_DIR}/linespec.dat
    ${COMMON_PROGS_DIR}/newui.dat
    ${COMMON_PROGS_DIR}/uibase.dat)
add_custom_target(doom_game ALL DEPENDS ${OUTDIR}/game.dat)
#add_dependencies(doom_game vcc engine linespec uibase)
add_dependencies(doom_game vcc linespec uibase)

add_custom_command(OUTPUT ${OUTDIR}/cgame.dat
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${COMMON_PROGS_DIR} -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/cgame/classes.vc ${OUTDIR}/cgame.dat
  DEPENDS
    vcc
    cgame/*.vc
    ${OUTDIR}/game.dat
    #${COMMON_PROGS_DIR}/engine.dat
    ${COMMON_PROGS_DIR}/linespec.dat
    ${COMMON_PROGS_DIR}/newui.dat
    ${COMMON_PROGS_DIR}/uibase.dat)
add_custom_target(doom_cgame ALL DEPENDS ${OUTDIR}/cgame.dat)
#add_dependencies(doom_cgame vcc engine linespec uibase doom_game)
add_dependencies(doom_cgame vcc linespec uibase doom_game)
