#version 120
$include "common/common.inc"

uniform mat4 LightMPV;
uniform mat4 LightView;
$include "common/texture_vars.vs"

varying vec3 VertWorldPos;


void main () {
  gl_Position = LightMPV*gl_Vertex;
  $include "common/texture_calc.vs"
  //VertWorldPos = gl_Vertex.xyz;
  //VertWorldPos = (gl_ModelViewMatrix*gl_Vertex).xyz;
  VertWorldPos = (LightView*gl_Vertex).xyz;
}
