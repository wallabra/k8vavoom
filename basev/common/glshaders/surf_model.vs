#version 120
$include "common/common.inc"

//uniform vec3 ViewOrigin;
uniform mat4 ModelToWorldMat;
uniform float Inter;

attribute vec4 Vert2;
attribute vec4 LightVal;
attribute vec2 TexCoord;

varying vec4 Light;
//varying vec3 VertToView;
//varying vec3 VPos;
varying vec2 TextureCoordinate;


void main () {
#if 0
  vec4 Vert = mix(gl_Vertex, Vert2, Inter);
#else
  vec4 Vert = mix(gl_Vertex, Vert2, Inter)*ModelToWorldMat;
  //vec4 Vert = ModelToWorldMat*mix(gl_Vertex, Vert2, Inter);
#endif
  gl_Position = gl_ModelViewProjectionMatrix*Vert;

  //VertToView = ViewOrigin-Vert.xyz;
  //VPos = ViewOrigin-gl_Position.xyz;
  Light = LightVal;
  TextureCoordinate = TexCoord;
}
