#version 120
$include "common/common.inc"

uniform sampler2D Texture;
$include "common/fog_vars.fs"
uniform float InAlpha;
uniform bool AllowTransparency;

varying vec4 Light;
varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a < 0.01) discard;
  if (!AllowTransparency) {
    if (TexColour.a < 0.666) discard;
  } else {
    if (TexColour.a < 0.01) discard;
  }
  TexColour *= Light;

  vec4 FinalColour_1 = TexColour;

  // do fog before premultiply, otherwise it is wrong
  $include "common/fog_calc.fs"

  // convert to premultiplied
  FinalColour_1.rgb *= FinalColour_1.a;
  FinalColour_1.a *= InAlpha;
  if (FinalColour_1.a < 0.01) discard;
  FinalColour_1 = min(FinalColour_1, 1.0);

  gl_FragColor = FinalColour_1;
}
