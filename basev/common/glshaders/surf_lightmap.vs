#version 120
$include "common/common.inc"

// vertex shader for lightmapped surfaces
$include "common/texlmap_vars.vs"

//#ifdef VV_AMBIENT_GLOW
$include "common/glow_vars.vs"
//#endif


void main () {
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  $include "common/texlmap_calc.vs"
//#ifdef VV_AMBIENT_GLOW
  $include "common/glow_calc.vs"
//#endif
}
