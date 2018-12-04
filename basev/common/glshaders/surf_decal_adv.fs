#version 120

uniform sampler2D Texture;
uniform sampler2D AmbLightTexture;
uniform float SplatAlpha; // image alpha will be multiplied by this
uniform vec4 Light;
uniform vec2 ScreenSize;

varying vec2 TextureCoordinate;


void main () {
  vec4 FinalColour_1;
  vec4 TexColour;

  if (SplatAlpha <= 0.01) discard;

  TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

  // convert to premultiplied
  FinalColour_1.r = TexColour.r*SplatAlpha;
  FinalColour_1.g = TexColour.g*SplatAlpha;
  FinalColour_1.b = TexColour.b*SplatAlpha;
  FinalColour_1.a = clamp(TexColour.a*SplatAlpha, 0.0, 1.0);
  if (FinalColour_1.a < 0.01) discard;

  // sample color from ambient light texture
  vec2 tc2 = gl_FragCoord.xy/ScreenSize.xy;
  vec4 ambColor = texture2D(AmbLightTexture, tc2);
  // Light.a == 1: fullbright
  if (Light.a == 0) {
    FinalColour_1.r = clamp(FinalColour_1.r*ambColor.r, 0.0, 1.0);
    FinalColour_1.g = clamp(FinalColour_1.g*ambColor.g, 0.0, 1.0);
    FinalColour_1.b = clamp(FinalColour_1.b*ambColor.b, 0.0, 1.0);
    //FinalColour_1 = ambColor;
    //FinalColour_1.a = 1;
  }

  gl_FragColor = FinalColour_1;
}
