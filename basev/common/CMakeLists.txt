configure_file(basepak.ls.in ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  COMMAND ${WINERUN} $<TARGET_FILE:vlumpy> ARGS basepak.ls
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.ls vlumpy
    basepak.ls.in
    *.txt
    confont.pcx
    default.cfg
    language.en
    startup.vs
    vavoom_decorate_defs.xml

    credits/*.txt

    actors/*.txt
    actors/doom/*
    actors/heretic/*.txt
    actors/hexen/*.txt
    actors/raven/*.txt
    actors/shared/*.txt
    actors/strife/*.txt

    fxdefs/common/*.txt
    fxdefs/doom/*.txt
    fxdefs/heretic/*.txt
    fxdefs/hexen/*.txt
    fxdefs/raven/*.txt
    fxdefs/strife/*.txt

    graphics/*.png
    graphics/*.lmp
    graphics/progbar/*.png
    graphics/healthbar/*.png

    sounds/*

    sprites/iceca0.png
    sprites/icecb0.png
    sprites/icecc0.png
    sprites/icecd0.png
    sprites/tnt1a0.lmp
    sprites/unkna0.png
    sprites/dogs/*

    textures/-noflat-.png

    uidef/*.txt
    uidef/menus/*.txt

    ${CMAKE_SOURCE_DIR}/progs/common/botai/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/engine/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/engine/vccrun/*.vc
    ${CMAKE_SOURCE_DIR}/progs/common/newui/*.vc

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
    ${CMAKE_SOURCE_DIR}/progs/common/linespec/weapon/*.vc

    glshaders/*.fs
    glshaders/*.vs
    glshaders/common/*.fs
    glshaders/common/*.vs
    glshaders/common/*.inc
  )
add_custom_target(common_basepak ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/basepak.pk3
  DESTINATION ${DATADIR}/basev/common)
