#version 120

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
  if (FullBright == 0) {
    FinalColour_1.r = clamp(FinalColour_1.r*ambColor.r, 0.0, 1.0);
    FinalColour_1.g = clamp(FinalColour_1.g*ambColor.g, 0.0, 1.0);
    FinalColour_1.b = clamp(FinalColour_1.b*ambColor.b, 0.0, 1.0);
  }

  // convert to premultiplied
  FinalColour_1.r = FinalColour_1.r*FinalColour_1.a;
  FinalColour_1.g = FinalColour_1.g*FinalColour_1.a;
  FinalColour_1.b = FinalColour_1.b*FinalColour_1.a;

  //FinalColour_1 = ambColor;
  gl_FragColor = FinalColour_1;
}
