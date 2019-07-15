  // `FinalColor` should contain "current final" color
#ifndef VAVOOM_SIMPLE_ALPHA_FOG
    //!if (FinalColor.a < ALPHA_MIN) discard; //k8: dunno if it worth it, but meh...
#endif

    float FogFactor_3;
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
    FogFactor_3 = (FogEnd-z)/(FogEnd-FogStart);
#ifdef VAVOOM_SIMPLE_ALPHA_FOG
    FogFactor_3 = 1.0-FogFactor_3;
#else
# ifdef VAVOOM_ADV_MASKED_FOG
    //FogFactor_3 = 1.0-FogFactor_3;
# endif
#endif
    FogFactor_3 = clamp(FogFactor_3, 0.0, 1.0); // "smooth factor"

    float FogFactor = clamp((FogFactor_3-0.1)/0.9, 0.0, 1.0);
    float FogCoeff_0 = FogFactor*FogFactor*(3.0-(2.0*FogFactor));

#ifdef VAVOOM_SIMPLE_ALPHA_FOG
    // used in advrender
    FinalColor.rgb = FogColor.rgb;
    FinalColor.a = FogCoeff_0;
#else
    // don't mess with alpha channel
    float oldAlpha = FinalColor.a;
# ifdef VAVOOM_ADV_MASKED_FOG
    vec4 fc2 = vec4(1.0-FogColor.r, 1.0-FogColor.g, 1.0-FogColor.b, FogColor.a);
    FinalColor = mix(fc2, FinalColor, FogCoeff_0);
# else
    FinalColor = mix(FogColor, FinalColor, FogCoeff_0);
# endif
    FinalColor.a = oldAlpha;
    //if (FinalColor.a < ALPHA_MIN) discard; //k8: dunno if it worth it, but meh...
#endif
