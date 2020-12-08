#define VV_SURFACE_LIGHTING

uniform vec3 LightColor;
uniform float LightRadius;
uniform float LightMin;
//$include "common/texshade.inc" // in "texture_vars.fs"
#ifdef VV_SPOTLIGHT
$include "common/spotlight_vars.fs"
#endif

//varying vec3 Normal;
varying vec3 VertToLight;
varying float Dist;
varying float VDist;

#ifdef VV_SHADOW_CHECK_TEXTURE
uniform sampler2D Texture;
$include "common/texture_vars.fs"
#endif

#ifdef VV_SHADOWMAPS
$include "shadowvol/smap_light_decl.fs"
#endif


void main () {
  if (VDist <= 0.0 || Dist <= 0.0) discard;

#ifdef VV_SHADOW_CHECK_TEXTURE
  vec4 TexColor = GetStdTexelSimpleShade(Texture, TextureCoordinate);
  //if (TexColor.a < ALPHA_MIN) discard; //FIXME
  if (TexColor.a < ALPHA_MASKED) discard; // only normal and masked walls should go thru this
#endif

  $include "shadowvol/common_light.fs"
}
