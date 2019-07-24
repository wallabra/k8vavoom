#version 120
$include "common/common.inc"

uniform vec4 Light;

#ifdef VV_AMBIENT_MASKED_WALL
uniform sampler2D Texture;
$include "common/texture_vars.fs"
#endif

#ifdef VV_AMBIENT_BRIGHTMAP_WALL
uniform sampler2D Texture;
$include "common/brightmap_vars.fs"
$include "common/texture_vars.fs"
#endif


//#ifdef VV_AMBIENT_GLOW
$include "common/glow_vars.fs"
//#endif


void main () {
//#ifdef VV_AMBIENT_GLOW
  vec4 lt = calcGlow(Light);
//#else
//  vec4 lt = Light;
//#endif
#ifdef VV_AMBIENT_MASKED_WALL
  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a <= ALPHA_MIN) discard;
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif
#ifdef VV_AMBIENT_BRIGHTMAP_WALL
  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a <= ALPHA_MIN) discard;
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
  $include "common/brightmap_calc.fs"
#endif
  gl_FragColor = lt;
}
