// decal renderer for regular case (normal and lightmapped surfaces)

uniform sampler2D Texture;
$include "common/texshade.inc"
#ifdef REG_LIGHTMAP
uniform sampler2D LightMap;
#ifdef VV_USE_OVERBRIGHT
uniform sampler2D SpecularMap;
#endif
#endif
#ifndef REG_LIGHTMAP
uniform vec4 Light;
#endif
uniform float SplatAlpha; // image alpha will be multiplied by this
$include "common/fog_vars.fs"

varying vec2 TextureCoordinate;
#ifdef REG_LIGHTMAP
varying vec2 LightmapCoordinate;
#endif


void main () {
  vec4 FinalColor;
  vec4 TexColor;

  if (SplatAlpha <= ALPHA_MIN) discard;

  TexColor = GetStdTexel(Texture, TextureCoordinate);
  if (TexColor.a < ALPHA_MIN) discard;

  FinalColor.a = clamp(TexColor.a*SplatAlpha, 0.0, 1.0);
  //if (FinalColor.a < ALPHA_MIN) discard;
  FinalColor.rgb = TexColor.rgb;

#ifdef REG_LIGHTMAP
  // lightmapped
  vec3 lmc = texture2D(LightMap, LightmapCoordinate).rgb;
#ifdef VV_USE_OVERBRIGHT
  vec3 spc = texture2D(SpecularMap, LightmapCoordinate).rgb;
#endif
  FinalColor.rgb *= lmc.rgb;
#ifdef VV_USE_OVERBRIGHT
  FinalColor.rgb += spc.rgb;
#endif
#else
  // normal
  FinalColor.rgb *= Light.rgb;
  FinalColor.rgb *= Light.a;
#endif
  FinalColor.rgb = clamp(FinalColor.rgb, 0.0, 1.0);

  $include "common/fog_calc.fs"

  //if (FinalColor.a < ALPHA_MIN) discard;

  // convert to premultiplied
  FinalColor.rgb *= FinalColor.a;

  gl_FragColor = FinalColor;
}
