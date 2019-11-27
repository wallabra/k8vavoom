#version 120
$include "common/common.inc"

#ifdef VV_MASKED_FOG
$include "common/texture_vars.vs"
#endif


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  #ifdef VV_MASKED_FOG
  // pass texture coordinates
  $include "common/texture_calc.vs"
  #endif
}
