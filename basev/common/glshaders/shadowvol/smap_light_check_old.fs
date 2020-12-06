$include "shadowvol/smap_common_defines.inc"

  // dunno which one is better (or even which one is right, lol)
  // fun thing: it seems that turning on cubemap texture filtering removes moire almost completely
  // so if the user will turn on filtering, we will turn off "adaptive" bias with `UseAdaptiveBias`
  #if 1
  float bias;
  // 0.026
  // 0.039
  if (UseAdaptiveBias > 0.0) {
    float cosTheta = clamp(dot(Normal, normV2L), 0.0, 1.0);
    //bias = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
    bias = clamp(BiasMul*tan(acos(cosTheta)), BiasMin, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
    //bias = 0.072; //clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
    //bias = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.072); // cosTheta is dot( n,l ), clamped between 0 and 1
    //bias = 0.0001;
  } else {
    //float mxdist = max(abs(VertToLight.x), max(abs(VertToLight.y), abs(VertToLight.z)));
   #ifdef VV_SMAP_BLUR4
    // remove moire
    float mxdist = dot(VertToLight, VertToLight);
         if (mxdist > 400*400) bias = 0.0019;
    else if (mxdist > 200*200) bias = 0.0012;
    else if (mxdist > 100*100) bias = 0.0006;
    else bias = 0.0002;
   #else
    bias = 0.0001;
   #endif
  }
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

  #ifdef VV_SMAP_BLUR4
    float cubetstep = 1.0/CubeSize;
    float daccum = 0.0;
    #ifdef VV_DYNAMIC_DCOUNT
    float dcount = 5.0;
    #endif
    if (textureCubeFn(ShadowTexture, ltfdir).r+bias >= currentDistanceToLight) daccum += 1.0;
    float maxaxis = max(abs(ltfdir.x), max(abs(ltfdir.y), abs(ltfdir.z)));
    if (ltfdir.x == maxaxis) {
      // do not touch x
      // y
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, -cubetstep, 0.0)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0,  cubetstep, 0.0)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      // z
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, 0.0, -cubetstep)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, 0.0,  cubetstep)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      #ifdef VV_SMAP_BLUR8
        #ifdef VV_SMAP_BLUR_FAST8
        if (daccum > 0.0 && daccum < 5.0)
        #endif
      {
        #ifdef VV_DYNAMIC_DCOUNT
        dcount = 9.0;
        #endif
        if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, -cubetstep, -cubetstep)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, -cubetstep,  cubetstep)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0,  cubetstep, -cubetstep)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0,  cubetstep,  cubetstep)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_BLUR16
          #ifdef VV_SMAP_BLUR_FAST16
          if (daccum > 0.0 && daccum < 9.0)
          #endif
        {
          #ifdef VV_DYNAMIC_DCOUNT
          dcount = 17.0;
          #endif
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, -cubetstep*2.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0,  cubetstep*2.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, 0.0, -cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, 0.0,  cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, -cubetstep*2.0, -cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, -cubetstep*2.0,  cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0,  cubetstep*2.0, -cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0,  cubetstep*2.0,  cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        }
        #endif
      }
      #endif
    } else if (ltfdir.y == maxaxis) {
      // do not touch y
      // x
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep, 0.0, 0.0)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep, 0.0, 0.0)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      // z
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, 0.0, -cubetstep)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, 0.0,  cubetstep)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      #ifdef VV_SMAP_BLUR8
        #ifdef VV_SMAP_BLUR_FAST8
        if (daccum > 0.0 && daccum < 5.0)
        #endif
      {
        #ifdef VV_DYNAMIC_DCOUNT
        dcount = 9.0;
        #endif
        if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep, 0.0, -cubetstep)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep, 0.0,  cubetstep)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep, 0.0, -cubetstep)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep, 0.0,  cubetstep)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_BLUR16
          #ifdef VV_SMAP_BLUR_FAST16
          if (daccum > 0.0 && daccum < 9.0)
          #endif
        {
          #ifdef VV_DYNAMIC_DCOUNT
          dcount = 17.0;
          #endif
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, 0.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep*2.0, 0.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, 0.0, -cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, 0.0,  cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, 0.0, -cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, 0.0,  cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep*2.0, 0.0, -cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep*2.0, 0.0,  cubetstep*2.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        }
        #endif
      }
      #endif
    } else {
      // do not touch z
      // x
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep, 0.0, 0.0)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep, 0.0, 0.0)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      // y
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, -cubetstep, 0.0)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0,  cubetstep, 0.0)).r+VV_BLUR4_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
      #ifdef VV_SMAP_BLUR8
        #ifdef VV_SMAP_BLUR_FAST8
        if (daccum > 0.0 && daccum < 5.0)
        #endif
      {
        #ifdef VV_DYNAMIC_DCOUNT
        dcount = 9.0;
        #endif
        if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep, -cubetstep, 0.0)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep,  cubetstep, 0.0)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep, -cubetstep, 0.0)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep,  cubetstep, 0.0)).r+VV_BLUR8_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_BLUR16
          #ifdef VV_SMAP_BLUR_FAST16
          if (daccum > 0.0 && daccum < 9.0)
          #endif
        {
          #ifdef VV_DYNAMIC_DCOUNT
          dcount = 17.0;
          #endif
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, 0.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep*2.0, 0.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0, -cubetstep*2.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(0.0,  cubetstep*2.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, -cubetstep*2.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3(-cubetstep*2.0,  cubetstep*2.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep*2.0, -cubetstep*2.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, ltfdir+vec3( cubetstep*2.0,  cubetstep*2.0, 0.0)).r+VV_BLUR16_MUL(bias) >= currentDistanceToLight) daccum += 1.0;
        }
        #endif
      }
      #endif
    }
    if (daccum <= 0.0) discard;
    #ifdef VV_DYNAMIC_DCOUNT
    float shadowMul = daccum/dcount;
    #else
    float shadowMul = daccum/DCOUNT;
    #endif
  #else
    // no blur
    if (textureCubeFn(ShadowTexture, ltfdir).r+bias < currentDistanceToLight) discard;
    float shadowMul = 1.0;
  #endif
  /*
  float currentDistanceToLight = (distanceToLight-u_nearFarPlane.x)/(u_nearFarPlane.y-u_nearFarPlane.x);
  currentDistanceToLight = clamp(currentDistanceToLight, 0, 1);
  */