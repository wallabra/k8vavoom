#version 120

// fragment shader for lightmapped surfaces

uniform sampler2D Texture;
uniform sampler2D LightMap;
uniform sampler2D SpecularMap;
uniform bool FogEnabled;
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;

varying vec2 TextureCoordinate;
varying vec2 LightmapCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate)*texture2D(LightMap, LightmapCoordinate)+texture2D(SpecularMap, LightmapCoordinate);
  vec4 FinalColour_1 = TexColour;

  if (TexColour.w < 0.1) discard;

  if (FogEnabled) {
    float FogFactor_3;

#ifdef VAVOOM_REVERSE_Z
    float z = 1.0/gl_FragCoord.w;
#else
    float z = gl_FragCoord.z/gl_FragCoord.w;
#endif

    if (FogType == 3) {
      FogFactor_3 = exp2(-FogDensity*FogDensity*z*z*1.442695);
    } else if (FogType == 2) {
      FogFactor_3 = exp2(-FogDensity*z*1.442695);
    } else {
      FogFactor_3 = (FogEnd-z)/(FogEnd-FogStart);
    }

    float SmoothFactor = clamp(FogFactor_3, 0.0, 1.0);
    FogFactor_3 = SmoothFactor;

    float FogFactor = clamp((SmoothFactor-0.1)/0.9, 0.0, 1.0);
    FinalColour_1 = mix(FogColour, TexColour, FogFactor*FogFactor*(3.0-(2.0*FogFactor)));
  }

  gl_FragColor = FinalColour_1;
}
