set(OUTDIR ${CMAKE_BINARY_DIR}/basev/heretic/progs)
set(COMMON_PROGS_DIR ${CMAKE_BINARY_DIR}/basev/common/progs)

add_custom_command(OUTPUT ${OUTDIR}/game.dat
  COMMAND mkdir ARGS -p ${OUTDIR}
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${COMMON_PROGS_DIR} -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/game/classes.vc ${OUTDIR}/game.dat
  DEPENDS
    vcc
    game/*.vc
    ${COMMON_PROGS_DIR}/engine.dat
    ${COMMON_PROGS_DIR}/linespec.dat
    ${COMMON_PROGS_DIR}/newui.dat
    ${COMMON_PROGS_DIR}/uibase.dat)
add_custom_target(heretic_game ALL DEPENDS ${OUTDIR}/game.dat)
add_dependencies(heretic_game vcc engine linespec uibase)

add_custom_command(OUTPUT ${OUTDIR}/cgame.dat
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${COMMON_PROGS_DIR} -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/cgame/classes.vc ${OUTDIR}/cgame.dat
  DEPENDS
    vcc
    cgame/*.vc
    ${OUTDIR}/game.dat
    ${COMMON_PROGS_DIR}/engine.dat
    ${COMMON_PROGS_DIR}/linespec.dat
    ${COMMON_PROGS_DIR}/newui.dat
    ${COMMON_PROGS_DIR}/uibase.dat)
add_custom_target(heretic_cgame ALL DEPENDS ${OUTDIR}/cgame.dat)
add_dependencies(heretic_cgame vcc engine linespec uibase heretic_game)
