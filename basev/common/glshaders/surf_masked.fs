#version 120
$include "common/common.inc"

//#define VAVOOM_ADV_MASKED_FOG

uniform sampler2D Texture;
#ifdef VV_MASKED_BRIGHTMAP
$include "common/brightmap_vars.fs"
#endif
uniform vec4 Light;
uniform float AlphaRef;

$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;

#ifdef VV_MASKED_GLOW
$include "common/glow_vars.fs"
#endif


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (TexColor.a < AlphaRef) discard;
  //TexColor *= Light;

#ifdef VV_MASKED_GLOW
  vec4 lt = calcGlow(Light);
#else
  vec4 lt = Light;
#endif
#ifdef VV_MASKED_BRIGHTMAP
  $include "common/brightmap_calc.fs"
#endif
  TexColor *= lt;

  // convert to premultiplied
  vec4 FinalColor;
#if 0
  FinalColor.rgb = (TexColor.rgb*TexColor.a)*lt.a;
  FinalColor.a = TexColor.a*lt.a;
#else
  FinalColor.rgb = TexColor.rgb;
  FinalColor.a = TexColor.a*lt.a;
#endif
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColor;
}
