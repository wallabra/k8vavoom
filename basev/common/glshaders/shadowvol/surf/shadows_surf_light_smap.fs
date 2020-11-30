#version 130
$include "common/common.inc"

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

varying vec4 VertLightDir;

$include "common/texture_vars.fs"


float shadowMult () {
  #if 0
  float lightDist = texture(ShadowTexture, VertLightDir.xyz).r;
  float myDist = length(VertLightDir.xyz);
  //if (myDist < lightDist+1.0) return 1.0; else return 0.0;
  if (lightDist > 8192.0) return 1.0; else return 0.0;
  #else
  float lightDist = texture(ShadowTexture, VertLightDir.xyz).r;
  float myDist = length(VertLightDir.xyz)/LightRadius;
  if (myDist < lightDist+0.0005) return 1.0; else return 0.0;
  #endif
}


void main () {
  if (VDist <= 0.0 || Dist <= 0.0) discard;

  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard; //FIXME
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this

  float DistToLight = max(1.0, dot(VertToLight, VertToLight));
  if (DistToLight >= LightRadius*LightRadius) discard;

  /*
  //float lightDist = texture(ShadowTexture, VertLightDir.xyz).r;
  float myDist = length(VertLightDir.xyz)/LightRadius;

  vec4 FinalColor = vec4(myDist, 0.0, 0.0, 1.0);
  //if (VertLightDir.x > 0) FinalColor.g = 1.0;
  if (VertLightDir.z > 0) FinalColor.b = 1.0;
  */

  //vec4 FinalColor = vec4(shadowMult(), 0.0, 0.0, 1.0);
  //float ld = texture(ShadowTexture, VertLightDir.xyz/VertLightDir.w).r;
  //float ld = texture(ShadowTexture, vec3(0, 0, 1)).r;
  float ld = texture(ShadowTexture, VertLightDir.xyz).r;
  float md = length(VertLightDir.xyz)/LightRadius;
  vec4 FinalColor;
  if (md < ld) {
    FinalColor = vec4(0.0, ld-md, 0.0, 1.0);
  } else {
    FinalColor = vec4(md-ld, 0.0, 0.0, 1.0);
  }

#if 0
  DistToLight = sqrt(DistToLight);

  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+(0.5*dot(normalize(VertToLight), Normal)));
#ifdef VV_SPOTLIGHT
  $include "common/spotlight_calc.fs"
#endif

  attenuation *= shadowMult();

  if (attenuation <= 0.0) discard;

  float ClampAdd = min(attenuation/255.0, 1.0);
  attenuation = ClampAdd;

  float Transp = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);

  vec4 FinalColor;
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
