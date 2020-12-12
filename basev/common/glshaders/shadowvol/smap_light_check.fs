// use more math instead of more conditions
//#define VV_SMAP_FILTER_OLD

// set by the endine
//#define VV_SMAP_SHITTY_BILINEAR

//#define VV_SMAP_SAMPLE(ofs_,bias_)  (sign(sign(textureCubeFn(ShadowTexture, (ofs_)).r+(bias_)-currentDistanceToLight)+0.5))

#define VV_SMAP_SAMPLE_SET(var_,ofs_)  (var_) += compareShadowTexelDistance(ofs_, origDist)
#define VV_SMAP_SAMPLE_ADD(var_,ofs_)  (var_) += compareShadowTexelDistance(normalize(ofs_), origDist)

  //VV_SMAP_BIAS = VV_SMAP_BIAS_N/LightRadius;

  // normalized distance to the point light source
  // hardware doesn't require that, but our cubemap calculations do
  vec3 fullltfdir = -VertToLight;
  // use squared distance in comparisons
  float origDist = dot(fullltfdir, fullltfdir);
  vec3 ltfdir = normalize(fullltfdir);

  #ifdef VV_SMAP_WEIGHTED_BLUR
    $include "shadowvol/cubemap_bilinear.fs"
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

      VV_SMAP_SAMPLE_SET(daccum, ltfdir);
      VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS(-1.0,  0.0));
      VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 1.0,  0.0));
      VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 0.0, -1.0));
      VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 0.0,  1.0));

      #ifdef VV_SMAP_BLUR8
        // perform 8-way blur
        #ifdef VV_SMAP_BLUR_FAST8
        if (daccum > 0.0 && daccum < 5.0)
        #endif
        {
          #ifdef VV_DYNAMIC_DCOUNT
          dcount = 9.0;
          #endif
          VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS(-1.0, -1.0));
          VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS(-1.0,  1.0));
          VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 1.0, -1.0));
          VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 1.0,  1.0));
          #ifdef VV_SMAP_BLUR16
            // perform 16-way blur
            #ifdef VV_DYNAMIC_DCOUNT
            dcount = 17.0;
            #endif
            VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS(-2.0,  0.0));
            VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 2.0,  0.0));
            VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 0.0, -2.0));
            VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 0.0,  2.0));
            VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS(-2.0, -2.0));
            VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS(-2.0,  2.0));
            VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 2.0, -2.0));
            VV_SMAP_SAMPLE_ADD(daccum, VV_SMAP_OFS( 2.0,  2.0));
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
        if (textureCubeFn(ShadowTexture, fullltfdir).r+bias1 < currentDistanceToLight) discard;
        #define shadowMul  1.0
      #else
        // distance from the light to the nearest shadow caster
        if (compareShadowTexelDistance(ltfdir, origDist) <= 0.0) discard;
        #define shadowMul  1.0
      #endif
    #endif
  #endif
  /*
  float currentDistanceToLight = (distanceToLight-u_nearFarPlane.x)/(u_nearFarPlane.y-u_nearFarPlane.x);
  currentDistanceToLight = clamp(currentDistanceToLight, 0, 1);
  */
