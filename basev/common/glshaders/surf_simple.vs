#version 120

// vertex shader for simple (non-lightmapped) surfaces
$include "common/texture_vars.vs"


void main () {
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  $include "common/texture_calc.vs"
}
