#version 120
$include "common/common.inc"

uniform sampler2D Texture;
$include "common/texshade.inc"
uniform float Alpha;
#ifdef LIGHTING
uniform vec4 Light;
#endif

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColor = GetStdTexel(Texture, TextureCoordinate);
  if (TexColor.a < ALPHA_MIN) discard;

  // we got a non-premultiplied color, convert it
  vec4 FinalColor;
  FinalColor.a = clamp(TexColor.a*Alpha, 0.0, 1.0);
  if (FinalColor.a < ALPHA_MIN) discard;

#ifdef LIGHTING
  FinalColor.a = clamp(FinalColor.a*Light.a, 0.0, 1.0);
  FinalColor.rgb = TexColor.rgb*Light.rgb;
  FinalColor.rgb = clamp(FinalColor.rgb*FinalColor.a, 0.0, 1.0);
#else
  FinalColor.rgb = TexColor.rgb*FinalColor.a;
#endif

  gl_FragColor = FinalColor;
}
