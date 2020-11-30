#version 120
$include "common/common.inc"

uniform mat4 LightMPV;
uniform mat4 LightView;

varying vec3 VertWorldPos;


void main () {
  gl_Position = LightMPV*gl_Vertex;
  //VertWorldPos = gl_Vertex.xyz;
  //VertWorldPos = (gl_ModelViewMatrix*gl_Vertex).xyz;
  VertWorldPos = (LightView*gl_Vertex).xyz;
}
