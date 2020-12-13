#define VV_MODEL_LIGHTING

uniform sampler2D Texture;
uniform vec3 LightColor;
uniform float LightRadius;
uniform float LightMin;
uniform float InAlpha;
uniform bool AllowTransparency;
#ifdef VV_SPOTLIGHT
$include "common/spotlight_vars.fs"
#endif

varying vec3 VNormal;
varying vec3 VertToLight;
/*
varying float Dist;
varying float VDist;
*/
varying float SurfDist;

varying vec2 TextureCoordinate;

vec3 Normal;

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

  // renormalize normal, because it may be distorted due to interpolation
  Normal = normalize(VNormal);

  $include "shadowvol/common_light.fs"
}
