#version 120

uniform sampler2D Texture;
uniform vec4 Light;
uniform bool FogEnabled;
uniform int FogType;
uniform vec4 FogColour;
uniform float FogDensity;
uniform float FogStart;
uniform float FogEnd;
uniform float AlphaRef;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate)*Light;
  if (TexColour.a < AlphaRef) discard;

  vec4 FinalColour_1 = TexColour;
  $include "common_fog.fs"

  gl_FragColor = FinalColour_1;
}
