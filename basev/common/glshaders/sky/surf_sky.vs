#version 120
$include "common/common.inc"

attribute vec3 Position;
attribute vec2 TexCoord;

varying vec2 TextureCoordinate;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*vec4(Position, 1.0);
  // pass texture coordinates
  TextureCoordinate = TexCoord;
}
