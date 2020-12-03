#version 120
$include "common/common.inc"

$include "shadowvol/smap_builder_decl.fs"

uniform sampler2D Texture;
varying vec2 TextureCoordinate;

#ifndef VV_SMAP_TEXTURED
# define VV_SMAP_TEXTURED
#endif

void main () {
  $include "shadowvol/smap_builder_calc.fs"
}
