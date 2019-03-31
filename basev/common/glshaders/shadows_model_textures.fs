#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform sampler2D AmbLightTexture;
uniform float InAlpha;
uniform bool AllowTransparency;
uniform vec2 ScreenSize;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  //if (TexColour.a < 0.01) discard;
  if (!AllowTransparency) {
    if (TexColour.a < 0.666) discard;
  } else {
    if (TexColour.a < 0.01) discard;
  }

  float alpha = clamp(TexColour.a*InAlpha, 0.0, 1.0);
  if (alpha < 0.01) discard;


  vec4 FinalColour_1;
  FinalColour_1.rgb = TexColour.rgb*alpha;
  FinalColour_1.a = alpha;

  // sample color from ambient light texture
  vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
  vec4 ambColor = texture2D(AmbLightTexture, tc2);

  // Light.a == 1: fullbright
  // k8: oops, no way to do it yet (why?)
  FinalColour_1.rgb = clamp(FinalColour_1.rgb*ambColor.rgb, 0.0, 1.0);

  gl_FragColor = FinalColour_1;
}
