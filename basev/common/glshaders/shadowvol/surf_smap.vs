#version 120
$include "common/common.inc"

//uniform mat4 LightMPV;
uniform mat4 LightProj;
uniform mat4 LightView;
//uniform vec3 LightPos;

varying vec3 VertLightDir;


void main () {
  // transforming the vertex
  //gl_Position = LightMPV*vec4(gl_Vertex.xyz, 1);
  mat4 vp = LightProj*LightView;
  gl_Position = vp*gl_Vertex;
  //VertLightDir = gl_Vertex.xyz-LightPos;
  VertLightDir = (LightView*gl_Vertex).xyz;
}
