#version 120
$include "common/common.inc"

attribute vec3 Position;

$include "common/texture_vars.vs"


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*vec4(Position, 1.0);
  // pass texture coordinates
  $include "common/texture_calc_pos.vs"
}
