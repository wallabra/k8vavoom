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
    #ifdef SMCHECK_ASSIGN
    if (sldist >= newCubeDist) destvar = 1.0;
    #else
    float xres = 0.0;
    if (sldist >= newCubeDist) xres = 1.0;
    return xres;
    //return min(sign(sldist-newCubeDist)+1.0, 1.0);
    #endif
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
    #ifdef SMCHECK_ASSIGN
    if (sldist >= newCubeDist) destvar = 1.0;
    #else
    if (sldist >= newCubeDist) return 1.0;
    #endif
    else
  #endif
  {
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
    #ifdef SMCHECK_ASSIGN
    if (sldist >= newCubeDist) destvar = 1.0;
    #else
    if (sldist >= newCubeDist) return 1.0;
    #endif
    else
    {
      // corner #2
      uc = 2.0*(texX+0.99)/CubeSize-1.0;
      vc = 2.0*(texY+0.99)/CubeSize-1.0;
      newCubeDir = SMCHECK_V3;
      dv = dot(Normal, newCubeDir);
      //dv += (1.0-abs(sign(dv)))*0.000001; // so it won't be zero
      t = ssd/dv;
      newCubeDir *= t;
      newCubeDist = dot(newCubeDir, newCubeDir);
      #ifdef SMCHECK_ASSIGN
      if (sldist >= newCubeDist) destvar = 1.0;
      #else
      if (sldist >= newCubeDist) return 1.0;
      else
      {
        return 0.0;
      }
      #endif
    }
  }
#endif
