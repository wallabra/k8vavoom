set(OUTDIR ${CMAKE_BINARY_DIR}/basev/common/progs)

### add_custom_command(OUTPUT ${OUTDIR}/engine.dat
###   COMMAND mkdir ARGS -p ${OUTDIR}
###   COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -D -P${OUTDIR} ${CMAKE_CURRENT_SOURCE_DIR}/engine/classes.vc ${OUTDIR}/engine.dat
###   DEPENDS
###     vcc
###     engine/*.vc)
### add_custom_target(engine ALL DEPENDS ${OUTDIR}/engine.dat)
### add_dependencies(engine vcc)

### -- not yet
### add_custom_command(OUTPUT ${OUTDIR}/newui.dat
###   COMMAND mkdir ARGS -p ${OUTDIR}
###   COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${OUTDIR} -P${PROJECT_SOURCE_DIR}/progs/common ${CMAKE_CURRENT_SOURCE_DIR}/newui/classes.vc ${OUTDIR}/newui.dat
###   DEPENDS
###     vcc
###     ${CMAKE_SOURCE_DIR}/progs/common/engine/*.vc
###     ${CMAKE_SOURCE_DIR}/progs/common/newui/*.vc)
###     #${OUTDIR}/engine.dat)
### add_custom_target(newui ALL DEPENDS ${OUTDIR}/newui.dat)
### #add_dependencies(newui vcc engine)
### add_dependencies(newui vcc)

add_custom_command(OUTPUT ${OUTDIR}/linespec.dat
  COMMAND mkdir ARGS -p ${OUTDIR}
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${OUTDIR} -P${PROJECT_SOURCE_DIR}/progs/common ${CMAKE_CURRENT_SOURCE_DIR}/linespec/classes.vc ${OUTDIR}/linespec.dat
  DEPENDS
    vcc
    ${CMAKE_SOURCE_DIR}/progs/common/engine/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/botai/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/actor/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/armor/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/basic/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/bridge/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/camera/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/dynlight/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/entityex/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/health/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/heretic/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/hexen/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/incomplete/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/linespec/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/particle/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/player/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/polythink/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/portals/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/powerup/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/raven/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/secaction/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/secthink/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/secthink/lights/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/secthink/movers/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/sound/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/strife/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/weapon/*.vc)
    #${OUTDIR}/engine.dat)
add_custom_target(linespec ALL DEPENDS ${OUTDIR}/linespec.dat)
#add_dependencies(linespec vcc engine)
add_dependencies(linespec vcc)

add_custom_command(OUTPUT ${OUTDIR}/uibase.dat
  COMMAND mkdir ARGS -p ${OUTDIR}
  COMMAND ${WINERUN} $<TARGET_FILE:vcc> ARGS -P${OUTDIR} -P${PROJECT_SOURCE_DIR}/progs/common ${CMAKE_CURRENT_SOURCE_DIR}/uibase/classes.vc ${OUTDIR}/uibase.dat
  DEPENDS
    vcc
    ${CMAKE_SOURCE_DIR}/progs/common/engine/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/client/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/condlg/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/finale/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/hud/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/menus/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/wgchoice/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/wgselector/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/wgslotsl/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/uibase/wgtextbtn/*.vc
    #${OUTDIR}/engine.dat
    ${OUTDIR}/linespec.dat
    #${OUTDIR}/newui.dat -- not yet
    )
add_custom_target(uibase ALL DEPENDS ${OUTDIR}/uibase.dat)
#add_dependencies(uibase vcc engine linespec newui)
#add_dependencies(uibase vcc linespec newui)
add_dependencies(uibase vcc linespec)
