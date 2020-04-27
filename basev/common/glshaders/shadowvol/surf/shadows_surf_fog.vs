#version 120
$include "common/common.inc"

attribute vec3 Position;

#ifdef VV_MASKED_FOG
$include "common/texture_vars.vs"
#endif


void main () {
  // transforming the vertex
  //gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  gl_Position = gl_ModelViewProjectionMatrix*vec4(Position, 1.0);
  #ifdef VV_MASKED_FOG
  // pass texture coordinates
  $include "common/texture_calc_pos.vs"
  #endif
}
