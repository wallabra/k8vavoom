// decal renderer for regular case (normal and lightmapped surfaces)

uniform sampler2D Texture;
#ifdef REG_LIGHTMAP
uniform sampler2D LightMap;
uniform sampler2D SpecularMap;
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
  vec4 FinalColour_1;
  vec4 TexColour;

  if (SplatAlpha <= 0.01) discard;

  TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard;

  FinalColour_1.a = clamp(TexColour.a*SplatAlpha, 0.0, 1.0);
  if (FinalColour_1.a < 0.01) discard;
  FinalColour_1.rgb = TexColour.rgb;

#ifdef REG_LIGHTMAP
  // lightmapped
  vec4 lmc = texture2D(LightMap, LightmapCoordinate);
  vec4 spc = texture2D(SpecularMap, LightmapCoordinate);
  FinalColour_1.rgb *= lmc.rgb;
  FinalColour_1.rgb += spc.rgb;
#else
  // normal
  FinalColour_1.rgb *= Light.rgb;
  FinalColour_1.rgb *= Light.a;
#endif
  FinalColour_1.rgb = clamp(FinalColour_1.rgb, 0.0, 1.0);

  $include "common/fog_calc.fs"

  if (FinalColour_1.a < 0.01) discard;

  // convert to premultiplied
  FinalColour_1.rgb *= FinalColour_1.a;

  gl_FragColor = FinalColour_1;
}
