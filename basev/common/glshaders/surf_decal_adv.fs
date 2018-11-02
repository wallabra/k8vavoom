#version 120

uniform sampler2D Texture;
uniform sampler2D AmbLightTexture;
uniform vec4 SplatColour; // do recolor if .a is not zero
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

  if (SplatColour.a != 0.0) {
    FinalColour_1.r = SplatColour.r*TexColour.r*SplatAlpha; // convert to premultiplied
    FinalColour_1.g = SplatColour.g*TexColour.r*SplatAlpha; // convert to premultiplied
    FinalColour_1.b = SplatColour.b*TexColour.r*SplatAlpha; // convert to premultiplied
    FinalColour_1.a = clamp(TexColour.r*SplatAlpha, 0.0, 1.0);
  } else {
    FinalColour_1.r = TexColour.r*SplatAlpha; // convert to premultiplied
    FinalColour_1.g = TexColour.g*SplatAlpha; // convert to premultiplied
    FinalColour_1.b = TexColour.b*SplatAlpha; // convert to premultiplied
    FinalColour_1.a = clamp(TexColour.a*SplatAlpha, 0.0, 1.0);
  }
  if (FinalColour_1.a < 0.01) discard;

#if 0
  // normal
  FinalColour_1.r = clamp((FinalColour_1.r*Light.r)*Light.a, 0.0, 1.0);
  FinalColour_1.g = clamp((FinalColour_1.g*Light.g)*Light.a, 0.0, 1.0);
  FinalColour_1.b = clamp((FinalColour_1.b*Light.b)*Light.a, 0.0, 1.0);
#else
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
#endif


/*
  const float multer = 1.0;
  float sa = SplatColour.a;
  FinalColour_1.r = clamp((FinalColour_1.r*(1.0-sa)+SplatColour.r*sa)*multer, 0.0, 1.0);
  FinalColour_1.g = clamp((FinalColour_1.g*(1.0-sa)+SplatColour.g*sa)*multer, 0.0, 1.0);
  FinalColour_1.b = clamp((FinalColour_1.b*(1.0-sa)+SplatColour.b*sa)*multer, 0.0, 1.0);
  FinalColour_1.a = fina;

#if 1
  FinalColour_1.r = clamp(FinalColour_1.r*Light.a, 0.0, 1.0);
  FinalColour_1.g = clamp(FinalColour_1.g*Light.a, 0.0, 1.0);
  FinalColour_1.b = clamp(FinalColour_1.b*Light.a, 0.0, 1.0);
#else
  FinalColour_1.r = clamp(FinalColour_1.r, 0.0, 1.0);
  FinalColour_1.g = clamp(FinalColour_1.g, 0.0, 1.0);
  FinalColour_1.b = clamp(FinalColour_1.b, 0.0, 1.0);
#endif
*/

  //FinalColour_1.r = 1;

  gl_FragColor = FinalColour_1;
}
