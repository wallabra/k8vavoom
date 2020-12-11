#version 120
$include "common/common.inc"

#ifdef VV_SMAP_TEXTURED
$include "common/texture_vars.vs"
#endif

$include "shadowvol/smap_builder_decl.vs"


void main () {
  //vec4 Vert = gl_Vertex;
  #define Vert gl_Vertex
  $include "shadowvol/smap_builder_calc.vs"
#ifdef VV_SMAP_TEXTURED
  $include "common/texture_calc.vs"
#endif
}
