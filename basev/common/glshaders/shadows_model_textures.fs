#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform sampler2D AmbLightTexture;
uniform float InAlpha;
uniform bool AllowTransparency;
uniform vec2 ScreenSize;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard;
  if (!AllowTransparency) {
    if (TexColor.a < ALPHA_MASKED) discard;
  } else {
    if (TexColor.a < ALPHA_MIN) discard;
  }

  float alpha = clamp(TexColor.a*InAlpha, 0.0, 1.0);
  if (alpha < ALPHA_MIN) discard;


  vec4 FinalColor;
  FinalColor.rgb = TexColor.rgb*alpha;
  FinalColor.a = alpha;

  // sample color from ambient light texture
  vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
  vec4 ambColor = texture2D(AmbLightTexture, tc2);

  // Light.a == 1: fullbright
  // k8: oops, no way to do it yet (why?)
  FinalColor.rgb = clamp(FinalColor.rgb*ambColor.rgb, 0.0, 1.0);

  gl_FragColor = FinalColor;
}
