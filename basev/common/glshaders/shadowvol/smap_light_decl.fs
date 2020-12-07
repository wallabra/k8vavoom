uniform samplerCube ShadowTexture;
varying vec3 VertWorldPos;
uniform float BiasMul;
uniform float BiasMax;
uniform float BiasMin;
uniform float CubeSize;
//uniform float CubeBlur;

uniform vec3 LightPos;
uniform float UseAdaptiveBias;

uniform float SurfDist;

$include "shadowvol/cubemap_conv.inc.fs"
