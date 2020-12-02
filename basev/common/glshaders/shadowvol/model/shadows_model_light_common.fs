uniform sampler2D Texture;
uniform vec3 LightColor;
uniform float LightRadius;
uniform float LightMin;
uniform float InAlpha;
uniform bool AllowTransparency;
#ifdef VV_SPOTLIGHT
$include "common/spotlight_vars.fs"
#endif

varying vec3 Normal;
varying vec3 VertToLight;
/*
varying float Dist;
varying float VDist;
*/

varying vec2 TextureCoordinate;

#ifdef VV_SHADOWMAPS
$include "shadowvol/smap_light_decl.fs"
#endif

#define VV_SHADOW_CHECK_TEXTURE
#define VV_LIGHT_MODEL


void main () {
  //k8: don't do this until i fix normals, otherwise lighting looks weird
  //if (VDist <= 0.0 || Dist <= 0.0) discard;

  vec4 TexColor = texture2D(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard;
  TexColor.a *= InAlpha;
  if (!AllowTransparency) {
    if (TexColor.a < ALPHA_MASKED) discard;
  } else {
    if (TexColor.a < ALPHA_MIN) discard;
  }

  $include "shadowvol/common_light.fs"

#if 0
factored out
  float DistToLight = max(1.0, dot(VertToLight, VertToLight));
  if (DistToLight >= LightRadius*LightRadius) discard;

  vec3 normV2L = normalize(VertToLight);

#ifdef VV_SHADOWMAPS
  $include "shadowvol/smap_light_check.fs"
#endif

  DistToLight = sqrt(DistToLight);

  float attenuation = (LightRadius-DistToLight-LightMin)*(0.5+(0.5*dot(normV2L, Normal)));
#ifdef VV_SPOTLIGHT
  $include "common/spotlight_calc.fs"
#endif

  if (attenuation <= 0.0) discard;

  float ClampAdd = min(attenuation/255.0, 1.0);
  attenuation = ClampAdd;

  float ClampTrans = clamp((TexColor.a-0.1)/0.9, 0.0, 1.0);
  if (ClampTrans < ALPHA_MIN) discard;

  vec4 FinalColor;
  FinalColor.rgb = LightColor;
  FinalColor.a = ClampAdd*(ClampTrans*(ClampTrans*(3.0-(2.0*ClampTrans))));

  gl_FragColor = FinalColor;
#endif
}
