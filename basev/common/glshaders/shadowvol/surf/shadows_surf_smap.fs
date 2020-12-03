#version 120
$include "common/common.inc"

$include "shadowvol/smap_builder_decl.fs"

#ifdef VV_SMAP_TEXTURED
uniform sampler2D Texture;
$include "common/texture_vars.fs"
#endif


void main () {
  $include "shadowvol/smap_builder_calc.fs"
}
