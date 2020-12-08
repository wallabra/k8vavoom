$include "shadowvol/smap_common_defines.inc"

// conditions seems to perform better
//#define VV_SMAP_MATH_SAMPLER

// use more math instead of more conditions
//#define VV_SMAP_FILTER_OLD

// try to do weighted 4-texel sampling?
//#define VV_SMAP_WEIGHTED_BLUR

#ifdef VV_SMAP_WEIGHTED_BLUR
# ifdef VV_SMAP_BLUR8
#  undef VV_SMAP_WEIGHTED_BLUR
# endif
#endif

#ifdef VV_SMAP_WEIGHTED_BLUR
# ifndef VV_SMAP_BLUR4
#  undef VV_SMAP_WEIGHTED_BLUR
# endif
#endif

#define VV_SMAP_SAMPLE(ofs_,bias_)  (sign(sign(textureCubeFn(ShadowTexture, (ofs_)).r+(bias_)-currentDistanceToLight)+0.5))


  // dunno which one is better (or even which one is right, lol)
  // fun thing: it seems that turning on cubemap texture filtering removes moire almost completely
  // so if the user will turn on filtering, we will turn off "adaptive" bias with `UseAdaptiveBias`
  #if 1
  float biasBase, bias1;
  // 0.026
  // 0.039
  if (UseAdaptiveBias > 0.0) {
    #if 1
    float cosTheta = clamp(dot(Normal, normV2L), 0.0, 1.0);
    //biasBase = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
    biasBase = clamp(BiasMul*tan(acos(cosTheta)), BiasMin, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
    //biasBase = 0.072; //clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
    //biasBase = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.072); // cosTheta is dot( n,l ), clamped between 0 and 1
    //biasBase = 0.0001;
    bias1 = biasBase;
    #else
    biasBase = max(0.05*(1.0-dot(abs(Normal), abs(normV2L))), 0.039);
    #endif
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
  vec3 ltfdir = VertWorldPos-LightPos;
  // normalized distance to the point light source
  float distanceToLight = length(ltfdir);
  float currentDistanceToLight = distanceToLight/LightRadius;
  // normalized direction from light source for sampling
  // (k8: there is no need to do that: this is just a direction, and hardware doesn't require it to be normalized)
  // (k8: but we may need normalized dir anyway)
  ltfdir = normalize(ltfdir);
  // sample shadow cube map

  #ifdef VV_SMAP_WEIGHTED_BLUR
  float shadowMul;

  if (UseAdaptiveBias+100.0 > 0.0) {
    vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords

  /*
    float x = texc.x*CubeSize;
    float y = texc.y*CubeSize;
    float x1 = floor(x);
    float y1 = floor(y);
    float x2 = x1+1;
    float y2 = y1+1;

    float fxy1 = (x2-x)/(x2-x1)*smp(x1,y1)+(x-x1)/(x2-x1)*smp(x2,y1)
    float fxy2 = (x2-x)/(x2-x1)*smp(x1,y2)+(x-x1)/(x2-x1)*smp(x2,y2)
    float fxy = (y2-y)/(y2-y1)*fxy1+(y-y1)/(y2-y1)*fxy2

    fxy1 = (x1-x+1)*smp(x1,y1) + (x-x1)*smp(x2,y1)
    fxy2 = (x1-x+1)*smp(x1,y2) + (x-x1)*smp(x2,y2)
    fxy = (y1-y+1)*fxy1 + (y-y1)*fxy2


    fxy1 = 1-fract(x)*smp(x1,y1) + fract(x)*smp(x2,y1)
    fxy2 = 1-fract(x)*smp(x1,y2) + fract(x)*smp(x2,y2)
    fxy = 1-fract(y)*fxy1 + fract(y)*fxy2
  */

    vec4 blurWD = calc_blur_weight_dir_new(cubeTC);
    #ifdef VV_SMAP_MATH_SAMPLER
    float valat    = VV_SMAP_SAMPLE(shift_cube_uv_slow(cubeTC, vec2(blurWD.x+0.0, blurWD.y+0.0)), bias1);
    float valhoriz = VV_SMAP_SAMPLE(shift_cube_uv_slow(cubeTC, vec2(blurWD.x+1.0, blurWD.y+0.0)), bias1);
    float valvert  = VV_SMAP_SAMPLE(shift_cube_uv_slow(cubeTC, vec2(blurWD.x+0.0, blurWD.y+1.0)), bias1);
    float valdiag  = VV_SMAP_SAMPLE(shift_cube_uv_slow(cubeTC, vec2(blurWD.x+1.0, blurWD.y+1.0)), bias1);
    #else
    //bias4 += 1.02;
    float valat = 0.0, valvert = 0.0, valhoriz = 0.0, valdiag = 0.0;
    if (textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(blurWD.x+0.0, blurWD.y+0.0))).r+bias1 >= currentDistanceToLight) valat = 1.0;
    if (textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(blurWD.x+1.0, blurWD.y+0.0))).r+bias1 >= currentDistanceToLight) valhoriz = 1.0;
    if (textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(blurWD.x+0.0, blurWD.y+1.0))).r+bias1 >= currentDistanceToLight) valvert = 1.0;
    if (textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(blurWD.x+1.0, blurWD.y+1.0))).r+bias1 >= currentDistanceToLight) valdiag = 1.0;
    #endif

    float fxy1 = (1.0-blurWD.z)*valat + blurWD.z*valhoriz;
    float fxy2 = (1.0-blurWD.z)*valvert + blurWD.z*valdiag;
    float fxy = (1.0-blurWD.w)*fxy1 + blurWD.w*fxy2;

    float daccum = fxy;

    if (daccum <= 0.0) discard;
    shadowMul = clamp(daccum, 0.0, 1.0);
  } else {
    //TODO: process cube edges
    #ifdef VV_SMAP_FILTER_OLD
    vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords
    #define VV_SMAP_OFS(ox_,oy_)  shift_cube_uv_slow(cubeTC, vec2(ox_, oy_))
    #else
    $include "shadowvol/cubemap_calc_filters.fs"
    #endif

    float daccum = 0.0;
    #ifdef VV_SMAP_MATH_SAMPLER
    daccum += VV_SMAP_SAMPLE(ltfdir, bias1);
    daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS(-1.0,  0.0), bias4);
    daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 1.0,  0.0), bias4);
    daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 0.0, -1.0), bias4);
    daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 0.0,  1.0), bias4);
    #else
    if (textureCubeFn(ShadowTexture, ltfdir).r+bias1 >= currentDistanceToLight) daccum += 1.0;
    if (textureCubeFn(ShadowTexture, VV_SMAP_OFS(-1.0,  0.0)).r+bias4 >= currentDistanceToLight) daccum += 1.0;
    if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 1.0,  0.0)).r+bias4 >= currentDistanceToLight) daccum += 1.0;
    if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 0.0, -1.0)).r+bias4 >= currentDistanceToLight) daccum += 1.0;
    if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 0.0,  1.0)).r+bias4 >= currentDistanceToLight) daccum += 1.0;
    #endif
    if (daccum <= 0.0) discard;
    shadowMul = daccum/5.0;
  }
  #else
    #ifdef VV_SMAP_BLUR4
      //float cubetstep = 1.0/CubeSize;
      float daccum = 0.0;
      #ifdef VV_DYNAMIC_DCOUNT
      float dcount = 5.0;
      #endif

      //TODO: process cube edges
      #ifdef VV_SMAP_FILTER_OLD
      vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords
      #define VV_SMAP_OFS(ox_,oy_)  shift_cube_uv_slow(cubeTC, vec2(ox_, oy_))
      #else
      $include "shadowvol/cubemap_calc_filters.fs"
      #endif

      #ifdef VV_SMAP_MATH_SAMPLER
      daccum += VV_SMAP_SAMPLE(ltfdir, bias1);
      daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS(-1.0,  0.0), bias4);
      daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 1.0,  0.0), bias4);
      daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 0.0, -1.0), bias4);
      daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 0.0,  1.0), bias4);
      #else
      if (textureCubeFn(ShadowTexture, ltfdir).r+bias1 >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, VV_SMAP_OFS(-1.0,  0.0)).r+bias4 >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 1.0,  0.0)).r+bias4 >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 0.0, -1.0)).r+bias4 >= currentDistanceToLight) daccum += 1.0;
      if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 0.0,  1.0)).r+bias4 >= currentDistanceToLight) daccum += 1.0;
      #endif

      #ifdef VV_SMAP_BLUR8
        // perform 8-way blur
        #ifdef VV_SMAP_BLUR_FAST8
        if (daccum > 0.0 && daccum < 5.0)
        #endif
        {
          #ifdef VV_DYNAMIC_DCOUNT
          dcount = 9.0;
          #endif
          #ifdef VV_SMAP_MATH_SAMPLER
          daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS(-1.0, -1.0), bias8);
          daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS(-1.0,  1.0), bias8);
          daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 1.0, -1.0), bias8);
          daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 1.0,  1.0), bias8);
          #else
          if (textureCubeFn(ShadowTexture, VV_SMAP_OFS(-1.0, -1.0)).r+bias8 >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, VV_SMAP_OFS(-1.0,  1.0)).r+bias8 >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 1.0, -1.0)).r+bias8 >= currentDistanceToLight) daccum += 1.0;
          if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 1.0,  1.0)).r+bias8 >= currentDistanceToLight) daccum += 1.0;
          #endif
          #ifdef VV_SMAP_BLUR16
            // perform 16-way blur
            #ifdef VV_SMAP_BLUR_FAST16
            if (daccum > 0.0 && daccum < 9.0)
            #endif
            {
              #ifdef VV_DYNAMIC_DCOUNT
              dcount = 17.0;
              #endif
              #ifdef VV_SMAP_MATH_SAMPLER
              daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS(-2.0,  0.0), bias16);
              daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 2.0,  0.0), bias16);
              daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 0.0, -2.0), bias16);
              daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 0.0,  2.0), bias16);
              daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS(-2.0, -2.0), bias16);
              daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS(-2.0,  2.0), bias16);
              daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 2.0, -2.0), bias16);
              daccum += VV_SMAP_SAMPLE(VV_SMAP_OFS( 2.0,  2.0), bias16);
              #else
              if (textureCubeFn(ShadowTexture, VV_SMAP_OFS(-2.0,  0.0)).r+bias16 >= currentDistanceToLight) daccum += 1.0;
              if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 2.0,  0.0)).r+bias16 >= currentDistanceToLight) daccum += 1.0;
              if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 0.0, -2.0)).r+bias16 >= currentDistanceToLight) daccum += 1.0;
              if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 0.0,  2.0)).r+bias16 >= currentDistanceToLight) daccum += 1.0;
              if (textureCubeFn(ShadowTexture, VV_SMAP_OFS(-2.0, -2.0)).r+bias16 >= currentDistanceToLight) daccum += 1.0;
              if (textureCubeFn(ShadowTexture, VV_SMAP_OFS(-2.0,  2.0)).r+bias16 >= currentDistanceToLight) daccum += 1.0;
              if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 2.0, -2.0)).r+bias16 >= currentDistanceToLight) daccum += 1.0;
              if (textureCubeFn(ShadowTexture, VV_SMAP_OFS( 2.0,  2.0)).r+bias16 >= currentDistanceToLight) daccum += 1.0;
              #endif
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
      #if 0
      //vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords
      //ltfdir = convert_cube_uv_to_xyz(cubeTC);
      if (textureCubeFn(ShadowTexture, ltfdir).r+bias1 < currentDistanceToLight) discard;
      float shadowMul = 1.0;
      #else

      // use constant distance for each shadowcube texel

      // distance from the light to the nearest shadow caster
      float sldist = textureCubeFn(ShadowTexture, ltfdir).r;
      #ifdef VV_MODEL_LIGHTING
      //sldist += bias1;
      float SurfDist = dot(Normal, VertWorldPos);
      sldist += 0.005; // this seems to be enough for surfaces
      #else
      sldist += 0.005; // this seems to be enough for surfaces
      #endif

      // snap direction to texel center
      vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords

      float texX = floor(cubeTC.x*CubeSize);
      float texY = floor(cubeTC.y*CubeSize);
      float newCubeDist;
      vec3 newCubeDir;

      // try 4 texel corners to find out which one is nearest
      // i am pretty sure that i can calculate it faster, but let's use this method for now
      cubeTC.x = (texX+0.01)/CubeSize;
      cubeTC.y = (texY+0.01)/CubeSize;
      // new direction vector
      newCubeDir = normalize(convert_cube_uv_to_xyz(cubeTC));
      // find the intersection of the surface plane and the ray from the light position
      float dv = dot(Normal, newCubeDir);
      float t = 0.0;
      if (abs(dv) >= 0.00001) t = (SurfDist-dot(Normal, LightPos))/dv;
      // t is "almost the distance" here
      newCubeDist = length(newCubeDir*t)/LightRadius;

      // corner #2
      //cubeTC.x = (texX+0.01)/CubeSize;
      cubeTC.y = (texY+0.99)/CubeSize;
      // new direction vector
      newCubeDir = normalize(convert_cube_uv_to_xyz(cubeTC));
      // find the intersection of the surface plane and the ray from the light position
      dv = dot(Normal, newCubeDir);
      t = 0.0;
      if (abs(dv) >= 0.00001) t = (SurfDist-dot(Normal, LightPos))/dv;
      // t is "almost the distance" here
      newCubeDist = min(newCubeDist, length(newCubeDir*t)/LightRadius);

      // corner #3
      cubeTC.x = (texX+0.99)/CubeSize;
      //cubeTC.y = (texY+0.99)/CubeSize;
      // new direction vector
      newCubeDir = normalize(convert_cube_uv_to_xyz(cubeTC));
      // find the intersection of the surface plane and the ray from the light position
      dv = dot(Normal, newCubeDir);
      t = 0.0;
      if (abs(dv) >= 0.00001) t = (SurfDist-dot(Normal, LightPos))/dv;
      // t is "almost the distance" here
      newCubeDist = min(newCubeDist, length(newCubeDir*t)/LightRadius);

      // corner #4
      //cubeTC.x = (texX+0.99)/CubeSize;
      cubeTC.y = (texY+0.01)/CubeSize;
      // new direction vector
      newCubeDir = normalize(convert_cube_uv_to_xyz(cubeTC));
      // find the intersection of the surface plane and the ray from the light position
      dv = dot(Normal, newCubeDir);
      t = 0.0;
      if (abs(dv) >= 0.00001) t = (SurfDist-dot(Normal, LightPos))/dv;
      // t is "almost the distance" here
      newCubeDist = min(newCubeDist, length(newCubeDir*t)/LightRadius);

      if (sldist < newCubeDist) discard;

      float shadowMul = 1.0;
      #endif
    #endif
  #endif
  /*
  float currentDistanceToLight = (distanceToLight-u_nearFarPlane.x)/(u_nearFarPlane.y-u_nearFarPlane.x);
  currentDistanceToLight = clamp(currentDistanceToLight, 0, 1);
  */
