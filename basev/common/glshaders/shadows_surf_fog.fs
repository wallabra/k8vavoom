#version 120

uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;


void main () {
#ifdef VAVOOM_REVERSE_Z
  float z = 1.0/gl_FragCoord.w;
#else
  float z = gl_FragCoord.z/gl_FragCoord.w;
#endif

  float FogFactor;
  if (FogType == 3) {
    FogFactor = exp2((((-FogDensity*FogDensity)*z)*z)*1.442695);
  } else if (FogType == 2) {
    FogFactor = exp2((-FogDensity*z)*1.442695);
  } else {
    FogFactor = (FogEnd-z)/(FogEnd-FogStart);
  }

  float ClampFogFactor = clamp(1.0-FogFactor, 0.0, 1.0);
  FogFactor = ClampFogFactor;

  float SmoothFactor = clamp((ClampFogFactor-0.1)/0.9, 0.0, 1.0);

  vec4 FinalFogColour;

  FinalFogColour.rgb = FogColour.rgb;
  FinalFogColour.a = SmoothFactor*(SmoothFactor*(3.0-(2.0*SmoothFactor)));
  if (FinalFogColour.a < 0.01) discard;

  gl_FragColor = FinalFogColour;
}
