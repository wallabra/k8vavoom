#version 120
$include "common/common.inc"

$include "shadowvol/smap_builder_decl.fs"

#ifdef VV_SMAP_TEXTURED
uniform sampler2D Texture;
 #ifdef VV_SMAP_NOBUF
  $include "common/texture_vars.fs"
 #else

varying vec2 TextureCoordinate;

 #endif
#endif


void main () {
  $include "shadowvol/smap_builder_calc.fs"
}
