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
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < AlphaRef) discard;
  //TexColour *= Light;

#ifdef VV_MASKED_GLOW
  vec4 lt = calcGlow(Light);
#else
  vec4 lt = Light;
#endif
#ifdef VV_MASKED_BRIGHTMAP
  $include "common/brightmap_calc.fs"
#endif
  TexColour *= lt;

  // convert to premultiplied
  vec4 FinalColour_1;
  FinalColour_1.rgb = (TexColour.rgb*TexColour.a)*lt.a;
  FinalColour_1.a = TexColour.a*lt.a;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColour_1;
}
