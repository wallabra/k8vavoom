#version 120
$include "common/common.inc"

uniform vec3 LightColor;
uniform float LightRadius;
uniform float LightMin;
uniform sampler2D Texture;
$include "common/texshade.inc"
#ifdef VV_SPOTLIGHT
$include "common/spotlight_vars.fs"
#endif

varying vec3 Normal;
varying vec3 VertToLight;
varying float Dist;
varying float VDist;

$include "common/texture_vars.fs"


void main () {
  if (VDist <= 0.0 || Dist <= 0.0) discard;

  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard; //FIXME
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this

  float DistToLight = max(1.0, dot(VertToLight, VertToLight));
  if (DistToLight >= LightRadius*LightRadius) discard;

  DistToLight = sqrt(DistToLight);

  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+(0.5*dot(normalize(VertToLight), Normal)));
#ifdef VV_SPOTLIGHT
  $include "common/spotlight_calc.fs"
#endif

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

  gl_FragColor = FinalColor;
}
