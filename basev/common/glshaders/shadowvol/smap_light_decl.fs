uniform samplerCube ShadowTexture;
// this will be #defined by the engine
//uniform float CubeSize;

uniform vec3 LightPos;

#ifdef VV_SURFACE_LIGHTING
uniform float SurfDist;
uniform vec3 SurfNormal;
#endif

#ifdef VV_MODEL_LIGHTING
//float SurfDist;
#endif

//float VV_SMAP_BIAS;

$include "shadowvol/smap_common_defines.inc"
$include "shadowvol/cubemap_conv.inc.fs"
