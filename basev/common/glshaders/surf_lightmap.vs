#version 120

// vertex shader for lightmapped surfaces
$include "common/texlmap_vars.vs"


void main () {
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  $include "common/texlmap_calc.vs"
}
