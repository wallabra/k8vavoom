#version 120

// fragment shader for lightmapped surfaces

uniform sampler2D Texture;
uniform sampler2D LightMap;
uniform sampler2D SpecularMap;
uniform bool FogEnabled;
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;

$include "common/texlmap_vars.fs"


void main () {
  //vec4 TexColour = texture2D(Texture, TextureCoordinate)*texture2D(LightMap, LightmapCoordinate)+texture2D(SpecularMap, LightmapCoordinate);
  //if (TexColour.a < 0.01) discard;

  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.a < 0.01) discard; // for steamlined masked textures
  TexColour *= texture2D(LightMap, LightmapCoordinate);
  TexColour += texture2D(SpecularMap, LightmapCoordinate);

  vec4 FinalColour_1 = TexColour;
  $include "common/fog.fs"

  gl_FragColor = FinalColour_1;
}
