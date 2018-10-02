  // FinalColour_1 should contain "current final" color
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
    FogFactor_3 = clamp(FogFactor_3, 0.0, 1.0); // "smooth factor"

    float FogFactor = clamp((FogFactor_3-0.1)/0.9, 0.0, 1.0);
    FinalColour_1 = mix(FogColour, FinalColour_1, FogFactor*FogFactor*(3.0-(2.0*FogFactor)));
    if (FinalColour_1.a < 0.01) discard; //???
  }
