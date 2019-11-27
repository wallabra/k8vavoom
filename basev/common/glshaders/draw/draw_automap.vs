#version 120
$include "common/common.inc"

varying vec4 Color;


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  // pass color
  Color = gl_Color;
}
