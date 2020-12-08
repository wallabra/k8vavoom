// use more math instead of more conditions
//#define VV_SMAP_FILTER_OLD


//#define VV_SMAP_SAMPLE(ofs_,bias_)  (sign(sign(textureCubeFn(ShadowTexture, (ofs_)).r+(bias_)-currentDistanceToLight)+0.5))

#define VV_SMAP_SAMPLE(var_,ofs_)  (var_) += compareShadowTexelDistance(ofs_, origDist)

  //VV_SMAP_BIAS = VV_SMAP_BIAS_N/LightRadius;

  #ifdef VV_MODEL_LIGHTING
  SurfDist = dot(Normal, VertWorldPos);
  #endif

  // normalized distance to the point light source
  // hardware doesn't require that, but our cubemap calculations do
  vec3 fullltfdir = VertWorldPos-LightPos;
  float origDist = dot(fullltfdir, fullltfdir); //length(fullltfdir)/LightRadius;
  vec3 ltfdir = normalize(fullltfdir);
  // sample shadow cube map
  float sldist = textureCubeFn(ShadowTexture, ltfdir).r+VV_SMAP_BIAS;

  vec3 newCDir;
  #ifdef VV_SMAP_WEIGHTED_BLUR
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

    float shadowMul;
    vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords
    vec4 blurWD = calc_blur_weight_dir_new(cubeTC);
    float valat = 0.0, valvert = 0.0, valhoriz = 0.0, valdiag = 0.0;

    VV_SMAP_SAMPLE(valat,    shift_cube_uv_slow(cubeTC, vec2(blurWD.x+0.0, blurWD.y+0.0)));
    VV_SMAP_SAMPLE(valhoriz, shift_cube_uv_slow(cubeTC, vec2(blurWD.x+1.0, blurWD.y+0.0)));
    VV_SMAP_SAMPLE(valvert,  shift_cube_uv_slow(cubeTC, vec2(blurWD.x+0.0, blurWD.y+1.0)));
    VV_SMAP_SAMPLE(valdiag,  shift_cube_uv_slow(cubeTC, vec2(blurWD.x+1.0, blurWD.y+1.0)));

    // bilinear filtering
    float fxy1 = (1.0-blurWD.z)*valat + blurWD.z*valhoriz;
    float fxy2 = (1.0-blurWD.z)*valvert + blurWD.z*valdiag;
    float fxy = (1.0-blurWD.w)*fxy1 + blurWD.w*fxy2;

    float daccum = fxy;

    if (daccum <= 0.0) discard;
    shadowMul = clamp(daccum, 0.0, 1.0);
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

      VV_SMAP_SAMPLE(daccum, ltfdir);
      VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS(-1.0,  0.0));
      VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 1.0,  0.0));
      VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 0.0, -1.0));
      VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 0.0,  1.0));

      #ifdef VV_SMAP_BLUR8
        // perform 8-way blur
        #ifdef VV_SMAP_BLUR_FAST8
        if (daccum > 0.0 && daccum < 5.0)
        #endif
        {
          #ifdef VV_DYNAMIC_DCOUNT
          dcount = 9.0;
          #endif
          VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS(-1.0, -1.0));
          VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS(-1.0,  1.0));
          VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 1.0, -1.0));
          VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 1.0,  1.0));
          #ifdef VV_SMAP_BLUR16
            // perform 16-way blur
            #ifdef VV_SMAP_BLUR_FAST16
            if (daccum > 0.0 && daccum < 9.0)
            #endif
            {
              #ifdef VV_DYNAMIC_DCOUNT
              dcount = 17.0;
              #endif
              VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS(-2.0,  0.0));
              VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 2.0,  0.0));
              VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 0.0, -2.0));
              VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 0.0,  2.0));
              VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS(-2.0, -2.0));
              VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS(-2.0,  2.0));
              VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 2.0, -2.0));
              VV_SMAP_SAMPLE(daccum, VV_SMAP_OFS( 2.0,  2.0));
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
        //float distanceToLight = length(ltfdir);
        //float currentDistanceToLight = distanceToLight/LightRadius;
        //vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords
        //ltfdir = convert_cube_uv_to_xyz(cubeTC);
        if (textureCubeFn(ShadowTexture, ltfdir).r+bias1 < currentDistanceToLight) discard;
        float shadowMul = 1.0;
      #else
        // distance from the light to the nearest shadow caster
        if (compareShadowTexelDistance(ltfdir, origDist) <= 0.0) discard;
        float shadowMul = 1.0;
      #endif
    #endif
  #endif
  /*
  float currentDistanceToLight = (distanceToLight-u_nearFarPlane.x)/(u_nearFarPlane.y-u_nearFarPlane.x);
  currentDistanceToLight = clamp(currentDistanceToLight, 0, 1);
  */
