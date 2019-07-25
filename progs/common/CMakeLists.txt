set(OUTDIR ${CMAKE_BINARY_DIR}/basev/common/progs)

### add_custom_command(OUTPUT ${OUTDIR}/engine.dat
###   COMMAND mkdir ARGS -p ${OUTDIR}
###   COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -D -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/engine/classes.vc ${OUTDIR}/engine.dat
###   DEPENDS
###     vcc
###     engine/*.vc)
### add_custom_target(engine ALL DEPENDS ${OUTDIR}/engine.dat)
### add_dependencies(engine vcc)

add_custom_command(OUTPUT ${OUTDIR}/newui.dat
  COMMAND mkdir ARGS -p ${OUTDIR}
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/newui/classes.vc ${OUTDIR}/newui.dat
  DEPENDS
    vcc
    engine/*.vc
    newui/*.vc)
    #${OUTDIR}/engine.dat)
add_custom_target(newui ALL DEPENDS ${OUTDIR}/newui.dat)
#add_dependencies(newui vcc engine)
add_dependencies(newui vcc)

add_custom_command(OUTPUT ${OUTDIR}/linespec.dat
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/linespec/classes.vc ${OUTDIR}/linespec.dat
  DEPENDS
    vcc
    engine/*.vc
    botai/*.vc
    linespec/*.vc
    linespec/actor/*.vc
    linespec/armor/*.vc
    linespec/basic/*.vc
    linespec/bridge/*.vc
    linespec/camera/*.vc
    linespec/dynlight/*.vc
    linespec/entityex/*.vc
    linespec/health/*.vc
    linespec/heretic/*.vc
    linespec/hexen/*.vc
    linespec/incomplete/*.vc
    linespec/linespec/*.vc
    linespec/particle/*.vc
    linespec/player/*.vc
    linespec/polythink/*.vc
    linespec/portals/*.vc
    linespec/powerup/*.vc
    linespec/raven/*.vc
    linespec/secaction/*.vc
    linespec/secthink/*.vc
    linespec/secthink/lights/*.vc
    linespec/secthink/movers/*.vc
    linespec/sound/*.vc
    linespec/strife/*.vc
    linespec/weapon/*.vc)
    #${OUTDIR}/engine.dat)
add_custom_target(linespec ALL DEPENDS ${OUTDIR}/linespec.dat)
#add_dependencies(linespec vcc engine)
add_dependencies(linespec vcc)

add_custom_command(OUTPUT ${OUTDIR}/uibase.dat
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/uibase/classes.vc ${OUTDIR}/uibase.dat
  DEPENDS
    vcc
    engine/*.vc
    uibase/*.vc
    uibase/client/*.vc
    uibase/condlg/*.vc
    uibase/finale/*.vc
    uibase/hud/*.vc
    uibase/menus/*.vc
    uibase/wgchoice/*.vc
    uibase/wgselector/*.vc
    uibase/wgslotsl/*.vc
    uibase/wgtextbtn/*.vc
    #${OUTDIR}/engine.dat
    ${OUTDIR}/linespec.dat
    ${OUTDIR}/newui.dat)
add_custom_target(uibase ALL DEPENDS ${OUTDIR}/uibase.dat)
#add_dependencies(uibase vcc engine linespec newui)
add_dependencies(uibase vcc linespec newui)
