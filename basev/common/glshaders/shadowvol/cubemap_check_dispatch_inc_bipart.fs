  sldist = textureCubeFn(ShadowTexture, tt_ltfdir).r+VV_SMAP_BIAS;
  #ifdef VV_MODEL_LIGHTING
  sldist += biasBase;
  #endif
  #ifdef VV_SMAP_SQUARED_DIST
    sldist *= LightRadius;
    sldist *= sldist;
  #endif
  if (sldist >= orgDist) {
    destvar = 1.0;
  } else {
    //float texX = floor((tt_texX)*CubeSize);
    //float texY = floor((tt_texY)*CubeSize);
    //$include "shadowvol/cubemap_check_dispatch_inc.fs"

    // trying to dynamically adjust texel coords doesn't work (moire)
    // use cube texel center
    //uc = 2.0*(texX+0.5)/CubeSize-1.0;
    //vc = 2.0*(texY+0.5)/CubeSize-1.0;
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
  }
