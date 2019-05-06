#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform float Alpha;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  if (TexColor.a < 0.01) discard;

  // we got a non-premultiplied color, convert it
  vec4 FinalColor;
  FinalColor.a = TexColor.a*clamp(Alpha, 0.0, 1.0);
  if (FinalColor.a < 0.01) discard;
  FinalColor.rgb = TexColor.rgb*FinalColor.a;
  gl_FragColor = FinalColor;
}
