#version 120
$include "common/common.inc"

attribute vec3 Position;

$include "common/texture_vars.vs"


void main () {
  // transforming the vertex
  //gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  gl_Position = gl_ModelViewProjectionMatrix*vec4(Position, 1.0);
  // pass texture coordinates
  //TextureCoordinate = TexCoord;
  $include "common/texture_calc_pos.vs"
}
