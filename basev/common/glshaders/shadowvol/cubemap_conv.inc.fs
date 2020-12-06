// input is normalized, z is face number
// output is not normalized
vec3 convert_cube_uv_to_xyz (const vec3 texc) {
  vec3 res;
  // convert [0..1] to [-1..1]
  float uc = 2.0*texc.x-1.0;
  float vc = 2.0*texc.y-1.0;
       if (texc.z == 0.0) { res.x =   1.0; res.y =    vc; res.z =   -uc; } // positive x
  else if (texc.z == 1.0) { res.x =  -1.0; res.y =    vc; res.z =    uc; } // negative x
  else if (texc.z == 2.0) { res.x =    uc; res.y =   1.0; res.z =   -vc; } // positive y
  else if (texc.z == 3.0) { res.x =    uc; res.y =  -1.0; res.z =    vc; } // negative y
  else if (texc.z == 4.0) { res.x =    uc; res.y =    vc; res.z =   1.0; } // positive z
  else if (texc.z == 5.0) { res.x =   -uc; res.y =    vc; res.z =  -1.0; } // negative z
  else res = vec3(0.0, 0.0, 1.0);
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
    if (dir.x > 0.0) {
      // positive x
      // u (0 to 1) goes from +z to -z
      // v (0 to 1) goes from -y to +y
      res.x = -dir.z;
      res.y =  dir.y;
      res.z = 0.0;
    } else {
      // negative x
      // u (0 to 1) goes from -z to +z
      // v (0 to 1) goes from -y to +y
      res.x = dir.z;
      res.y = dir.y;
      res.z = 1.0;
    }
  } else if (absdir.y >= absdir.x && absdir.y >= absdir.z) {
    // positive y or negative y
    maxAxis = absdir.y;
    if (dir.y > 0.0) {
      // positive y
      // u (0 to 1) goes from -x to +x
      // v (0 to 1) goes from +z to -z
      res.x =  dir.x;
      res.y = -dir.z;
      res.z = 2.0;
    } else {
      // negative y
      // u (0 to 1) goes from -x to +x
      // v (0 to 1) goes from -z to +z
      res.x = dir.x;
      res.y = dir.z;
      res.z = 3.0;
    }
  } else {
    // positive z or negative z
    maxAxis = absdir.z;
    if (dir.z > 0.0) {
      // positive z
      // u (0 to 1) goes from -x to +x
      // v (0 to 1) goes from -y to +y
      res.x = dir.x;
      res.y = dir.y;
      res.z = 4.0;
    } else {
      // negative z
      // u (0 to 1) goes from +x to -x
      // v (0 to 1) goes from -y to +y
      res.x = -dir.x;
      res.y =  dir.y;
      res.z = 5.0;
    }
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
