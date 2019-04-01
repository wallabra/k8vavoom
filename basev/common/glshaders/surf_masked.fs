#version 120
$include "common/common.inc"

//#define VAVOOM_ADV_MASKED_FOG

uniform sampler2D Texture;
#ifdef VV_MASKED_BRIGHTMAP
uniform sampler2D TextureBM;
#endif
uniform vec4 Light;
uniform float AlphaRef;

$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < AlphaRef) discard;
  //TexColour *= Light;

  vec4 lt = Light;
#ifdef VV_MASKED_BRIGHTMAP
  vec4 BMColor = texture2D(TextureBM, TextureCoordinate);
  lt.rgb = max(lt.rgb, BMColor.rgb);
  //lt.rgb = BMColor.rgb;
#endif
  TexColour.rgb *= lt.rgb;

  // convert to premultiplied
  vec4 FinalColour_1;
  FinalColour_1.rgb = (TexColour.rgb*TexColour.a)*lt.a;
  FinalColour_1.a = TexColour.a*lt.a;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColour_1;
}
