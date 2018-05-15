#version 120

// fragment shader for simple (non-lightmapped) surfaces

uniform sampler2D Texture;
uniform vec4 Light;
uniform vec4 SplatColour; // do recolor if .a is not zero
uniform float SplatAlpha; // image alpha will be multiplied by this
uniform bool FogEnabled;
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;
uniform bool IsLightmap;

varying vec2 TextureCoordinate;


void main () {
  vec4 FinalColour_1;
  vec4 TexColour;

  if (SplatAlpha <= 0.0) discard;

  TexColour = (texture2D(Texture, TextureCoordinate)*Light);
  if (TexColour.a < 0.1) discard;

  //TexColour = (texture2D(Texture, gl_TexCoord[0].xy)*Light);
  //FinalColour_1 = TexColour;

  float lumi = 0.2126*TexColour.r+0.7152*TexColour.g+0.0722*TexColour.b;
  if (lumi < 0.1) discard;

  FinalColour_1.r = clamp(TexColour.r*(1.0-SplatColour.a)+SplatColour.r*SplatColour.a, 0.0, 1.0);
  FinalColour_1.g = clamp(TexColour.g*(1.0-SplatColour.a)+SplatColour.g*SplatColour.a, 0.0, 1.0);
  FinalColour_1.b = clamp(TexColour.b*(1.0-SplatColour.a)+SplatColour.b*SplatColour.a, 0.0, 1.0);
  FinalColour_1.a = min(lumi*SplatAlpha, 1.0);

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

    float ClampFactor = clamp(FogFactor_3, 0.0, 1.0);
    FogFactor_3 = ClampFactor;

    float FogFactor = clamp((ClampFactor-0.1)/0.9, 0.0, 1.0);
    FinalColour_1 = mix(FogColour, FinalColour_1, FogFactor*FogFactor*(3.0-(2.0*FogFactor)));
  }

  gl_FragColor = FinalColour_1;
}
