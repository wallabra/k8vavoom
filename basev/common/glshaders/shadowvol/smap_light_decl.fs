uniform samplerCube ShadowTexture;
varying vec3 VertWorldPos;
uniform float CubeSize;

uniform vec3 LightPos;

#ifdef VV_SURFACE_LIGHTING
uniform float SurfDist;
uniform vec3 SurfNormal;
#endif

#ifdef VV_MODEL_LIGHTING
float SurfDist;
#endif

//float VV_SMAP_BIAS;

$include "shadowvol/cubemap_conv.inc.fs"
