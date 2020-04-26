#version 120
$include "common/common.inc"

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
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
#ifdef VV_AMBIENT_MASKED_WALL
  $include "common/texture_calc.vs"
#endif
#ifdef VV_AMBIENT_BRIGHTMAP_WALL
  $include "common/texture_calc.vs"
#endif
//#ifdef VV_AMBIENT_GLOW
  $include "common/glow_calc.vs"
//#endif
}
