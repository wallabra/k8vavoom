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

varying vec2 TextureCoordinate;
varying vec2 LightmapCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate)*texture2D(LightMap, LightmapCoordinate)+texture2D(SpecularMap, LightmapCoordinate);
  if (TexColour.a < 0.01) discard;

  vec4 FinalColour_1 = TexColour;
  $include "common_fog.fs"

  gl_FragColor = FinalColour_1;
}
