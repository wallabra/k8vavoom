#version 130
$include "common/common.inc"

uniform vec3 LightPos2;
uniform vec3 LightColor;
uniform float LightRadius;
uniform float LightMin;
uniform sampler2D Texture;
uniform samplerCube ShadowTexture;
//$include "common/texshade.inc" // in "texture_vars.fs"
#ifdef VV_SPOTLIGHT
$include "common/spotlight_vars.fs"
#endif

varying vec3 Normal;
varying vec3 VertToLight;
varying float Dist;
varying float VDist;

//varying vec4 VertLightDir;
varying vec3 VertWorldPos;

$include "common/texture_vars.fs"


void main () {
  if (VDist <= 0.0 || Dist <= 0.0) discard;

  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard; //FIXME
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this

  float DistToLight = max(1.0, dot(VertToLight, VertToLight));
  if (DistToLight >= LightRadius*LightRadius) discard;

  vec4 FinalColor;

  // difference between position of the light source and position of the fragment
  vec3 fromLightToFragment = LightPos2-VertWorldPos;
  // normalized distance to the point light source
  float distanceToLight = length(fromLightToFragment);
  // normalized direction from light source for sampling
  // (k8: there is no need to do that: this is just a direction, and hardware doesn't require it to be normalized)
  fromLightToFragment = normalize(fromLightToFragment);
  // sample shadow cube map
  //float referenceDistanceToLight = texture(ShadowTexture, -fromLightToFragment).r;
  vec3 ltfdir;
  ltfdir.x = -fromLightToFragment.x;
  ltfdir.y =  fromLightToFragment.y;
  ltfdir.z =  fromLightToFragment.z;
  float referenceDistanceToLight = texture(ShadowTexture, ltfdir).r;
  /*
  float currentDistanceToLight = (distanceToLight-u_nearFarPlane.x)/(u_nearFarPlane.y-u_nearFarPlane.x);
  currentDistanceToLight = clamp(currentDistanceToLight, 0, 1);
  */
  float currentDistanceToLight = distanceToLight/LightRadius;

  // dunno which one is better (or even which one is right, lol)
  #if 0
  float cosTheta = clamp(dot(Normal, normalize(VertToLight)), 0.0, 1.0);
  float bias = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
  #else
  float biasMod = 1.0-clamp(dot(Normal, normalize(VertToLight)), 0, 1);
  float bias = 0.001+0.026*biasMod;
  #endif
  // compare distances to determine whether the fragment is in shadow
  if (currentDistanceToLight > referenceDistanceToLight+bias) discard;

#if 1
  DistToLight = sqrt(DistToLight);

  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+(0.5*dot(normalize(VertToLight), Normal)));
#ifdef VV_SPOTLIGHT
  $include "common/spotlight_calc.fs"
#endif

  //attenuation *= shadowMult();

  if (attenuation <= 0.0) discard;

  float ClampAdd = min(attenuation/255.0, 1.0);
  attenuation = ClampAdd;

  float Transp = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);

  //vec4 FinalColor;
#ifdef VV_DEBUG_LIGHT
  //FinalColor = vec4(1.0, 0.5, 0.5, 1.0);
  FinalColor.rgb = LightColor;
  FinalColor.a = 1.0;
#else
  FinalColor.rgb = LightColor;
  FinalColor.a = ClampAdd*(Transp*(Transp*(3.0-(2.0*Transp))));
  //if (FinalColor.a < ALPHA_MIN) discard;
#endif

#endif
  gl_FragColor = FinalColor;
}
