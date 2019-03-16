#version 120
$include "common/common.inc"

varying vec4 Colour;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  // pass color
  Colour = gl_Color;
}
