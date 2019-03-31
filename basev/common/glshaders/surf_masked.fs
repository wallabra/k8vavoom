#version 120
$include "common/common.inc"

//#define VAVOOM_ADV_MASKED_FOG

uniform sampler2D Texture;
uniform vec4 Light;
uniform float AlphaRef;

$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < AlphaRef) discard;
  //TexColour *= Light;
  TexColour.rgb *= Light.rgb;

  // convert to premultiplied
  vec4 FinalColour_1;
  FinalColour_1.rgb = (TexColour.rgb*TexColour.a)*Light.a;
  FinalColour_1.a = TexColour.a*Light.a;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColour_1;
}
