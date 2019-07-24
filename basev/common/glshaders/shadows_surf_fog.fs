#version 120
$include "common/common.inc"

#define VAVOOM_SIMPLE_ALPHA_FOG
$include "common/fog_vars.fs"

#ifdef VV_MASKED_FOG
uniform sampler2D Texture;
$include "common/texture_vars.fs"
#endif


void main () {
#ifdef VV_MASKED_FOG
  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard;
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif

  vec4 FinalColor;
  $include "common/fog_calc_main.fs"

  gl_FragColor = FinalColor;
}
