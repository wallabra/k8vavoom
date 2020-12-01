#version 120
$include "common/common.inc"

uniform mat4 LightMPV;
uniform mat4 LightView;
#ifdef VV_SMAP_TEXTURED
$include "common/texture_vars.vs"
#endif

varying vec3 VertWorldPos;


void main () {
  gl_Position = LightMPV*gl_Vertex;
#ifdef VV_SMAP_TEXTURED
  $include "common/texture_calc.vs"
#endif
  //VertWorldPos = gl_Vertex.xyz;
  //VertWorldPos = (gl_ModelViewMatrix*gl_Vertex).xyz;
  VertWorldPos = (LightView*gl_Vertex).xyz;
}
