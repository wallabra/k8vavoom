#define VV_SMAP_ALLOW_BLUR3
#define VV_SMAP_MORE_CHECKS

  // dunno which one is better (or even which one is right, lol)
  #if 1
  // 0.026
  // 0.039
  float cosTheta = clamp(dot(Normal, normV2L), 0.0, 1.0);
  //float bias = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
  float bias = clamp(BiasMul*tan(acos(cosTheta)), BiasMin, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
  #else
  float biasMod = 1.0-clamp(dot(Normal, normV2L), 0.0, 1.0);
  //float bias = 0.001+0.039*biasMod;
  float bias = clamp(BiasMin+BiasMul*biasMod, 0.0, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
  #endif

  // difference between position of the light source and position of the fragment
  vec3 fromLightToFragment = LightPos2-VertWorldPos;
  // normalized distance to the point light source
  float distanceToLight = length(fromLightToFragment);
  float currentDistanceToLight = distanceToLight/LightRadius;
  // normalized direction from light source for sampling
  // (k8: there is no need to do that: this is just a direction, and hardware doesn't require it to be normalized)
  fromLightToFragment = normalize(fromLightToFragment);
  // sample shadow cube map
  vec3 ltfdir;
  ltfdir.x = -fromLightToFragment.x;
  ltfdir.y =  fromLightToFragment.y;
  ltfdir.z =  fromLightToFragment.z;
  #if 0
  if (currentDistanceToLight > textureCube(ShadowTexture, ltfdir).r+bias) discard;
  float shadowMul = 1.0;
  #else
  float shadowMul;
  if (CubeBlur > 0.0) {
    float tstep = 1.0/CubeSize;
    float daccum = 0.0;
    float dcount = 5.0;
    if (textureCube(ShadowTexture, ltfdir).r+bias >= currentDistanceToLight) daccum += 1.0;
    float maxaxis = max(ltfdir.x, max(ltfdir.y, ltfdir.z));
    if (ltfdir.x == maxaxis) {
      // do not touch x
      // y
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -tstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  tstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      // z
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0, -tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0,  tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (CubeBlur > 1.0
          #ifdef VV_SMAP_MORE_CHECKS
          && daccum < 5.0
          #endif
         )
      {
        dcount = 9.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -tstep, -tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -tstep,  tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  tstep, -tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  tstep,  tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_ALLOW_BLUR3
        if (CubeBlur > 2.0
            #ifdef VV_SMAP_MORE_CHECKS
            && daccum < 9.0
            #endif
           )
        {
          dcount = 17.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -tstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  tstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0, -tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0,  tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -tstep*2.0, -tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -tstep*2.0,  tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  tstep*2.0, -tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  tstep*2.0,  tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        }
        #endif
      }
    } else if (ltfdir.y == maxaxis) {
      // do not touch y
      // x
      if (textureCube(ShadowTexture, ltfdir+vec3(-tstep, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3( tstep, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      // z
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0, -tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0,  tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (CubeBlur > 1.0
          #ifdef VV_SMAP_MORE_CHECKS
          && daccum < 5.0
          #endif
         )
      {
        dcount = 9.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(-tstep, 0.0, -tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(-tstep, 0.0,  tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3( tstep, 0.0, -tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3( tstep, 0.0,  tstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_ALLOW_BLUR3
        if (CubeBlur > 2.0
            #ifdef VV_SMAP_MORE_CHECKS
            && daccum < 9.0
            #endif
           )
        {
          dcount = 17.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-tstep*2.0, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( tstep*2.0, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0, -tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0,  tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-tstep*2.0, 0.0, -tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-tstep*2.0, 0.0,  tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( tstep*2.0, 0.0, -tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( tstep*2.0, 0.0,  tstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        }
        #endif
      }
    } else {
      // do not touch z
      // x
      if (textureCube(ShadowTexture, ltfdir+vec3(-tstep, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3( tstep, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      // y
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -tstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  tstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (CubeBlur > 1.0
          #ifdef VV_SMAP_MORE_CHECKS
          && daccum < 5.0
          #endif
         )
      {
        dcount = 9.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(-tstep, -tstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(-tstep,  tstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3( tstep, -tstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3( tstep,  tstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_ALLOW_BLUR3
        if (CubeBlur > 2.0
            #ifdef VV_SMAP_MORE_CHECKS
            && daccum < 9.0
            #endif
           )
        {
          dcount = 17.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-tstep*2.0, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( tstep*2.0, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -tstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  tstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-tstep*2.0, -tstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-tstep*2.0,  tstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( tstep*2.0, -tstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( tstep*2.0,  tstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        }
        #endif
      }
    }
    if (daccum <= 0.0) discard;
    shadowMul = daccum/dcount;
  } else {
    if (textureCube(ShadowTexture, ltfdir).r+bias < currentDistanceToLight) discard;
    shadowMul = 1.0;
  }
  #endif
  /*
  float currentDistanceToLight = (distanceToLight-u_nearFarPlane.x)/(u_nearFarPlane.y-u_nearFarPlane.x);
  currentDistanceToLight = clamp(currentDistanceToLight, 0, 1);
  */
