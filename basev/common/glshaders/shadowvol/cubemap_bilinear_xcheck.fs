#define SMCHECK_ASSIGN

  float ssd = SurfDist-dot(Normal, LightPos); // this is invariant

#define destvar    valhoriz
#define tt_ltfdir  ltf_horiz
#define tt_texX    cubeTC.x+tshift
#define tt_texY    cubeTC.y
  sldist = textureCubeFn(ShadowTexture, tt_ltfdir).r+VV_SMAP_BIAS;
  #ifdef VV_SMAP_SQUARED_DIST
    sldist *= LightRadius;
    sldist *= sldist;
  #endif
  if (sldist >= orgDist) {
    destvar = 1.0;
  } else {
    float texX = floor((tt_texX)*CubeSize);
    float texY = floor((tt_texY)*CubeSize);
    $include "shadowvol/cubemap_check_dispatch_inc.fs"
  }
#undef destvar
#undef tt_ltfdir
#undef tt_texX
#undef tt_texY


#define destvar    valvert
#define tt_ltfdir  ltf_vert
#define tt_texX    cubeTC.x
#define tt_texY    cubeTC.y+tshift
  sldist = textureCubeFn(ShadowTexture, tt_ltfdir).r+VV_SMAP_BIAS;
  #ifdef VV_SMAP_SQUARED_DIST
    sldist *= LightRadius;
    sldist *= sldist;
  #endif
  if (sldist >= orgDist) {
    destvar = 1.0;
  } else {
    float texX = floor((tt_texX)*CubeSize);
    float texY = floor((tt_texY)*CubeSize);
    $include "shadowvol/cubemap_check_dispatch_inc.fs"
  }
#undef destvar
#undef tt_ltfdir
#undef tt_texX
#undef tt_texY


#define destvar    valdiag
#define tt_ltfdir  ltf_diag
#define tt_texX    cubeTC.x+tshift
#define tt_texY    cubeTC.y+tshift
  sldist = textureCubeFn(ShadowTexture, tt_ltfdir).r+VV_SMAP_BIAS;
  #ifdef VV_SMAP_SQUARED_DIST
    sldist *= LightRadius;
    sldist *= sldist;
  #endif
  if (sldist >= orgDist) {
    destvar = 1.0;
  } else {
    float texX = floor((tt_texX)*CubeSize);
    float texY = floor((tt_texY)*CubeSize);
    $include "shadowvol/cubemap_check_dispatch_inc.fs"
  }
#undef destvar
#undef tt_ltfdir
#undef tt_texX
#undef tt_texY

#undef SMCHECK_ASSIGN
