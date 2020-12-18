#version 120
$include "common/common.inc"

#ifdef VV_SMAP_TEXTURED
 #ifdef VV_SMAP_NOBUF
  $include "common/texture_vars.vs"
 #else

attribute vec3 SAxis;
attribute vec3 TAxis;
attribute vec4 TexOfsSize; // .x=SOffs; .y=TOffs; .z=TexIW; .w=TexIH
// put this also into fragment shader
varying vec2 TextureCoordinate;

#define SOffs TexOfsSize.x
#define TOffs TexOfsSize.y
#define TexIW TexOfsSize.z
#define TexIH TexOfsSize.w

 #endif
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
  //$include "common/texture_calc_pos.vs"

  // calculate texture coordinates
  TextureCoordinate = vec2(
    (dot(Position, SAxis)+SOffs)*TexIW,
    (dot(Position, TAxis)+TOffs)*TexIH
  );

 #endif
#endif
}
