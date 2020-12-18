#version 120
$include "common/common.inc"

#ifdef VV_SMAP_TEXTURED
$include "common/texture_vars.vs"
#endif

$include "shadowvol/smap_builder_decl.vs"


void main () {
  #ifdef VV_SMAP_NOBUF
  vec4 Vert = gl_Vertex;
  #else
  vec4 Vert = vec4(Position.xyz, 1.0);
  #endif
  $include "shadowvol/smap_builder_calc.vs"
#ifdef VV_SMAP_TEXTURED
 #ifdef VV_SMAP_NOBUF
  $include "common/texture_calc.vs"
 #else
  $include "common/texture_calc_pos.vs"
 #endif
#endif
}
