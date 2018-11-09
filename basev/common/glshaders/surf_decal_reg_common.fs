// decal renderer for regular case (normal and lightmapped surfaces)

uniform sampler2D Texture;
#ifdef REG_LIGHTMAP
uniform sampler2D LightMap;
uniform sampler2D SpecularMap;
#endif
uniform vec4 Light;
uniform vec4 SplatColour; // do recolor if .a is not zero
uniform float SplatAlpha; // image alpha will be multiplied by this
uniform bool FogEnabled;
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;

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

  if (SplatColour.a != 0.0) {
    FinalColour_1.r = SplatColour.r*TexColour.a*SplatAlpha; // convert to premultiplied
    FinalColour_1.g = SplatColour.g*TexColour.a*SplatAlpha; // convert to premultiplied
    FinalColour_1.b = SplatColour.b*TexColour.a*SplatAlpha; // convert to premultiplied
    FinalColour_1.a = clamp(TexColour.r*SplatAlpha, 0.0, 1.0);
  } else {
    FinalColour_1.r = TexColour.r*SplatAlpha; // convert to premultiplied
    FinalColour_1.g = TexColour.g*SplatAlpha; // convert to premultiplied
    FinalColour_1.b = TexColour.b*SplatAlpha; // convert to premultiplied
    FinalColour_1.a = clamp(TexColour.a*SplatAlpha, 0.0, 1.0);
  }
  if (FinalColour_1.a < 0.01) discard;

  FinalColour_1.r *= FinalColour_1.a;
  FinalColour_1.g *= FinalColour_1.a;
  FinalColour_1.b *= FinalColour_1.a;

#ifdef REG_LIGHTMAP
  // lightmapped
  vec4 lmc = texture2D(LightMap, LightmapCoordinate);
  vec4 spc = texture2D(SpecularMap, LightmapCoordinate);
  FinalColour_1.r = clamp(FinalColour_1.r*lmc.r+spc.r, 0.0, 1.0);
  FinalColour_1.g = clamp(FinalColour_1.g*lmc.g+spc.g, 0.0, 1.0);
  FinalColour_1.b = clamp(FinalColour_1.b*lmc.b+spc.b, 0.0, 1.0);
#else
  // normal
  FinalColour_1.r = clamp((FinalColour_1.r*Light.r)*Light.a, 0.0, 1.0);
  FinalColour_1.g = clamp((FinalColour_1.g*Light.g)*Light.a, 0.0, 1.0);
  FinalColour_1.b = clamp((FinalColour_1.b*Light.b)*Light.a, 0.0, 1.0);
#endif

  $include "common_fog.fs"

  gl_FragColor = FinalColour_1;
}
