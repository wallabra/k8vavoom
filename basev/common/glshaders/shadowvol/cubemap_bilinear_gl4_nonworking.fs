    float shadowMul;
    vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords

    // sampling is always to the right, and to the up
    float fX = fract(cubeTC.x*CubeSize);
    float fY = fract(cubeTC.y*CubeSize);

    float valat, valvert, valhoriz, valdiag;

    #ifdef GLVER_MAJOR_44
      vec4 gather = textureGather(ShadowTexture, ltfdir);
      #define valat_u     (gather.x)
      #define valhoriz_u  (gather.z)
      #define valvert_u   (gather.w)
      #define valdiag_u   (gather.y)
    #else
      //valat = compareShadowTexelDistance(ltfdir, origDist);
      #define valat_u     textureCubeFn(ShadowTexture, ltfdir).r
      #define valhoriz_u  textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(1.0, 0.0))).r
      #define valvert_u   textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(0.0, 1.0))).r
      #define valdiag_u   textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(1.0, 1.0))).r
    #endif

    valat = compareShadowTexelDistanceEx(ltfdir, origDist, valat_u);

    #ifdef VV_SMAP_SHITTY_BILINEAR
      valvert = 0.0;
      valhoriz = 0.0;
      valdiag = 0.0;
      float sldist;

      float biasMod = 1.0-clamp(dot(Normal, normV2L), 0.0, 1.0);
      //float biasBase = 0.001+0.039*biasMod;
      float biasBase = 0.003+0.052*biasMod;
      //float biasBase = clamp(0.001+0.0065*biasMod, 0.0, 0.036); // cosTheta is dot( n,l ), clamped between 0 and 1

      /*
      float cosTheta = clamp(dot(Normal, normV2L), 0.0, 1.0);
      float biasBase = clamp(0.0065*tan(acos(cosTheta)), 0.0015, 0.036); // cosTheta is dot( n,l ), clamped between 0 and 1
      */

      sldist = (valhoriz_u+biasBase)*LightRadius;
      sldist *= sldist;
      if (sldist >= origDist) valhoriz = 1.0;

      sldist = (valvert_u+biasBase)*LightRadius;
      sldist *= sldist;
      if (sldist >= origDist) valvert = 1.0;

      sldist = (valdiag_u+biasBase)*LightRadius;
      sldist *= sldist;
      if (sldist >= origDist) valdiag = 1.0;

      /*
      sldist = (textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(1.0, 0.0))).r+biasBase)*LightRadius;
      sldist *= sldist;
      if (sldist >= origDist) valhoriz = 1.0;

      sldist = (textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(0.0, 1.0))).r+biasBase)*LightRadius;
      sldist *= sldist;
      if (sldist >= origDist) valvert = 1.0;

      sldist = (textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(1.0, 1.0))).r+biasBase+0.0014)*LightRadius;
      sldist *= sldist;
      if (sldist >= origDist) valdiag = 1.0;
      */

    #else
      valhoriz = compareShadowTexelDistanceEx(normalize(shift_cube_uv_slow(cubeTC, vec2(1.0, 0.0))), origDist, valhoriz_u);
      valvert = compareShadowTexelDistanceEx(normalize(shift_cube_uv_slow(cubeTC, vec2(0.0, 1.0))), origDist, valvert_u);
      valdiag = compareShadowTexelDistanceEx(normalize(shift_cube_uv_slow(cubeTC, vec2(1.0, 1.0))), origDist, valdiag_u);

      /*
      // sadly, new shifter is not working here
      #ifndef VV_SMAP_FILTER_OLD
      # define VV_SMAP_FILTER_OLD
      #endif
      #ifdef VV_SMAP_FILTER_OLD
        VV_SMAP_SAMPLE_SET(valhoriz, normalize(shift_cube_uv_slow(cubeTC, vec2(1.0, 0.0))));
        VV_SMAP_SAMPLE_SET(valvert,  normalize(shift_cube_uv_slow(cubeTC, vec2(0.0, 1.0))));
        VV_SMAP_SAMPLE_SET(valdiag,  normalize(shift_cube_uv_slow(cubeTC, vec2(1.0, 1.0))));
      #else
        $include "shadowvol/cubemap_calc_filters.fs"
        VV_SMAP_SAMPLE_SET(valhoriz, normalize(VV_SMAP_OFS(1.0, 0.0)));
        VV_SMAP_SAMPLE_SET(valvert,  normalize(VV_SMAP_OFS(0.0, 1.0)));
        VV_SMAP_SAMPLE_SET(valdiag,  normalize(VV_SMAP_OFS(1.0, 1.0)));
      #endif
      */
    #endif

    float daccum = valat+valhoriz+valvert+valdiag;
    if (daccum <= 0.0) discard;

    // bilinear filtering
    /*
    float fxy1 = (1.0-fX)*valat+fX*valhoriz;
    float fxy2 = (1.0-fX)*valvert+fX*valdiag;
    shadowMul = (1.0-fY)*fxy1+fY*fxy2;
    */
    shadowMul = mix(mix(valat, valhoriz, fX), mix(valvert, valdiag, fX), fY);

/*
#ifdef VV_CMP_SUPER_SHITTY_CHECKS
    // center
    uc = 2.0*(texX+0.5)/CubeSize-1.0;
    vc = 2.0*(texY+0.5)/CubeSize-1.0;
    newCubeDir = SMCHECK_V3;
    dv = dot(Normal, newCubeDir);
    //dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
    t = ssd/dv;
    newCubeDir *= t;
    newCubeDist = dot(newCubeDir, newCubeDir);
    if (sldist >= newCubeDist) return 1.0;
#else
  #ifndef VV_CMP_SHITTY_CHECKS
    // center
    uc = 2.0*(texX+0.5)/CubeSize-1.0;
    vc = 2.0*(texY+0.5)/CubeSize-1.0;
    newCubeDir = SMCHECK_V3;
    dv = dot(Normal, newCubeDir);
    //dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
    t = ssd/dv;
    newCubeDir *= t;
    newCubeDist = dot(newCubeDir, newCubeDir);
    if (sldist >= newCubeDist) return 1.0;
  #endif

    // corner #1
    uc = 2.0*(texX+0.01)/CubeSize-1.0;
    vc = 2.0*(texY+0.01)/CubeSize-1.0;
    vc1 = vc;
    newCubeDir = SMCHECK_V3;
    dv = dot(Normal, newCubeDir);
    //dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
    t = ssd/dv;
    newCubeDir *= t;
    newCubeDist = dot(newCubeDir, newCubeDir);
    if (sldist >= newCubeDist) return 1.0;

    // corner #2
    uc = 2.0*(texX+0.99)/CubeSize-1.0;
    vc = 2.0*(texY+0.99)/CubeSize-1.0;
    newCubeDir = SMCHECK_V3;
    dv = dot(Normal, newCubeDir);
    //dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
    t = ssd/dv;
    newCubeDir *= t;
    newCubeDist = dot(newCubeDir, newCubeDir);
    if (sldist >= newCubeDist) return 1.0;
#endif

    return 0.0;
*/
