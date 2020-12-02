uniform vec3 LightColor;
uniform float LightRadius;
uniform float LightMin;
//$include "common/texshade.inc" // in "texture_vars.fs"
#ifdef VV_SPOTLIGHT
$include "common/spotlight_vars.fs"
#endif

varying vec3 Normal;
varying vec3 VertToLight;
varying float Dist;
varying float VDist;

#ifdef VV_SHADOW_CHECK_TEXTURE
uniform sampler2D Texture;
$include "common/texture_vars.fs"
#endif

#ifdef VV_SHADOWMAPS
uniform samplerCube ShadowTexture;
varying vec3 VertWorldPos;
uniform vec3 LightPos2;
uniform float BiasMul;
uniform float BiasMax;
uniform float BiasMin;
#endif


void main () {
  if (VDist <= 0.0 || Dist <= 0.0) discard;

#ifdef VV_SHADOW_CHECK_TEXTURE
  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard; //FIXME
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif

  float DistToLight = max(1.0, dot(VertToLight, VertToLight));
  if (DistToLight >= LightRadius*LightRadius) discard;

  vec3 normV2L = normalize(VertToLight);

#ifdef VV_SHADOWMAPS
  // difference between position of the light source and position of the fragment
  vec3 fromLightToFragment = LightPos2-VertWorldPos;
  // normalized distance to the point light source
  float distanceToLight = length(fromLightToFragment);
  // normalized direction from light source for sampling
  // (k8: there is no need to do that: this is just a direction, and hardware doesn't require it to be normalized)
  fromLightToFragment = normalize(fromLightToFragment);
  // sample shadow cube map
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
  #if 1
  // 0.026
  // 0.039
  float cosTheta = clamp(dot(Normal, normV2L), 0.0, 1.0);
  //float bias = clamp(0.0065*tan(acos(cosTheta)), 0.0, 0.026); // cosTheta is dot( n,l ), clamped between 0 and 1
  float bias = clamp(BiasMul*tan(acos(cosTheta)), BiasMin, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
  #else
  float biasMod = 1.0-clamp(dot(Normal, normV2L), 0.0, 1.0);
  //float bias = 0.001+0.039*biasMod;
  float bias = clamp(BiasMin+BiasMul*biasMod, 0.0, BiasMax); // cosTheta is dot( n,l ), clamped between 0 and 1
  #endif
  // compare distances to determine whether the fragment is in shadow
  if (currentDistanceToLight > referenceDistanceToLight+bias) discard;
#endif


  DistToLight = sqrt(DistToLight);
  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+(0.5*dot(normV2L, Normal)));
#ifdef VV_SPOTLIGHT
  $include "common/spotlight_calc.fs"
#endif

  if (attenuation <= 0.0) discard;
  float finalA = min(attenuation/255.0, 1.0);

#ifdef VV_SHADOW_CHECK_TEXTURE
  float transp = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);
  finalA *= transp*transp*(3.0-(2.0*transp));
#endif

  //gl_FragColor = vec4(clamp(Normal, 0.0, 1.0), 0.1);
#ifdef VV_SHADOWMAPS
  //gl_FragColor = vec4(clamp(fromLightToFragment.xyz, 0.0, 1.0), 0.1);
  gl_FragColor = vec4(LightColor, finalA);
#else
  gl_FragColor = vec4(LightColor, finalA);
#endif
}
