$include "shadowvol/smap_common_defines.inc"

  // dunno which one is better (or even which one is right, lol)
  // fun thing: it seems that turning on cubemap texture filtering removes moire almost completely
  // so if the user will turn on filtering, we will turn off "adaptive" bias with `UseAdaptiveBias`
  #if 1
  float biasBase, bias1;
  // 0.026
  // 0.039
  if (UseAdaptiveBias > 0.0) {
    float cosTheta = clamp(dot(Normal, normV2L), 0.0, 1.0);
    //biasBase = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
    biasBase = clamp(BiasMul*tan(acos(cosTheta)), BiasMin, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
    //biasBase = 0.072; //clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
    //biasBase = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.072); // cosTheta is dot( n,l ), clamped between 0 and 1
    //biasBase = 0.0001;
    bias1 = biasBase;
  } else {
    //float mxdist = max(abs(VertToLight.x), max(abs(VertToLight.y), abs(VertToLight.z)));
   #ifdef VV_SMAP_BLUR4
    // remove moire
    #if 1
    float mxdist = dot(VertToLight, VertToLight);
         if (mxdist > 400*400) biasBase = 0.0019;
    else if (mxdist > 200*200) biasBase = 0.0012;
    else if (mxdist > 100*100) biasBase = 0.0006;
    else biasBase = 0.0002;
    #else
      float cosTheta = clamp(dot(Normal, normV2L), 0.0, 1.0);
      biasBase = clamp(BiasMul*tan(acos(cosTheta)), BiasMin, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
    #endif
    bias1 = 0.0001;
   #else
    biasBase = 0.0001;
    bias1 = biasBase;
   #endif
  }
  #else
  float biasMod = 1.0-clamp(dot(Normal, normV2L), 0.0, 1.0);
  //float biasBase = 0.001+0.039*biasMod;
  float biasBase = clamp(BiasMin+BiasMul*biasMod, 0.0, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
  #endif
  // alas, this does nothing at all
  #ifdef VV_SMAP_BLUR4
  //float bias4 = biasBase*1.2;
  float bias4 = biasBase+0.006;
  #endif
  #ifdef VV_SMAP_BLUR8
  //float bias8 = biasBase*1.4;
  float bias8 = biasBase+0.008;
  #endif
  #ifdef VV_SMAP_BLUR16
  //float bias16 = biasBase*1.8;
  float bias16 = biasBase+0.009;
  #endif

  // difference between position of the light source and position of the fragment
  vec3 fromLightToFragment = LightPos2-VertWorldPos;
  // normalized distance to the point light source
  float distanceToLight = length(fromLightToFragment);
  float currentDistanceToLight = distanceToLight/LightRadius;
  // normalized direction from light source for sampling
  // (k8: there is no need to do that: this is just a direction, and hardware doesn't require it to be normalized)
  // (k8: but we may need normalized dir anyway)
  fromLightToFragment = normalize(fromLightToFragment);
  // sample shadow cube map
  vec3 ltfdir;
  ltfdir.x = -fromLightToFragment.x;
  ltfdir.y =  fromLightToFragment.y;
  ltfdir.z =  fromLightToFragment.z;

  #ifdef VV_SMAP_BLUR4
    //float cubetstep = 1.0/CubeSize;
    float daccum = 0.0;
    #ifdef VV_DYNAMIC_DCOUNT
    float dcount = 5.0;
    #endif
    if (textureCubeFn(ShadowTexture, ltfdir).r+bias1 >= currentDistanceToLight) daccum += 1.0;

    //TODO: process cube edges
    vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords

    // perform 4-way blur
    if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2(-1.0,  0.0))).r+bias4 >= currentDistanceToLight) daccum += 1.0;
    if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 1.0,  0.0))).r+bias4 >= currentDistanceToLight) daccum += 1.0;
    if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 0.0, -1.0))).r+bias4 >= currentDistanceToLight) daccum += 1.0;
    if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 0.0,  1.0))).r+bias4 >= currentDistanceToLight) daccum += 1.0;

    #ifdef VV_SMAP_BLUR8
      // perform 8-way blur
      #ifdef VV_SMAP_BLUR_FAST8
      if (daccum > 0.0 && daccum < 5.0)
      #endif
      {
        #ifdef VV_DYNAMIC_DCOUNT
        dcount = 9.0;
        #endif
        if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2(-1.0, -1.0))).r+bias8 >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2(-1.0,  1.0))).r+bias8 >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 1.0, -1.0))).r+bias8 >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 1.0,  1.0))).r+bias8 >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_BLUR16
          // perform 16-way blur
          #ifdef VV_SMAP_BLUR_FAST16
          if (daccum > 0.0 && daccum < 9.0)
          #endif
          {
            #ifdef VV_DYNAMIC_DCOUNT
            dcount = 17.0;
            #endif
            if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2(-2.0,  0.0))).r+bias16 >= currentDistanceToLight) daccum += 1.0;
            if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 2.0,  0.0))).r+bias16 >= currentDistanceToLight) daccum += 1.0;

            if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 0.0, -2.0))).r+bias16 >= currentDistanceToLight) daccum += 1.0;
            if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 0.0,  2.0))).r+bias16 >= currentDistanceToLight) daccum += 1.0;

            if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2(-2.0, -2.0))).r+bias16 >= currentDistanceToLight) daccum += 1.0;
            if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2(-2.0,  2.0))).r+bias16 >= currentDistanceToLight) daccum += 1.0;

            if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 2.0, -2.0))).r+bias16 >= currentDistanceToLight) daccum += 1.0;
            if (textureCubeFn(ShadowTexture, shift_cube_uv(cubeTC, vec2( 2.0,  2.0))).r+bias16 >= currentDistanceToLight) daccum += 1.0;
          }
        #endif
      }
    #endif
    if (daccum <= 0.0) discard;
    #ifdef VV_DYNAMIC_DCOUNT
    float shadowMul = daccum/dcount;
    #else
    float shadowMul = daccum/DCOUNT;
    #endif
  #else
    // no blur
    //vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords
    //ltfdir = convert_cube_uv_to_xyz(cubeTC);
    if (textureCubeFn(ShadowTexture, ltfdir).r+bias1 < currentDistanceToLight) discard;
    float shadowMul = 1.0;
  #endif
  /*
  float currentDistanceToLight = (distanceToLight-u_nearFarPlane.x)/(u_nearFarPlane.y-u_nearFarPlane.x);
  currentDistanceToLight = clamp(currentDistanceToLight, 0, 1);
  */
