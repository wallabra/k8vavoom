#version 120
$include "common/common.inc"

attribute vec3 Position;

uniform mat4 ModelToWorldMat;
uniform float Inter;

attribute vec4 Vert2;
attribute vec2 TexCoord;

varying vec2 TextureCoordinate;


void main () {
  vec4 Vert = mix(vec4(Position, 1.0), Vert2, Inter)*ModelToWorldMat;
  gl_Position = gl_ModelViewProjectionMatrix*Vert;

  TextureCoordinate = TexCoord;
}
