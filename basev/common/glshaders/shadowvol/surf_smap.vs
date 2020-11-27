#version 120
$include "common/common.inc"

uniform mat4 LightMPV;
//uniform vec3 LightPos;


void main () {
  // transforming the vertex
  gl_Position = LightMPV*vec4(gl_Vertex.xyz, 1);
}
