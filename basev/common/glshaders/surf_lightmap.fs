#version 120
$include "common/common.inc"

// fragment shader for lightmapped surfaces

uniform sampler2D Texture;
uniform sampler2D LightMap;
uniform sampler2D SpecularMap;
#ifdef VV_LIGHTMAP_BRIGHTMAP
uniform sampler2D TextureBM;
#endif

$include "common/fog_vars.fs"
$include "common/texlmap_vars.fs"


void main () {
  //vec4 TexColour = texture2D(Texture, TextureCoordinate)*texture2D(LightMap, LightmapCoordinate)+texture2D(SpecularMap, LightmapCoordinate);
  //if (TexColour.a < 0.01) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard; // for steamlined masked textures //FIXME

  vec4 lt = texture2D(LightMap, LightmapCoordinate);
#ifdef VV_LIGHTMAP_BRIGHTMAP
  vec4 BMColor = texture2D(TextureBM, TextureCoordinate);
  BMColor.rgb *= BMColor.a;
  lt.r = max(lt.r, BMColor.r);
  lt.g = max(lt.g, BMColor.g);
  lt.b = max(lt.b, BMColor.b);
  //lt.rgb = BMColor.rgb;
#endif
  TexColour *= lt;
  TexColour += texture2D(SpecularMap, LightmapCoordinate);

  TexColour = clamp(TexColour, 0.0, 1.0);

  vec4 FinalColour_1 = TexColour;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColour_1;
}
