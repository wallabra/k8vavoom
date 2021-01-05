// for now, "shitty bilinear" looks better on models
#ifdef VV_MODEL_LIGHTING
# ifndef VV_SMAP_SHITTY_BILINEAR
#  define VV_SMAP_SHITTY_BILINEAR
# endif
#endif

    float shadowMul;
    vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords

    // sampling is always to the right, and to the up
    float fX = fract(cubeTC.x*CubeSize);
    float fY = fract(cubeTC.y*CubeSize);

    float valat, valvert, valhoriz, valdiag;

    #ifdef VV_SMAP_SHITTY_BILINEAR
      valvert = 0.0;
      valhoriz = 0.0;
      valdiag = 0.0;
      float sldist;

      /*
      float biasMod = 1.0-clamp(dot(abs(Normal), abs(normV2L)), 0.0, 1.0);
      //float biasBase = 0.001+0.039*biasMod;
      float biasBase = 0.003+0.052*biasMod;
      //float biasBase = clamp(0.001+0.0065*biasMod, 0.0, 0.036); // cosTheta is dot( n,l ), clamped between 0 and 1
      */
      float biasBase = VV_CALC_BIAS;

      /*
      float cosTheta = clamp(dot(Normal, normV2L), 0.0, 1.0);
      float biasBase = clamp(0.0065*tan(acos(cosTheta)), 0.0015, 0.036); // cosTheta is dot( n,l ), clamped between 0 and 1
      */

      valat = compareShadowTexelDistanceBS(ltfdir, origDist, biasBase);

      sldist = textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(1.0, 0.0))).r+biasBase;
      #ifdef VV_SMAP_SQUARED_DIST
        sldist *= LightRadius;
        sldist *= sldist;
      #endif
      if (sldist >= origDist) valhoriz = 1.0;

      sldist = textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(0.0, 1.0))).r+biasBase;
      #ifdef VV_SMAP_SQUARED_DIST
        sldist *= LightRadius;
        sldist *= sldist;
      #endif
      if (sldist >= origDist) valvert = 1.0;

      sldist = textureCubeFn(ShadowTexture, shift_cube_uv_slow(cubeTC, vec2(1.0, 1.0))).r+biasBase+0.0014;
      #ifdef VV_SMAP_SQUARED_DIST
        sldist *= LightRadius;
        sldist *= sldist;
      #endif
      if (sldist >= origDist) valdiag = 1.0;

    #else
        vec3 ltf_horiz, ltf_vert, ltf_diag;
        float ttexX = 2.0*cubeTC.x-1.0;
        float ttexY = 2.0*cubeTC.y-1.0;
        //float tshift = 1.0/CubeSize;
        float tshift2 = 2.0/CubeSize;

        float uc, vc, vc1;
        float t, dv;
        float newCubeDist;
        vec3 newCubeDir;
        float sldist;
        #define orgDist  origDist
        // this fixes checks on cube edges
        #define CUBE_FIX_EDGES
        // my edge fixing algo is totally wrong (see cube faces to understand why)
        // use "valat" at edge instead for now
        #define CUBE_FIX_EDGES_BAD_GUESS
        //#define CUBE_FIX_EDGES_norm(v_)  v_
        #define CUBE_FIX_EDGES_norm(v_)  normalize(v_)

        // for models, it is better to use bias, otherwise we'll get alot more wrong shadows
        /*
        #ifdef VV_SURFACE_LIGHTING
        valat = compareShadowTexelDistance(ltfdir, origDist);
        #else
        float biasBase = VV_CALC_BIAS;
        valat = compareShadowTexelDistanceBS(ltfdir, origDist, biasBase);
        #endif
        */
        valat = 0.0;
        valhoriz = 0.0;
        valvert = 0.0;
        valdiag = 0.0;

        #define SMCHECK_ASSIGN
        if (cubeTC.z == 0.0) {
          // positive x
          ltf_horiz = vec3(1.0, ttexY, -(ttexX+tshift2));
          ltf_vert = vec3(1.0, ttexY+tshift2, -ttexX);
          ltf_diag = vec3(1.0, ttexY+tshift2, -(ttexX+tshift2));
          #define SMCHECK_V3  vec3(1.0, vc, -uc)
          $include "shadowvol/cubemap_bilinear_xcheck.fs"
          #undef SMCHECK_V3
        } else if (cubeTC.z == 1.0) {
          // negative x
          ltf_horiz = vec3(-1.0, ttexY, ttexX+tshift2);
          ltf_vert = vec3(-1.0, ttexY+tshift2, ttexX);
          ltf_diag = vec3(-1.0, ttexY+tshift2, ttexX+tshift2);
          #define SMCHECK_V3  vec3(-1.0, vc, uc)
          $include "shadowvol/cubemap_bilinear_xcheck.fs"
          #undef SMCHECK_V3
        } else if (cubeTC.z == 2.0) {
          // positive y
          ltf_horiz = vec3(ttexX+tshift2, 1.0, -ttexY);
          ltf_vert = vec3(ttexX, 1.0, -(ttexY+tshift2));
          ltf_diag = vec3(ttexX+tshift2, 1.0, -(ttexY+tshift2));
          #define SMCHECK_V3  vec3(uc, 1.0, -vc)
          $include "shadowvol/cubemap_bilinear_xcheck.fs"
          #undef SMCHECK_V3
        } else if (cubeTC.z == 3.0) {
          // negative y
          ltf_horiz = vec3(ttexX+tshift2, -1.0, ttexY);
          ltf_vert = vec3(ttexX, -1.0, ttexY+tshift2);
          ltf_diag = vec3(ttexX+tshift2, -1.0, ttexY+tshift2);
          #define SMCHECK_V3  vec3(uc, -1.0, vc)
          $include "shadowvol/cubemap_bilinear_xcheck.fs"
          #undef SMCHECK_V3
        } else if (cubeTC.z == 4.0) {
          // positive z
          ltf_horiz = vec3(ttexX+tshift2, ttexY, 1.0);
          ltf_vert = vec3(ttexX, ttexY+tshift2, 1.0);
          ltf_diag = vec3(ttexX+tshift2, ttexY+tshift2, 1.0);
          #define SMCHECK_V3  vec3(uc, vc, 1.0)
          $include "shadowvol/cubemap_bilinear_xcheck.fs"
          #undef SMCHECK_V3
        } else {
          // negative z
          ltf_horiz = vec3(-(ttexX+tshift2), ttexY, -1.0);
          ltf_vert = vec3(-ttexX, ttexY+tshift2, -1.0);
          ltf_diag = vec3(-(ttexX+tshift2), ttexY+tshift2, -1.0);
          #define SMCHECK_V3  vec3(-uc, vc, -1.0)
          $include "shadowvol/cubemap_bilinear_xcheck.fs"
          #undef SMCHECK_V3
        }
        #undef SMCHECK_ASSIGN
    #endif

    float daccum = valat+valhoriz+valvert+valdiag;
    if (daccum <= 0.0) discard;

    // bilinear filtering
    shadowMul = mix(mix(valat, valhoriz, fX), mix(valvert, valdiag, fX), fY);
