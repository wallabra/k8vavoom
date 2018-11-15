set(OUTDIR ${CMAKE_BINARY_DIR}/basev/common/progs)

add_custom_command(OUTPUT ${OUTDIR}/engine.dat
  COMMAND mkdir ARGS -p ${OUTDIR}
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/engine/classes.vc ${OUTDIR}/engine.dat
  DEPENDS vcc engine/*.vc)
add_custom_target(engine ALL DEPENDS ${OUTDIR}/engine.dat)
add_dependencies(engine vcc)

add_custom_command(OUTPUT ${OUTDIR}/linespec.dat
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/linespec/classes.vc ${OUTDIR}/linespec.dat
  DEPENDS
    vcc
    linespec/*.vc
    linespec/actor/*.vc
    linespec/armor/*.vc
    linespec/basic/*.vc
    linespec/entityex/*.vc
    linespec/health/*.vc
    linespec/heretic/*.vc
    linespec/hexen/*.vc
    linespec/incomplete/*.vc
    linespec/player/*.vc
    linespec/powerup/*.vc
    linespec/raven/*.vc
    linespec/strife/*.vc
    linespec/weapon/*.vc
    ${OUTDIR}/engine.dat)
add_custom_target(linespec ALL DEPENDS ${OUTDIR}/linespec.dat)
add_dependencies(linespec vcc engine)

add_custom_command(OUTPUT ${OUTDIR}/uibase.dat
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/uibase/classes.vc ${OUTDIR}/uibase.dat
  DEPENDS
    vcc
    uibase/*.vc
    uibase/hud/*.vc
    uibase/condlg/*.vc
    uibase/finale/*.vc
    uibase/client/*.vc
    uibase/menus/*.vc
    uibase/wgchoice/*.vc
    uibase/wgslotsl/*.vc
    uibase/wgselector/*.vc
    uibase/wgtextbtn/*.vc
    ${OUTDIR}/engine.dat)
add_custom_target(uibase ALL DEPENDS ${OUTDIR}/uibase.dat)
add_dependencies(uibase vcc engine linespec)
