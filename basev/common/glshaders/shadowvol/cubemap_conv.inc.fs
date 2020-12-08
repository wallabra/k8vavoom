// input is normalized, z is face number
// output is not normalized
vec3 convert_cube_uv_to_xyz (const vec3 texc) {
  // convert [0..1] to [-1..1]
  float uc = 2.0*texc.x-1.0;
  float vc = 2.0*texc.y-1.0;
  if (texc.z == 0.0) return vec3(1.0, vc, -uc); // positive x
  if (texc.z == 1.0) return vec3(-1.0, vc, uc); // negative x
  if (texc.z == 2.0) return vec3(uc, 1.0, -vc); // positive y
  if (texc.z == 3.0) return vec3(uc, -1.0, vc); // negative y
  if (texc.z == 4.0) return vec3(uc, vc, 1.0); // positive z
  return vec3(-uc, vc, -1.0); // negative z
}


// given the texture coordinates, calculate sampling direction, and weight factors
// `x` and `y` are the directions
// `z` and `w` are [0..1] proximity to the corresponding texel edge
// (so 1.0-prox is the weight for the neighbouring texel)
vec4 calc_blur_weight_dir (const vec3 texc) {
  vec4 res;
  float texU = texc.x*CubeSize;
  float texV = texc.y*CubeSize;
  // fractional parts gives texel offsets
  res.z = fract(texU)-0.5;
  res.w = fract(texV)-0.5;
  // horizontal
  res.x = sign(res.z);
  res.z = 0.5-abs(res.z);
  // vertical
  res.y = sign(res.w);
  res.w = 0.5-abs(res.w);
  return res;
}


// given the texture coordinates, calculate weight factors
// `x` and `y` are the offset of left-bottom; use +1 to get the neighbour
// `z` and `w` are the weight factors
// sampling is always to the right, and to the up
vec4 calc_blur_weight_dir_new (const vec3 texc) {
  vec4 res;
#if 0
  float fX = fract(texc.x*CubeSize);
  float fY = fract(texc.y*CubeSize);
  if (fX < 0.5) {
    // move left
    res.x = -1.0;
    res.z = 1.0-fX;
  } else {
    res.x = 0.0;
    res.z = fX;
  }
  if (fY < 0.5) {
    // move down
    res.y = -1.0;
    res.w = 1.0-fY;
  } else {
    res.y = 0.0;
    res.w = fY;
  }
#else
  res.x = 0;
  res.y = 0;
  res.z = fract(texc.x*CubeSize);
  res.w = fract(texc.y*CubeSize);
#endif
  return res;
}


// input is normalized
// z is face number
vec3 convert_xyz_to_cube_uv (const vec3 dir) {
  vec3 res;
  float maxAxis;

  vec3 absdir = abs(dir);
  if (absdir.x >= absdir.y && absdir.x >= absdir.z) {
    // positive x or negative x
    maxAxis = absdir.x;
    res.x = -dir.z*sign(dir.x);
    res.y = dir.y;
    res.z = 1.0-max(sign(dir.x), 0.0);
  } else if (absdir.y >= absdir.x && absdir.y >= absdir.z) {
    // positive y or negative y
    maxAxis = absdir.y;
    res.x = dir.x;
    res.y = -dir.z*sign(dir.y);
    res.z = 3.0-max(sign(dir.y), 0.0);
  } else {
    // positive z or negative z
    maxAxis = absdir.z;
    res.x = dir.x*sign(dir.z);
    res.y = dir.y;
    res.z = 5.0-max(sign(dir.z), 0.0);
  }

  // convert [-1..1] to [0..1]
  res.x = 0.5*(res.x/maxAxis+1.0);
  res.y = 0.5*(res.y/maxAxis+1.0);
  return res;
}


// input is `convert_cube_uv_to_xyz()` result
// returns new dir, not normalized
// it doesn't matter in which order we'll sample the texels,
// so we can go with simplier thing
vec3 shift_cube_uv_slow (vec3 tc, const vec2 shift) {
  tc.x += shift.x/CubeSize;
  tc.y += shift.y/CubeSize;
  return convert_cube_uv_to_xyz(tc);
}


#define VV_CMP_FASTER_CHECKS

float compareShadowTexelDistance (const vec3 ltfdir, float orgDist) {
  float sldist = (textureCubeFn(ShadowTexture, ltfdir).r+VV_SMAP_BIAS)*LightRadius;
  sldist *= sldist;
  if (sldist >= orgDist) return 1.0;

  // snap direction to texel center
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
      $include "shadowvol/cubemap_check_inc.fs"
      #undef SMCHECK_V3
    }
    if (cubeTC.z == 1.0) {
      // negative x
      #define SMCHECK_V3  vec3(-1.0, vc, uc)
      $include "shadowvol/cubemap_check_inc.fs"
      #undef SMCHECK_V3
    }
    if (cubeTC.z == 2.0) {
      // positive y
      #define SMCHECK_V3  vec3(uc, 1.0, -vc)
      $include "shadowvol/cubemap_check_inc.fs"
      #undef SMCHECK_V3
    }
    if (cubeTC.z == 3.0) {
      // negative y
      #define SMCHECK_V3  vec3(uc, -1.0, vc)
      $include "shadowvol/cubemap_check_inc.fs"
      #undef SMCHECK_V3
    }
    if (cubeTC.z == 4.0) {
      // positive z
      #define SMCHECK_V3  vec3(uc, vc, 1.0)
      $include "shadowvol/cubemap_check_inc.fs"
      #undef SMCHECK_V3
    }
    // negative z
    #define SMCHECK_V3  vec3(-uc, vc, -1.0)
    $include "shadowvol/cubemap_check_inc.fs"
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
