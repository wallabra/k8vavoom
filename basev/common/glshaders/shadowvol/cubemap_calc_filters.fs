#if 0
    vec3 cubeTC = convert_xyz_to_cube_uv(ltfdir); // texture coords
    vec3 cubeMDir = convert_cube_uv_to_xyz(cubeTC);
    //float cubeMOfs = convert_xyz_to_cube_uvofs(ltfdir);
    vec2 cubeXMul, cubeYMul, cubeZMul;
    vec3 absdir = abs(ltfdir);
    if (absdir.x >= absdir.y && absdir.x >= absdir.z) {
      // positive x or negative x
      cubeXMul = vec2(0.0, 0.0);
      cubeYMul = vec2(1.0, 0.0);
      cubeZMul = vec2(0.0, 1.0);
    } else if (absdir.y >= absdir.x && absdir.y >= absdir.z) {
      // positive y or negative y
      cubeXMul = vec2(1.0, 0.0);
      cubeYMul = vec2(0.0, 0.0);
      cubeZMul = vec2(0.0, 1.0);
    } else {
      // positive z or negative z
      cubeXMul = vec2(1.0, 0.0);
      cubeYMul = vec2(0.0, 1.0);
      cubeZMul = vec2(0.0, 0.0);
    }
    float cubeDVC = 2.0/CubeSize;
    #define VV_SMAP_OFS(ox_,oy_)  (vec3(cubeMDir.x+ox_*cubeDVC*cubeXMul.x+oy_*cubeDVC*cubeXMul.y, cubeMDir.y+ox_*cubeDVC*cubeYMul.x+oy_*cubeDVC*cubeYMul.y, cubeMDir.z+ox_*cubeDVC*cubeZMul.x+oy_*cubeDVC*cubeZMul.y)

#else
  // convert dir to "linear texture direction", set mul vectors
  vec3 cubeTDir = ltfdir;
  vec2 cubeXMul, cubeYMul, cubeZMul;

  vec3 absdir = abs(ltfdir);
  if (absdir.x >= absdir.y && absdir.x >= absdir.z) {
    // positive x or negative x
    cubeXMul = vec2(0.0, 0.0);
    cubeYMul = vec2(1.0, 0.0);
    cubeZMul = vec2(0.0, 1.0);

    cubeTDir.x = sign(ltfdir.x);
    cubeTDir.y /= absdir.x;
    cubeTDir.z /= absdir.x;
  } else if (absdir.y >= absdir.x && absdir.y >= absdir.z) {
    // positive y or negative y
    cubeXMul = vec2(1.0, 0.0);
    cubeYMul = vec2(0.0, 0.0);
    cubeZMul = vec2(0.0, 1.0);

    cubeTDir.x /= absdir.y;
    cubeTDir.y = sign(ltfdir.y);
    cubeTDir.z /= absdir.y;
  } else {
    // positive z or negative z
    cubeXMul = vec2(1.0, 0.0);
    cubeYMul = vec2(0.0, 1.0);
    cubeZMul = vec2(0.0, 0.0);

    cubeTDir.x /= absdir.z;
    cubeTDir.y /= absdir.z;
    cubeTDir.z = sign(ltfdir.z);
  }

  float cubeDVC = 2.0/CubeSize;
  #define VV_SMAP_OFS(ox_,oy_)  (vec3(cubeTDir.x+ox_*cubeDVC*cubeXMul.x+oy_*cubeDVC*cubeXMul.y, cubeTDir.y+ox_*cubeDVC*cubeYMul.x+oy_*cubeDVC*cubeYMul.y, cubeTDir.z+ox_*cubeDVC*cubeZMul.x+oy_*cubeDVC*cubeZMul.y)

#endif
