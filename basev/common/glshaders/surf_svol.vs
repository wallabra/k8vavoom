#version 120
$include "common/common.inc"

uniform vec3 LightPos;


void main () {
  // t will be 1 for w == 0, and 0 otherwise
  float t = 1.0-gl_Vertex.w;
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*vec4(gl_Vertex.xyz-LightPos*t, gl_Vertex.w);
}
