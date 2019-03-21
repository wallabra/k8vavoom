#version 120
$include "common/common.inc"

uniform float Inter;
uniform vec3 LightPos;
uniform mat4 ModelToWorldMat;

attribute vec4 Vert2;
attribute float Offset; //0 means "offset to infinity"

/*
varying vec3 VertToView;
varying vec3 VPosL;
varying vec3 VPos;
varying float Offs;

uniform vec3 ViewOrigin;
*/


void main () {
  /*
  vec4 Vert = mix(gl_Vertex, Vert2, Inter)*ModelToWorldMat;
  if (Offset > 0.0) Vert.xyz = LightPos+(Offset*normalize(Vert.xyz-LightPos));
  gl_Position = gl_ModelViewProjectionMatrix*Vert;
  */

  vec4 Vert = mix(gl_Vertex, Vert2, Inter)*ModelToWorldMat;
  // transforming the vertex
  // t will be 1 for w == 0, and 0 otherwise
  float t = 1.0-Offset;
  gl_Position = gl_ModelViewProjectionMatrix*vec4(Vert.xyz-LightPos*t, 1.0-t);

  /*
  VertToView = ViewOrigin-Vert.xyz;
  VPosL = LightPos-gl_Position.xyz;
  VPos = ViewOrigin-gl_Position.xyz;
  Offs = Offset;
  */
}
