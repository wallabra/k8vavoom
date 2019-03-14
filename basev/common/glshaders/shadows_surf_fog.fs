#version 120

#define VAVOOM_SIMPLE_ALPHA_FOG

uniform bool FogEnabled; // unused, but required
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;


void main () {
#if 0
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

  vec4 FinalColour_1;

  FinalColour_1.rgb = FogColour.rgb;
  FinalColour_1.a = SmoothFactor*(SmoothFactor*(3.0-(2.0*SmoothFactor)));
  if (FinalColour_1.a < 0.01) discard;

#else
  vec4 FinalColour_1;
  $include "common/fog_calc_main.fs"
#endif

  gl_FragColor = FinalColour_1;
}
