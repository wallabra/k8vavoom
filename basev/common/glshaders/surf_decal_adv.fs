#version 120

uniform sampler2D Texture;
uniform vec4 SplatColour; // do recolor if .a is not zero
uniform float SplatAlpha; // image alpha will be multiplied by this
uniform vec4 Light;

varying vec2 TextureCoordinate;


void main () {
  vec4 FinalColour_1;
  vec4 TexColour;

  if (SplatAlpha <= 0.0) discard;

  TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.1) discard;

  float fina;

  if (TexColour.r == TexColour.g && TexColour.r == TexColour.b && TexColour.a == 1.0) {
    float lumi = min(0.2126*TexColour.r+0.7152*TexColour.g+0.0722*TexColour.b, 1.0);
    if (lumi < 0.1) discard;
    fina = clamp(lumi*SplatAlpha, 0.0, 1.0);
  } else {
    fina = clamp(TexColour.a*SplatAlpha, 0.0, 1.0);
  }

  //float ClampTransp = clamp((fina-0.1)/0.9, 0.0, 1.0);
  //fina = fina*(ClampTransp*(ClampTransp*(3.0-(2.0*ClampTransp))));
  //fina = clamp(fina+0.1, 0.0, 1.0);

  if (fina <= 0.0) discard;

/*
#if 0
  FinalColour_1.r = clamp(TexColour.r*Light.a, 0.0, 1.0);
  FinalColour_1.g = clamp(TexColour.g*Light.a, 0.0, 1.0);
  FinalColour_1.b = clamp(TexColour.b*Light.a, 0.0, 1.0);
#else
  FinalColour_1.r = clamp(TexColour.r, 0.0, 1.0);
  FinalColour_1.g = clamp(TexColour.g, 0.0, 1.0);
  FinalColour_1.b = clamp(TexColour.b, 0.0, 1.0);
#endif
*/

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

  //FinalColour_1.r = 1;

  gl_FragColor = FinalColour_1;
}
