#version 120
$include "common/common.inc"

attribute vec3 Position;

#ifdef VV_AMBIENT_MASKED_WALL
$include "common/texture_vars.vs"
#else
# ifdef VV_AMBIENT_BRIGHTMAP_WALL
$include "common/texture_vars.vs"
# endif
#endif

//#ifdef VV_AMBIENT_GLOW
$include "common/glow_vars.vs"
//#endif


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*vec4(Position, 1.0);
#ifdef VV_AMBIENT_MASKED_WALL
  $include "common/texture_calc_pos.vs"
#endif
#ifdef VV_AMBIENT_BRIGHTMAP_WALL
  $include "common/texture_calc_pos.vs"
#endif
//#ifdef VV_AMBIENT_GLOW
  $include "common/glow_calc_pos.vs"
//#endif
}
