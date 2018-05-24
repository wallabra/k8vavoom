cmake_minimum_required( VERSION 2.8.7 )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -fomit-frame-pointer -Wno-shift-negative-value -Wno-unused-function" )

add_library( jpeg
    jaricom.c
    jcomapi.c
    jdapimin.c
    jdapistd.c
    jdarith.c
    jdatasrc.c
    jdcoefct.c
    jdcolor.c
    jddctmgr.c
    jdhuff.c
    jdinput.c
    jdmainct.c
    jdmarker.c
    jdmaster.c
    jdmerge.c
    jdpostct.c
    jdsample.c
    jerror.c
    jidctflt.c
    jidctfst.c
    jidctint.c
    jmemansi.c
    jmemmgr.c
    jquant1.c
    jquant2.c
    jutils.c )
target_link_libraries( jpeg )
