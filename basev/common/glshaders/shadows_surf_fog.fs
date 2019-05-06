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
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < 0.01) discard;
  if (TexColor.a < 0.666) discard; //FIXME: only normal and masked walls should go thru this
#endif

  vec4 FinalColor;
  $include "common/fog_calc_main.fs"

  gl_FragColor = FinalColor;
}
