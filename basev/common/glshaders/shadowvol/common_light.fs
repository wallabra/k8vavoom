#ifdef VV_SURFACE_LIGHTING
# define Normal  SurfNormal
#endif


  float DistToLight = max(1.0, dot(VertToLight, VertToLight));
  if (DistToLight >= LightRadius*LightRadius) discard;

  vec3 normV2L = normalize(VertToLight);

#ifdef VV_SHADOWMAPS
  $include "shadowvol/smap_light_check.fs"
#endif

  // "half-lambert" lighting model
  DistToLight = sqrt(DistToLight);
  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+0.5*dot(normV2L, Normal));
#ifdef VV_SPOTLIGHT
  $include "common/spotlight_calc.fs"
#endif

#ifdef VV_SHADOWMAPS
  attenuation *= shadowMul;
  //attenuation = 1.0; // debug
#endif
  if (attenuation <= 0.0) discard;
  float finalA = min(attenuation/255.0, 1.0);

#ifdef VV_SHADOW_CHECK_TEXTURE
  float transp = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);
/*
  #ifdef VV_LIGHT_MODEL
  if (transp < ALPHA_MIN) discard;
  #endif
*/
  finalA *= transp*transp*(3.0-(2.0*transp));
#endif

#ifdef VV_SHADOWMAPS
  //shadowMul = clamp(shadowMul*1.0, 0.0, 1.0); //debug
  //gl_FragColor = vec4(shadowMul, 0.0, 0.0, 1.0);
  //gl_FragColor = vec4(clamp(fromLightToFragment.x, 0.0, 1.0), 0.0, 0.0, 1.0);
  gl_FragColor = vec4(LightColor, finalA);
#else
  gl_FragColor = vec4(LightColor, finalA);
#endif
