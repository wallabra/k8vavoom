#version 120
$include "common/common.inc"

//attribute vec2 TexCoord;

$include "common/texture_vars.vs"


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  // pass texture coordinates
  //TextureCoordinate = TexCoord;
  $include "common/texture_calc.vs"
}
