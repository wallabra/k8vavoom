// should be always defined
#define VV_CMP_FASTER_CHECKS

// this is set in the engine
// only one (or none) of the following must be defined
//#define VV_CMP_FASTEST_CHECKS
//#define VV_CMP_SHITTY_CHECKS
//#define VV_CMP_SUPER_SHITTY_CHECKS


float compareShadowTexelDistance (const vec3 ltfdir, float orgDist) {
  float sldist = textureCubeFn(ShadowTexture, ltfdir).r+VV_SMAP_BIAS;
  #ifdef VV_SMAP_SQUARED_DIST
    sldist *= LightRadius;
    sldist *= sldist;
  #endif
  if (sldist >= orgDist) return 1.0;

  // snap direction to texel corners
  vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords
  #ifdef VV_CMP_FASTER_CHECKS
    float texX = floor(cubeTC.x*CubeSize);
    float texY = floor(cubeTC.y*CubeSize);
    float uc, vc, vc1;
    float t, dv;
    float newCubeDist;
    vec3 newCubeDir;
    float ssd = SurfDist-dot(Normal, LightPos); // this is invariant
    if (cubeTC.z == 0.0) {
      // positive x
      #define SMCHECK_V3  vec3(1.0, vc, -uc)
      $include "shadowvol/cubemap_check_dispatch_inc.fs"
      #undef SMCHECK_V3
    }
    if (cubeTC.z == 1.0) {
      // negative x
      #define SMCHECK_V3  vec3(-1.0, vc, uc)
      $include "shadowvol/cubemap_check_dispatch_inc.fs"
      #undef SMCHECK_V3
    }
    if (cubeTC.z == 2.0) {
      // positive y
      #define SMCHECK_V3  vec3(uc, 1.0, -vc)
      $include "shadowvol/cubemap_check_dispatch_inc.fs"
      #undef SMCHECK_V3
    }
    if (cubeTC.z == 3.0) {
      // negative y
      #define SMCHECK_V3  vec3(uc, -1.0, vc)
      $include "shadowvol/cubemap_check_dispatch_inc.fs"
      #undef SMCHECK_V3
    }
    if (cubeTC.z == 4.0) {
      // positive z
      #define SMCHECK_V3  vec3(uc, vc, 1.0)
      $include "shadowvol/cubemap_check_dispatch_inc.fs"
      #undef SMCHECK_V3
    }
    // negative z
    #define SMCHECK_V3  vec3(-uc, vc, -1.0)
      $include "shadowvol/cubemap_check_dispatch_inc.fs"
    #undef SMCHECK_V3
  #else
    float texX = floor(cubeTC.x*CubeSize);
    float texY = floor(cubeTC.y*CubeSize);
    float newCubeDist;
    vec3 newCubeDir;
    float t, dv, dcd;

    // try 4 texel corners to find out which one is nearest
    // i am pretty sure that i can calculate it faster, but let's use this method for now
    cubeTC.x = (texX+0.01)/CubeSize;
    cubeTC.y = (texY+0.01)/CubeSize;
    // new direction vector
    newCubeDir = normalize(convert_cube_uv_to_xyz(cubeTC));
    // find the intersection of the surface plane and the ray from the light position
    dv = dot(Normal, newCubeDir);
    dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
    t = (SurfDist-dot(Normal, LightPos))/dv;
    newCubeDist = length(newCubeDir*t);
    //dcd = newCubeDist/LightRadius;
    dcd = newCubeDist*newCubeDist;
    if (sldist >= dcd) return 1.0;

    // corner #2
    //cubeTC.x = (texX+0.01)/CubeSize;
    cubeTC.y = (texY+0.99)/CubeSize;
    // new direction vector
    newCubeDir = normalize(convert_cube_uv_to_xyz(cubeTC));
    // find the intersection of the surface plane and the ray from the light position
    dv = dot(Normal, newCubeDir);
    dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
    t = (SurfDist-dot(Normal, LightPos))/dv;
    newCubeDist = length(newCubeDir*t);
    //dcd = newCubeDist/LightRadius;
    dcd = newCubeDist*newCubeDist;
    if (sldist >= dcd) return 1.0;

    // corner #3
    cubeTC.x = (texX+0.99)/CubeSize;
    //cubeTC.y = (texY+0.99)/CubeSize;
    // new direction vector
    newCubeDir = normalize(convert_cube_uv_to_xyz(cubeTC));
    // find the intersection of the surface plane and the ray from the light position
    dv = dot(Normal, newCubeDir);
    dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
    (SurfDist-dot(Normal, LightPos))/dv;
    newCubeDist = length(newCubeDir*t);
    //dcd = newCubeDist/LightRadius;
    dcd = newCubeDist*newCubeDist;
    if (sldist >= dcd) return 1.0;

    // corner #4
    //cubeTC.x = (texX+0.99)/CubeSize;
    cubeTC.y = (texY+0.01)/CubeSize;
    // new direction vector
    newCubeDir = normalize(convert_cube_uv_to_xyz(cubeTC));
    // find the intersection of the surface plane and the ray from the light position
    dv = dot(Normal, newCubeDir);
    dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
    t = (SurfDist-dot(Normal, LightPos))/dv;
    newCubeDist = length(newCubeDir*t);
    //dcd = newCubeDist/LightRadius;
    dcd = newCubeDist*newCubeDist;
    if (sldist >= dcd) return 1.0;

    return 0.0;
  #endif
}
