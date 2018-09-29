#version 120

uniform sampler2D Texture;
uniform vec4 SplatColour; // do recolor if .a is not zero
uniform float SplatAlpha; // image alpha will be multiplied by this
uniform vec4 Light;

varying vec2 TextureCoordinate;


void main () {
  vec4 FinalColour_1;
  vec4 TexColour;

  if (SplatAlpha <= 0.05) discard;

  TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.05) discard;

  if (SplatColour.a != 0.0) {
    FinalColour_1.r = SplatColour.r*TexColour.r;
    FinalColour_1.g = SplatColour.g*TexColour.r;
    FinalColour_1.b = SplatColour.b*TexColour.r;
    FinalColour_1.a = clamp(TexColour.r*SplatAlpha, 0.0, 1.0);
  } else {
    FinalColour_1.r = TexColour.r;
    FinalColour_1.g = TexColour.g;
    FinalColour_1.b = TexColour.b;
    FinalColour_1.a = clamp(TexColour.a*SplatAlpha, 0.0, 1.0);
  }
  if (FinalColour_1.a < 0.05) discard;

  if (SplatAlpha <= 0.0) discard;

  // normal
  FinalColour_1.r = clamp(FinalColour_1.r*(Light.r*Light.a), 0.0, 1.0);
  FinalColour_1.g = clamp(FinalColour_1.g*(Light.g*Light.a), 0.0, 1.0);
  FinalColour_1.b = clamp(FinalColour_1.b*(Light.b*Light.a), 0.0, 1.0);

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
