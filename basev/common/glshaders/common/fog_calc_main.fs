  // `FinalColor` should contain "current final" color
#ifndef VAVOOM_SIMPLE_ALPHA_FOG
    //!if (FinalColor.a < ALPHA_MIN) discard; //k8: dunno if it worth it, but meh...
#endif

#ifdef VAVOOM_REVERSE_Z
    float z = 1.0/gl_FragCoord.w;
#else
    float z = gl_FragCoord.z/gl_FragCoord.w;
#endif

/*
    if (FogType == 3) {
      FogFactor_3 = exp2(-FogDensity*FogDensity*z*z*1.442695);
    } else if (FogType == 2) {
      FogFactor_3 = exp2(-FogDensity*z*1.442695);
    } else {
      FogFactor_3 = (FogEnd-z)/(FogEnd-FogStart);
    }
*/
    float FogFactor = (FogEnd-z)/(FogEnd-FogStart);
#ifdef VAVOOM_SIMPLE_ALPHA_FOG
    FogFactor = 1.0-FogFactor;
#endif
    FogFactor = clamp(FogFactor, 0.0, 1.0); // "smooth factor"

    FogFactor = clamp((FogFactor-0.1)/0.9, 0.0, 1.0);
    float FogCoeff = FogFactor*FogFactor*(3.0-(2.0*FogFactor));

#ifdef VAVOOM_SIMPLE_ALPHA_FOG
    // used in advrender
    FinalColor.rgb = FogColor.rgb;
    FinalColor.a = FogCoeff;
#else
    // don't mess with alpha channel
    float oldAlpha = FinalColor.a;
    //FinalColor = mix(FogColor, FinalColor, FogCoeff);
    FinalColor = mix(FogColor*oldAlpha, FinalColor, FogCoeff);
    FinalColor.a = oldAlpha;
    //if (FinalColor.a < ALPHA_MIN) discard; //k8: dunno if it worth it, but meh...
#endif
