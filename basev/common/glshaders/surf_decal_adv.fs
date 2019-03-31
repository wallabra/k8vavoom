#version 120
$include "common/common.inc"

uniform sampler2D Texture;
uniform sampler2D AmbLightTexture;
uniform float SplatAlpha; // image alpha will be multiplied by this
uniform float FullBright;
uniform vec2 ScreenSize;

varying vec2 TextureCoordinate;


void main () {
  vec4 FinalColour_1;
  vec4 TexColour;

  if (SplatAlpha <= 0.01) discard;

  TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

  FinalColour_1.a = clamp(TexColour.a*SplatAlpha, 0.0, 1.0);
  if (FinalColour_1.a < 0.01) discard;
  FinalColour_1.rgb = TexColour.rgb;

  // sample color from ambient light texture
  vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
  vec4 ambColor = texture2D(AmbLightTexture, tc2);
  // if `FullBright` is 1, mul by 1.0, otherwise mul by ambient
  ambColor.r = 1.0*FullBright+(1.0-FullBright)*ambColor.r;
  ambColor.g = 1.0*FullBright+(1.0-FullBright)*ambColor.g;
  ambColor.b = 1.0*FullBright+(1.0-FullBright)*ambColor.b;
  FinalColour_1.rgb = clamp(FinalColour_1.rgb*ambColor.rgb, 0.0, 1.0);

  // convert to premultiplied
  FinalColour_1.rgb *= FinalColour_1.a;

  //FinalColour_1 = ambColor;
  gl_FragColor = FinalColour_1;
}
