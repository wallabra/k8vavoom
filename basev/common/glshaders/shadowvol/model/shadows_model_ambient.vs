#version 120
$include "common/common.inc"

uniform mat4 ModelToWorldMat;
uniform float Inter;

attribute vec4 Vert2;
attribute vec2 TexCoord;

varying vec2 TextureCoordinate;


void main () {
  vec4 Vert = mix(gl_Vertex, Vert2, Inter)*ModelToWorldMat;
  gl_Position = gl_ModelViewProjectionMatrix*Vert;

  TextureCoordinate = TexCoord;
}
