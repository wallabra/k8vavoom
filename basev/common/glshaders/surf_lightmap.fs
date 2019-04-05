#version 120
$include "common/common.inc"

// fragment shader for lightmapped surfaces

uniform sampler2D Texture;
uniform sampler2D LightMap;
uniform sampler2D SpecularMap;
#ifdef VV_LIGHTMAP_BRIGHTMAP
$include "common/brightmap_vars.fs"
#endif

$include "common/fog_vars.fs"
$include "common/texlmap_vars.fs"

//#ifdef VV_AMBIENT_GLOW
$include "common/glow_vars.fs"
uniform float FullBright; // 0.0 or 1.0
uniform vec4 Light;
//#endif


void main () {
  //vec4 TexColour = texture2D(Texture, TextureCoordinate)*texture2D(LightMap, LightmapCoordinate)+texture2D(SpecularMap, LightmapCoordinate);
  //if (TexColour.a < 0.01) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard; // for steamlined masked textures //FIXME

  vec4 lt = texture2D(LightMap, LightmapCoordinate);
  lt.r = mix(lt.r, Light.r, FullBright);
  lt.g = mix(lt.g, Light.g, FullBright);
  lt.b = mix(lt.b, Light.b, FullBright);
  lt = calcGlow(lt);
#ifdef VV_LIGHTMAP_BRIGHTMAP
  $include "common/brightmap_calc.fs"
#endif
  TexColour *= lt;
  TexColour += texture2D(SpecularMap, LightmapCoordinate);

  TexColour = clamp(TexColour, 0.0, 1.0);

  vec4 FinalColour_1 = TexColour;
  $include "common/fog_calc.fs"

  gl_FragColor = FinalColour_1;
}
