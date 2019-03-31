#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform float Alpha;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

  // we got a non-premultiplied color, convert it
  vec4 FinalColour_1;
  FinalColour_1.a = TexColour.a*clamp(Alpha, 0.0, 1.0);
  if (FinalColour_1.a < 0.01) discard;
  FinalColour_1.rgb = TexColour.rgb*FinalColour_1.a;
  gl_FragColor = FinalColour_1;
}
