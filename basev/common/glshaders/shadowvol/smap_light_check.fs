#ifdef VV_SMAP_BLUR8_FAST
# define VV_SMAP_BLUR8
# define VV_SMAP_BLUR_FAST8
#endif

#ifdef VV_SMAP_BLUR16_FAST
# define VV_SMAP_BLUR16
# define VV_SMAP_BLUR_FAST8
#endif

#ifdef VV_SMAP_BLUR16_FASTER
# define VV_SMAP_BLUR16
# define VV_SMAP_BLUR_FAST16
#endif

#ifdef VV_SMAP_BLUR16_FASTEST
# define VV_SMAP_BLUR16
# define VV_SMAP_BLUR_FAST8
# define VV_SMAP_BLUR_FAST16
#endif


// for each next blur, previous one must be defined
//#define VV_SMAP_BLUR4
//#define VV_SMAP_BLUR8
//#define VV_SMAP_BLUR16

// you can define VV_SMAP_BLUR_FAST16 without VV_SMAP_BLUR_FAST8 here
//#define VV_SMAP_BLUR_FAST8
//#define VV_SMAP_BLUR_FAST16

// ensure proper defines
#ifdef VV_SMAP_BLUR16
# ifndef VV_SMAP_BLUR8
#  define VV_SMAP_BLUR8
# endif
#endif

#ifdef VV_SMAP_BLUR8
# ifndef VV_SMAP_BLUR4
#  define VV_SMAP_BLUR4
# endif
#endif

#ifdef VV_SMAP_BLUR16
# define DCOUNT 17.0
#endif

#ifndef DCOUNT
# ifdef VV_SMAP_BLUR8
#  define DCOUNT 9.0
# endif
#endif

#ifndef DCOUNT
# ifdef VV_SMAP_BLUR4
#  define DCOUNT 5.0
# endif
#endif

#ifndef DCOUNT
# define DCOUNT 5.0
#endif

#ifndef VV_DYNAMIC_DCOUNT
# ifdef VV_SMAP_BLUR_FAST8
#  define VV_DYNAMIC_DCOUNT
# endif
#endif

#ifndef VV_DYNAMIC_DCOUNT
# ifdef VV_SMAP_BLUR_FAST16
#  define VV_DYNAMIC_DCOUNT
# endif
#endif

#ifdef VV_DYNAMIC_DCOUNT
# ifndef VV_SMAP_BLUR8
#  undef VV_DYNAMIC_DCOUNT
# endif
#endif

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

  #ifdef VV_SMAP_BLUR4
    float cubetstep = 1.0/CubeSize;
    float daccum = 0.0;
    #ifdef VV_DYNAMIC_DCOUNT
    float dcount = 5.0;
    #endif
    if (textureCube(ShadowTexture, ltfdir).r+bias >= currentDistanceToLight) daccum += 1.0;
    float maxaxis = max(ltfdir.x, max(ltfdir.y, ltfdir.z));
    if (ltfdir.x == maxaxis) {
      // do not touch x
      // y
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -cubetstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  cubetstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      // z
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0, -cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0,  cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
      #ifdef VV_SMAP_BLUR8
        #ifdef VV_SMAP_BLUR_FAST8
        if (daccum > 0.0 && daccum < 5.0)
        #endif
      {
        #ifdef VV_DYNAMIC_DCOUNT
        dcount = 9.0;
        #endif
        if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -cubetstep, -cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -cubetstep,  cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  cubetstep, -cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  cubetstep,  cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_BLUR16
          #ifdef VV_SMAP_BLUR_FAST16
          if (daccum > 0.0 && daccum < 9.0)
          #endif
        {
          #ifdef VV_DYNAMIC_DCOUNT
          dcount = 17.0;
          #endif
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -cubetstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  cubetstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0, -cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0,  cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -cubetstep*2.0, -cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -cubetstep*2.0,  cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  cubetstep*2.0, -cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  cubetstep*2.0,  cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        }
        #endif
      }
      #endif
    } else if (ltfdir.y == maxaxis) {
      // do not touch y
      // x
      if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      // z
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0, -cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0,  cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
      #ifdef VV_SMAP_BLUR8
        #ifdef VV_SMAP_BLUR_FAST8
        if (daccum > 0.0 && daccum < 5.0)
        #endif
      {
        #ifdef VV_DYNAMIC_DCOUNT
        dcount = 9.0;
        #endif
        if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep, 0.0, -cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep, 0.0,  cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep, 0.0, -cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep, 0.0,  cubetstep)).r+bias >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_BLUR16
          #ifdef VV_SMAP_BLUR_FAST16
          if (daccum > 0.0 && daccum < 9.0)
          #endif
        {
          #ifdef VV_DYNAMIC_DCOUNT
          dcount = 17.0;
          #endif
          if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep*2.0, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0, -cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, 0.0,  cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, 0.0, -cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, 0.0,  cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep*2.0, 0.0, -cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep*2.0, 0.0,  cubetstep*2.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        }
        #endif
      }
      #endif
    } else {
      // do not touch z
      // x
      if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      // y
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -cubetstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  cubetstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
      #ifdef VV_SMAP_BLUR8
        #ifdef VV_SMAP_BLUR_FAST8
        if (daccum > 0.0 && daccum < 5.0)
        #endif
      {
        #ifdef VV_DYNAMIC_DCOUNT
        dcount = 9.0;
        #endif
        if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep, -cubetstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep,  cubetstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep, -cubetstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep,  cubetstep, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
        #ifdef VV_SMAP_BLUR16
          #ifdef VV_SMAP_BLUR_FAST16
          if (daccum > 0.0 && daccum < 9.0)
          #endif
        {
          #ifdef VV_DYNAMIC_DCOUNT
          dcount = 17.0;
          #endif
          if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep*2.0, 0.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0, -cubetstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(0.0,  cubetstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep*2.0, -cubetstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3(-cubetstep*2.0,  cubetstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep*2.0, -cubetstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
          if (textureCube(ShadowTexture, ltfdir+vec3( cubetstep*2.0,  cubetstep*2.0, 0.0)).r+bias >= currentDistanceToLight) daccum += 1.0;
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
    if (textureCube(ShadowTexture, ltfdir).r+bias < currentDistanceToLight) discard;
    float shadowMul = 1.0;
  #endif
  /*
  float currentDistanceToLight = (distanceToLight-u_nearFarPlane.x)/(u_nearFarPlane.y-u_nearFarPlane.x);
  currentDistanceToLight = clamp(currentDistanceToLight, 0, 1);
  */
