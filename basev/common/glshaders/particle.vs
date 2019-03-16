#version 120
$include "common/common.inc"

attribute vec4 LightVal;
attribute vec2 TexCoord;

varying vec4 Light;
varying vec2 TextureCoordinate;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  // pass light
  Light = LightVal;
  // pass texture coordinates
  TextureCoordinate = TexCoord;
}
