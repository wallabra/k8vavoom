#version 120

uniform sampler2D Texture;
uniform sampler2D LightMap;
uniform sampler2D SpecularMap;
uniform vec4 Light;
uniform vec4 SplatColour; // do recolor if .a is not zero
uniform float SplatAlpha; // image alpha will be multiplied by this
uniform bool FogEnabled;
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;

varying vec2 TextureCoordinate;
varying vec2 LightmapCoordinate;


void main () {
  vec4 FinalColour_1;
  vec4 TexColour;

  if (SplatAlpha <= 0.0) discard;

  TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.1) discard;

  float lumi = min(0.2126*TexColour.r+0.7152*TexColour.g+0.0722*TexColour.b, 1.0);
  if (lumi < 0.1) discard;

  FinalColour_1.r = clamp(TexColour.r*(1.0-SplatColour.a)+SplatColour.r*SplatColour.a, 0.0, 1.0);
  FinalColour_1.g = clamp(TexColour.g*(1.0-SplatColour.a)+SplatColour.g*SplatColour.a, 0.0, 1.0);
  FinalColour_1.b = clamp(TexColour.b*(1.0-SplatColour.a)+SplatColour.b*SplatColour.a, 0.0, 1.0);

  /*
  if (TexColour.a < 0.8) {
    FinalColour_1.a = clamp(TexColour.a*SplatAlpha, 0.0, 1.0);
  } else {
    FinalColour_1.a = clamp(lumi*SplatAlpha, 0.0, 1.0);
  }
  FinalColour_1.a = clamp(SplatAlpha, 0.0, 1.0);
  */

  if (TexColour.r == TexColour.g && TexColour.r == TexColour.b && TexColour.a == 1.0) {
    FinalColour_1.a = clamp(lumi*SplatAlpha, 0.0, 1.0);
  } else {
    FinalColour_1.a = clamp(TexColour.a*SplatAlpha, 0.0, 1.0);
  }

  if (FinalColour_1.a <= 0.0) discard;

  vec4 lmc = texture2D(LightMap, LightmapCoordinate);
  vec4 spc = texture2D(SpecularMap, LightmapCoordinate);
  FinalColour_1.r = clamp(FinalColour_1.r*lmc.r+spc.r, 0.0, 1.0);
  FinalColour_1.g = clamp(FinalColour_1.g*lmc.g+spc.g, 0.0, 1.0);
  FinalColour_1.b = clamp(FinalColour_1.b*lmc.b+spc.b, 0.0, 1.0);

  //FinalColour_1.r = clamp(FinalColour_1.r*Light.r, 0.0, 1.0);
  //FinalColour_1.g = clamp(FinalColour_1.g*Light.g, 0.0, 1.0);
  //FinalColour_1.b = clamp(FinalColour_1.b*Light.b, 0.0, 1.0);

  if (FogEnabled) {
    float FogFactor_3;

    float z = gl_FragCoord.z/gl_FragCoord.w;

    if (FogType == 3) {
      FogFactor_3 = exp2(-FogDensity*FogDensity*z*z*1.442695);
    } else if (FogType == 2) {
      FogFactor_3 = exp2(-FogDensity*z*1.442695);
    } else {
      FogFactor_3 = (FogEnd-z)/(FogEnd-FogStart);
    }

    FogFactor_3 = clamp(FogFactor_3, 0.0, 1.0);

    float FogFactor = clamp((FogFactor_3-0.1)/0.9, 0.0, 1.0);
    float aa = FinalColour_1.a;
    FinalColour_1 = mix(FogColour, FinalColour_1, FogFactor*FogFactor*(3.0-(2.0*FogFactor)));
    FinalColour_1.r = clamp(FinalColour_1.r, 0.0, 1.0);
    FinalColour_1.g = clamp(FinalColour_1.g, 0.0, 1.0);
    FinalColour_1.b = clamp(FinalColour_1.b, 0.0, 1.0);
    FinalColour_1.a = aa;
  }

  //FinalColour_1 = vec4(1, 0, 0, 1);

  gl_FragColor = FinalColour_1;
}
